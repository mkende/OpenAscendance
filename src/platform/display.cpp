#include "platform/display.h"

#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/log.h"

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

struct Display::Impl {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  std::vector<std::uint32_t> xrgb = std::vector<std::uint32_t>(std::size_t{kScreenWidth} * kScreenHeight);
  bool fullscreen = false;
  Scaling scaling = Scaling::kSmooth;
  // The Enter key of the last Alt+Enter, until its release.
  std::optional<SDL_Keycode> toggle_key;
};

Display::Display(std::string_view title, int scale, Scaling scaling) : impl_(std::make_unique<Impl>()) {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    sdl_fail("SDL_InitSubSystem(VIDEO)");
  }
  const std::string t(title);
  if (!SDL_CreateWindowAndRenderer(t.c_str(), kScreenWidth * scale, kScreenHeight * scale, SDL_WINDOW_RESIZABLE,
                                   &impl_->window, &impl_->renderer)) {
    sdl_fail("SDL_CreateWindowAndRenderer");
  }
  if (!SDL_SetRenderLogicalPresentation(impl_->renderer, kScreenWidth, kScreenHeight,
                                        SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
    sdl_fail("SDL_SetRenderLogicalPresentation");
  }
  SDL_SetRenderVSync(impl_->renderer, 1);
  impl_->texture = SDL_CreateTexture(impl_->renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     kScreenWidth, kScreenHeight);
  if (impl_->texture == nullptr) {
    sdl_fail("SDL_CreateTexture");
  }
  set_scaling(scaling);
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
  SDL_SetRenderDrawColor(impl_->renderer, 0, 0, 0, 255);
  SDL_RenderClear(impl_->renderer);
  SDL_RenderTexture(impl_->renderer, impl_->texture, nullptr, nullptr);
  SDL_RenderPresent(impl_->renderer);
}

bool Display::fullscreen() const { return impl_->fullscreen; }

void Display::set_fullscreen(bool fullscreen) {
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
    SDL_ConvertEventToRenderCoordinates(impl_->renderer, &e);
    InputEvent out;
    switch (e.type) {
      case SDL_EVENT_QUIT:
        out.type = EventType::kQuit;
        return out;
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
        out.type = EventType::kMouseMove;
        out.x = static_cast<int>(std::floor(e.motion.x));
        out.y = static_cast<int>(std::floor(e.motion.y));
        return out;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
      case SDL_EVENT_MOUSE_BUTTON_UP:
        out.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? EventType::kMouseDown : EventType::kMouseUp;
        out.x = static_cast<int>(std::floor(e.button.x));
        out.y = static_cast<int>(std::floor(e.button.y));
        out.button = to_button(e.button.button);
        return out;
      default:
        break;
    }
  }
  return std::nullopt;
}

}  // namespace oa
