#include "platform/display.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/log.h"
#ifdef SDL_PLATFORM_ANDROID
#include "platform/android_cutout.h"
#endif

namespace oa {

namespace {

[[noreturn]] void sdl_fail(std::string_view what) {
  throw std::runtime_error(std::format("{}: {}", what, SDL_GetError()));
}

MouseButton to_button(std::uint8_t b) {
  switch (b) {
    case SDL_BUTTON_LEFT:
      return MouseButton::kLeft;
    case SDL_BUTTON_RIGHT:
      return MouseButton::kRight;
    case SDL_BUTTON_MIDDLE:
      return MouseButton::kMiddle;
    default:
      return MouseButton::kNone;
  }
}

// Whether `key` is the fullscreen toggle: Alt with either Enter key.
bool is_fullscreen_toggle(const SDL_KeyboardEvent& key) {
  return (key.key == SDLK_RETURN || key.key == SDLK_KP_ENTER) && (key.mod & SDL_KMOD_ALT) != 0;
}

}  // namespace

PictureRect letterbox(int width, int height, const std::array<int, 4>& cutout) {
  float x0 = static_cast<float>(cutout[0]);
  float y0 = static_cast<float>(cutout[1]);
  float x1 = static_cast<float>(width - cutout[2]);
  float y1 = static_cast<float>(height - cutout[3]);
  if (x1 - x0 < kScreenWidth / 4.0F || y1 - y0 < kScreenHeight / 4.0F) {
    x0 = 0;
    y0 = 0;
    x1 = static_cast<float>(width);
    y1 = static_cast<float>(height);
  }
  const float scale = std::min((x1 - x0) / kScreenWidth, (y1 - y0) / kScreenHeight);
  PictureRect r;
  r.w = kScreenWidth * scale;
  r.h = kScreenHeight * scale;
  r.x = std::floor(x0 + (x1 - x0 - r.w) / 2);
  r.y = std::floor(y0 + (y1 - y0 - r.h) / 2);
  return r;
}

struct Display::Impl {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  std::vector<std::uint32_t> xrgb = std::vector<std::uint32_t>(std::size_t{kScreenWidth} * kScreenHeight);
  bool fullscreen = false;
  Scaling scaling = Scaling::kSmooth;
  // The Enter key of the last Alt+Enter, until its release.
  std::optional<SDL_Keycode> toggle_key;
  // The picture's place in the output, in pixels.
  SDL_FRect picture{0, 0, kScreenWidth, kScreenHeight};
  // The display cutout's safe insets (left, top, right, bottom), in pixels;
  // and the frames since they were read.
  std::array<int, 4> cutout{};
  int frames_since_cutout = 0;

  // Places the picture in the output as it is now (`letterbox`).
  void place() {
    int w = 0;
    int h = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &w, &h) || w <= 0 || h <= 0) {
      return;
    }
    const PictureRect r = letterbox(w, h, cutout);
    picture = SDL_FRect{r.x, r.y, r.w, r.h};
  }

  // Shows the texture (the last frame) in its place, on black.
  void render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, &picture);
    SDL_RenderPresent(renderer);
  }

  // Reads the display cutout again (Android: the insets come after the
  // window, and change with a fold or a rotation).
  void update_cutout() {
#ifdef SDL_PLATFORM_ANDROID
    const std::array<int, 4> insets = android_cutout_insets();
    if (insets != cutout) {
      cutout = insets;
      log_info("display cutout insets {} {} {} {}", insets[0], insets[1], insets[2], insets[3]);
    }
#endif
    frames_since_cutout = 0;
    place();
  }

  // The screen pixel of output pixel (px, py), rounded down: left of and
  // above the picture are negative.
  void to_screen(float px, float py, int& x, int& y) const {
    x = static_cast<int>(std::floor((px - picture.x) * kScreenWidth / picture.w));
    y = static_cast<int>(std::floor((py - picture.y) * kScreenHeight / picture.h));
  }
  // The screen pixel of window point (wx, wy) (the mouse's coordinates).
  void window_to_screen(float wx, float wy, int& x, int& y) const {
    const float density = SDL_GetWindowPixelDensity(window);
    to_screen(wx * density, wy * density, x, y);
  }
  // The screen pixel of a finger at (fx, fy), normalized to the window.
  void finger_to_screen(float fx, float fy, int& x, int& y) const {
    int w = 0;
    int h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    to_screen(fx * static_cast<float>(w), fy * static_cast<float>(h), x, y);
  }
};

Display::Display(std::string_view title, int scale, Scaling scaling) : impl_(std::make_unique<Impl>()) {
  // Fingers are read as such (platform/touch.h): SDL makes no mouse events
  // of them, nor touches of the mouse. Android's Back reaches the program
  // as a key instead of closing the activity.
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
#ifdef SDL_PLATFORM_ANDROID
  // Always the full screen without the system bars (immersive), in
  // landscape either way up; the window follows the screen as a foldable
  // folds and unfolds.
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  flags |= SDL_WINDOW_FULLSCREEN;
  impl_->fullscreen = true;
#endif
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    sdl_fail("SDL_InitSubSystem(VIDEO)");
  }
  const std::string t(title);
  if (!SDL_CreateWindowAndRenderer(t.c_str(), kScreenWidth * scale, kScreenHeight * scale, flags, &impl_->window,
                                   &impl_->renderer)) {
    sdl_fail("SDL_CreateWindowAndRenderer");
  }
  SDL_SetRenderVSync(impl_->renderer, 1);
  impl_->texture = SDL_CreateTexture(impl_->renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     kScreenWidth, kScreenHeight);
  if (impl_->texture == nullptr) {
    sdl_fail("SDL_CreateTexture");
  }
  // Black until the first frame (the window may be shown again before).
  SDL_UpdateTexture(impl_->texture, nullptr, impl_->xrgb.data(), kScreenWidth * 4);
  set_scaling(scaling);
  impl_->update_cutout();
  SDL_HideCursor();
}

Display::~Display() {
  SDL_DestroyTexture(impl_->texture);
  SDL_DestroyRenderer(impl_->renderer);
  SDL_DestroyWindow(impl_->window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Display::present(const Image8& frame, const Palette& palette) {
  if (frame.width() != kScreenWidth || frame.height() != kScreenHeight) {
    throw std::invalid_argument("Display::present: the frame must be 640x480");
  }
  std::array<std::uint32_t, 256> lut{};
  for (std::size_t i = 0; i < lut.size(); ++i) {
    const Rgb c = palette.colors[i];
    lut[i] = (std::uint32_t{c.r} << 16) | (std::uint32_t{c.g} << 8) | c.b;
  }
  const auto px = frame.pixels();
  for (std::size_t i = 0; i < px.size(); ++i) {
    impl_->xrgb[i] = lut[px[i]];
  }
  SDL_UpdateTexture(impl_->texture, nullptr, impl_->xrgb.data(), kScreenWidth * 4);
#ifdef SDL_PLATFORM_ANDROID
  // About twice a second: the cutout's insets come late and change with
  // the window, not always with an event.
  if (++impl_->frames_since_cutout >= 30) {
    impl_->update_cutout();
  }
#endif
  impl_->place();
  impl_->render();
}

PictureRect Display::picture() const {
  const SDL_FRect& p = impl_->picture;
  return PictureRect{p.x, p.y, p.w, p.h};
}

void Display::set_window_size(int width, int height) {
  if (!SDL_SetWindowSize(impl_->window, width, height)) {
    log_warning("cannot resize the window: {}", SDL_GetError());
  }
  SDL_SyncWindow(impl_->window);
}

bool Display::fullscreen() const { return impl_->fullscreen; }

void Display::set_fullscreen(bool fullscreen) {
#ifdef SDL_PLATFORM_ANDROID
  // Always the full screen.
  fullscreen = true;
#endif
  if (!SDL_SetWindowFullscreen(impl_->window, fullscreen)) {
    log_warning("cannot change the fullscreen state: {}", SDL_GetError());
    return;
  }
  impl_->fullscreen = fullscreen;
}

Scaling Display::scaling() const { return impl_->scaling; }

void Display::set_scaling(Scaling scaling) {
  if (!SDL_SetTextureScaleMode(impl_->texture,
                               scaling == Scaling::kSharp ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR)) {
    log_warning("cannot change the scaling: {}", SDL_GetError());
    return;
  }
  impl_->scaling = scaling;
}

std::optional<InputEvent> Display::poll() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    InputEvent out;
    switch (e.type) {
      case SDL_EVENT_QUIT:
        out.type = EventType::kQuit;
        return out;
      case SDL_EVENT_WINDOW_RESIZED:
      case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
      case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      case SDL_EVENT_DISPLAY_ORIENTATION:
      case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
      case SDL_EVENT_WINDOW_EXPOSED:
        // Folded, unfolded, rotated, moved: the picture placed again and
        // shown at once (no stretched or stale frame until the next one).
        impl_->update_cutout();
        impl_->render();
        break;
      case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
      case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        // Also when the window manager changes it.
        impl_->fullscreen = e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
        break;
      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP:
        if (e.type == SDL_EVENT_KEY_DOWN && is_fullscreen_toggle(e.key)) {
          if (!e.key.repeat) {
            set_fullscreen(!impl_->fullscreen);
          }
          impl_->toggle_key = e.key.key;
          break;
        }
        if (e.type == SDL_EVENT_KEY_UP && impl_->toggle_key == e.key.key) {
          impl_->toggle_key.reset();
          break;
        }
        if (e.key.key == SDLK_AC_BACK) {
          if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            out.type = EventType::kBack;
            return out;
          }
          break;
        }
        out.type = e.type == SDL_EVENT_KEY_DOWN ? EventType::kKeyDown : EventType::kKeyUp;
        out.key = e.key.key;
        out.mods = e.key.mod;
        out.repeat = e.key.repeat;
        out.scancode = e.key.scancode;
        if (e.key.scancode != SDL_SCANCODE_UNKNOWN) {
          // The layout's own characters (platform/bios_keys.h).
          const auto level = static_cast<SDL_Keymod>(e.key.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS));
          out.character = SDL_GetKeyFromScancode(e.key.scancode, level, false);
          const char32_t altgr =
              SDL_GetKeyFromScancode(e.key.scancode, static_cast<SDL_Keymod>(level | SDL_KMOD_MODE), false);
          out.altgr_character = altgr != out.character ? altgr : 0;
        }
        return out;
      case SDL_EVENT_MOUSE_MOTION:
        if (e.motion.which == SDL_TOUCH_MOUSEID) {
          break;  // a finger: its own events below
        }
        out.type = EventType::kMouseMove;
        impl_->window_to_screen(e.motion.x, e.motion.y, out.x, out.y);
        return out;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.which == SDL_TOUCH_MOUSEID) {
          break;
        }
        out.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? EventType::kMouseDown : EventType::kMouseUp;
        impl_->window_to_screen(e.button.x, e.button.y, out.x, out.y);
        out.button = to_button(e.button.button);
        return out;
      case SDL_EVENT_MOUSE_WHEEL: {
        // Whole notches (SDL accumulates a smooth wheel's fractions), in the
        // system's direction (its "natural scrolling" kept).
        const int notches = e.wheel.integer_y;
        if (e.wheel.which == SDL_TOUCH_MOUSEID || notches == 0) {
          break;
        }
        out.type = EventType::kMouseWheel;
        impl_->window_to_screen(e.wheel.mouse_x, e.wheel.mouse_y, out.x, out.y);
        out.wheel = notches;
        return out;
      }
      case SDL_EVENT_FINGER_DOWN:
      case SDL_EVENT_FINGER_MOTION:
      case SDL_EVENT_FINGER_UP:
      case SDL_EVENT_FINGER_CANCELED:
        out.type = e.type == SDL_EVENT_FINGER_DOWN     ? EventType::kFingerDown
                   : e.type == SDL_EVENT_FINGER_MOTION ? EventType::kFingerMove
                   : e.type == SDL_EVENT_FINGER_UP     ? EventType::kFingerUp
                                                       : EventType::kFingerCancel;
        impl_->finger_to_screen(e.tfinger.x, e.tfinger.y, out.x, out.y);
        out.finger = static_cast<std::uint64_t>(e.tfinger.fingerID);
        out.time_ms = e.tfinger.timestamp / 1'000'000;
        return out;
      default:
        break;
    }
  }
  return std::nullopt;
}

}  // namespace oa
