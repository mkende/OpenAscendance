#include "app/corn.h"

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>

#include <ctime>
#include <deque>
#include <memory>
#include <stdexcept>

#include "assets/cfile.h"
#include "assets/gif.h"
#include "assets/shp.h"
#include "audio/mixer.h"
#include "audio/sound_system.h"
#include "core/edition.h"
#include "core/log.h"
#include "core/watcom_random.h"
#include "gfx/gsystem.h"
#include "platform/audio_device.h"
#include "platform/clock.h"
#include "platform/input.h"
#include "platform/screenshot.h"
#include "ui/winmgr.h"

namespace oa {

namespace {

// The logo's fades (`palette_fade_window_colors` calls of `main`), in
// vertical blanks.
constexpr int kLogoFadeIn = 25;
constexpr int kLogoFadeOut = 50;

// Thrown to end a headless run once its last frame is saved.
struct ScreenshotTaken {};

// Shuts SDL down when the program ends.
struct SdlQuitGuard {
  SdlQuitGuard() = default;
  ~SdlQuitGuard() { SDL_Quit(); }
  SdlQuitGuard(const SdlQuitGuard&) = delete;
  SdlQuitGuard& operator=(const SdlQuitGuard&) = delete;
};

// The SDL host of the game: the window, the clock, the input and the sound
// output, and the software mouse pointer (the MOUSE module's).
class Host {
 public:
  Host(const GameOptions& options, const GameFiles& files)
      : options_(options),
        headless_(options.screenshot.has_value()),
        display_(edition::kName, 2, options.scaling),
        clock_(headless_),
        pending_(options.keys.begin(), options.keys.end()) {
    if (!headless_) {
      try {
        audio_ = std::make_unique<AudioDevice>(mixer_);
      } catch (const std::runtime_error& e) {
        log_warning("no sound: {}", e.what());
      }
    }
    // `GSystem_init_mouse` 0x2be64: the pointer shapes.
    pointer_ = std::make_unique<ShapeTable>(files.load("DATA\\MOUSE.SHP"), "DATA\\MOUSE.SHP");
  }

  Clock& clock() { return clock_; }
  Input& input() { return input_; }
  // The mixer when there is a sound output (headless runs have none).
  Mixer* mixer() { return headless_ || audio_ ? &mixer_ : nullptr; }
  bool quit_requested() const { return quit_; }
  void set_pointer_shown(bool shown) { pointer_shown_ = shown; }

  // Reads the system's input events, and the next scripted step when
  // `scripted` (one per main-loop pass).
  void pump(bool scripted) {
    while (const std::optional<InputEvent> e = display_.poll()) {
      if (e->type == EventType::kQuit) {
        quit_ = true;
      } else {
        input_.feed(*e);
      }
    }
    if (scripted && !pending_.empty()) {
      input_.feed(pending_.front());
      pending_.pop_front();
    }
  }

  // Shows the screen with the DAC and the pointer. In a headless run, saves
  // the frame and throws `ScreenshotTaken` once it is the last.
  void present(const GSystem& gs) {
    frame_ = gs.screen();
    if (pointer_shown_) {
      const InputState& s = input_.state();
      vfx_shape_draw(frame_, GSystem::kScreenPane, pointer_->at(0), s.x, s.y);
    }
    const Palette palette = Palette::from_vga(gs.dac());
    display_.present(frame_, palette);
    ++frames_;
    if (headless_ && frames_ >= options_.frames) {
      save_bmp(*options_.screenshot, frame_, palette);
      throw ScreenshotTaken();
    }
  }

 private:
  const GameOptions& options_;
  bool headless_;
  Display display_;
  Clock clock_;
  Input input_;
  Mixer mixer_;
  std::unique_ptr<AudioDevice> audio_;
  std::unique_ptr<ShapeTable> pointer_;
  std::deque<InputEvent> pending_;
  Image8 frame_;
  int frames_ = 0;
  bool pointer_shown_ = false;
  bool quit_ = false;
};

// `main` 0x1c424 from `GSystem_init` on.
void run(const GameOptions& options, const GameFiles& files) {
  Host host(options, files);
  std::unique_ptr<GSystem> gs_holder;
  gs_holder = std::make_unique<GSystem>([&] {
    host.present(*gs_holder);
    host.pump(false);
    host.clock().wait_vblank();
  });
  GSystem& gs = *gs_holder;

  // The logo: logo.pal as the target, the DAC black, the picture faded in.
  vfx_pane_wipe(gs.screen(), GSystem::kScreenPane, 0);
  gs.load_palette(files, "data\\logo.pal", 0, 256);
  gs.set_dac_range(0, 256);
  const Image8 logo = decode_gif(files.load("data\\logo.gif"), "data\\logo.gif");
  for (int y = 0; y < kScreenHeight; ++y) {
    for (int x = 0; x < kScreenWidth; ++x) {
      gs.screen().set(x, y, logo.contains(x, y) ? logo.at(x, y) : 0);
    }
  }
  gs.wait_vblank();
  gs.fade_window_colors(gs.shadow(), kLogoFadeIn);

  SoundSystem sound(files, host.mixer());
  WatcomRandom random;
  random.srand(static_cast<std::uint32_t>(std::time(nullptr)));
  WinMgr wm(UiServices{.files = files,
                       .gsystem = gs,
                       .sound = sound,
                       .now_us = [&] { return host.clock().micros(); },
                       .ticks = [&] { return host.clock().ticks(); },
                       .idle = [&] { host.clock().idle(); },
                       .show_pointer = [&](bool shown) { host.set_pointer_shown(shown); }});

  gs.fade_window_colors(VgaPalette{}, kLogoFadeOut);
  wm.start_pending_fade();
  vfx_pane_wipe(gs.screen(), GSystem::kScreenPane, 0);
  gs.load_palette(files, "DATA\\game.pal", 0, 256);
  gs.set_dac_range(0, 256);
  wm.set_screen(0, false);
  host.set_pointer_shown(true);

  while (wm.running()) {
    host.pump(true);
    if (host.quit_requested()) {
      wm.post_message(kMessageQuit, 0, 0);
    }
    wm.process_input(host.input().poll());
    wm.refresh_screen();
    host.present(gs);
  }
}

}  // namespace

int run_game(const GameOptions& options, const Install& install) {
  const SdlQuitGuard sdl_quit;
  if (options.screenshot) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  }
  const GameFiles files(install);
  try {
    run(options, files);
  } catch (const ScreenshotTaken&) {
    log_info("screenshot saved to {}", options.screenshot->string());
  }
  return 0;
}

}  // namespace oa
