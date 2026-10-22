#include "app/corn.h"

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>

#include <ctime>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include "assets/cfile.h"
#include "assets/gif.h"
#include "assets/shp.h"
#include "audio/mixer.h"
#include "audio/sound_system.h"
#include "core/edition.h"
#include "core/log.h"
#include "game/new_game.h"
#include "gfx/gsystem.h"
#include "platform/audio_device.h"
#include "platform/clock.h"
#include "platform/input.h"
#include "platform/lifecycle.h"
#include "platform/screenshot.h"
#include "platform/touch.h"
#include "screens/options_wnd.h"
#include "screens/screens.h"
#include "ui/game_core_session.h"
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

// The SDL host of the game: the window, the clock, the input (the touch
// screen as the mouse) and the sound output, and the software mouse pointer
// (the MOUSE module's), not drawn while the fingers move it.
class Host final : public DisplayControl {
 public:
  Host(const GameOptions& options, const GameFiles& files)
      : options_(options),
        headless_(options.screenshot.has_value()),
        display_(edition::kName, 2, options.settings.scaling),
        clock_(headless_),
        touch_(input_),
        pending_(options.keys.begin(), options.keys.end()) {
#ifdef SDL_PLATFORM_ANDROID
    // No pointer until a mouse moves.
    touch_pointer_ = true;
#endif
    if (!headless_) {
      if (options.settings.fullscreen) {
        display_.set_fullscreen(true);
      }
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
  // The window settings of the options screen (none in a headless run).
  DisplayControl* display_control() { return headless_ ? nullptr : this; }
  bool fullscreen() const override { return display_.fullscreen(); }
  void set_fullscreen(bool fullscreen) override { display_.set_fullscreen(fullscreen); }
  Scaling scaling() const override { return display_.scaling(); }
  void set_scaling(Scaling scaling) override { display_.set_scaling(scaling); }
  Input& input() { return input_; }
  // The mixer when there is a sound output (headless runs have none).
  Mixer* mixer() { return headless_ || audio_ ? &mixer_ : nullptr; }
  bool quit_requested() const { return quit_; }
  void set_pointer_shown(bool shown) { pointer_shown_ = shown; }
  // `GSystem_set_pointer`: the `MOUSE.SHP` frame drawn.
  void set_pointer_frame(int frame) { pointer_frame_ = frame; }
  void warp_pointer(int x, int y) { input_.warp(x, y); }
  // `TouchMapper::set_skip_query`.
  void set_skip_query(std::function<bool()> query) { touch_.set_skip_query(std::move(query)); }
  // The last frame shown, with the pointer.
  const Image8& shown_frame() const { return frame_; }

  // Reads the system's input events, and the next scripted step when
  // `scripted` (one per main-loop pass). The lifecycle events are handled
  // meanwhile (`LifecycleWatch`).
  void pump(bool scripted) {
    while (const std::optional<InputEvent> e = display_.poll()) {
      take(*e);
    }
    if (scripted && !pending_.empty()) {
      const InputEvent e = pending_.front();
      pending_.pop_front();
      if (e.type == EventType::kBackground) {
        LifecycleWatch::simulate_background();
        // Its events now (as on Android, during the poll).
        while (const std::optional<InputEvent> polled = display_.poll()) {
          take(*polled);
        }
      } else {
        take(e);
      }
    }
    touch_.update(SDL_GetTicks());
  }

  // A lifecycle event: the game kept in `resume.gam` by `save` as the
  // program goes to the background (where Android may kill it without
  // notice), runs low on memory or ends; the clock and the sound stopped
  // while in the background.
  void lifecycle(LifecycleEvent e, const std::function<void()>& save) {
    log_info("lifecycle: {}", lifecycle_event_name(e));
    switch (e) {
      case LifecycleEvent::kWillEnterBackground:
      case LifecycleEvent::kDidEnterBackground:
        if (!saved_) {
          save();
          saved_ = true;
        }
        clock_.suspend();
        if (audio_) {
          audio_->set_paused(true);
        }
        break;
      case LifecycleEvent::kLowMemory:
      case LifecycleEvent::kTerminating:
        // In the foreground the game goes on: saved again at the next
        // event.
        if (!saved_) {
          save();
        }
        break;
      case LifecycleEvent::kWillEnterForeground:
        break;
      case LifecycleEvent::kDidEnterForeground:
        clock_.resume();
        if (audio_) {
          audio_->set_paused(false);
        }
        saved_ = false;
        break;
    }
  }

  // Shows the screen with the DAC and the pointer. In a headless run, saves
  // the frame and throws `ScreenshotTaken` once it is the last.
  void present(const GSystem& gs) {
    frame_ = gs.screen();
    if (pointer_shown_ && !touch_pointer_) {
      const InputState& s = input_.state();
      vfx_shape_draw(frame_, GSystem::kScreenPane, pointer_->at(static_cast<std::size_t>(pointer_frame_)), s.x, s.y);
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
  // One input event: the quit noted, fingers through the touch mapper.
  void take(const InputEvent& e) {
    switch (e.type) {
      case EventType::kQuit:
        quit_ = true;
        break;
      case EventType::kFingerDown:
      case EventType::kFingerMove:
      case EventType::kFingerUp:
      case EventType::kFingerCancel:
        touch_pointer_ = true;
        touch_.feed(e);
        break;
      case EventType::kMouseMove:
      case EventType::kMouseDown:
      case EventType::kMouseUp:
      case EventType::kMouseWheel:
        touch_pointer_ = false;
        input_.feed(e);
        break;
      default:
        input_.feed(e);
        break;
    }
  }
  const GameOptions& options_;
  bool headless_;
  Display display_;
  Clock clock_;
  Input input_;
  TouchMapper touch_;
  Mixer mixer_;
  std::unique_ptr<AudioDevice> audio_;
  std::unique_ptr<ShapeTable> pointer_;
  std::deque<InputEvent> pending_;
  Image8 frame_;
  int frames_ = 0;
  bool pointer_shown_ = false;
  int pointer_frame_ = 0;
  bool quit_ = false;
  // Whether the last pointer input came from a finger (the pointer is then
  // not drawn).
  bool touch_pointer_ = false;
  // Whether `resume.gam` holds the game as it is (saved by a lifecycle
  // event, the program not back in the foreground since).
  bool saved_ = false;
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
  // The start-up galaxy (`srand(time(NULL))`, `cosmos.txt`), before the
  // windows. Headless runs read no save of the install.
  GameCoreSession game(files, options.user_dir, !options.screenshot.has_value());
  RemakeOptions settings = options.settings;
  game.set_new_game_variant(settings.variant);
  std::optional<game::CosmosTxt> cosmos_txt;
  if (options.seed) {
    cosmos_txt = game::CosmosTxt{.stars = 50, .races = 7, .seed = *options.seed, .species = {}};
  } else if (!options.user_dir.empty()) {
    std::error_code ec;
    if (const std::filesystem::path path = options.user_dir / "cosmos.txt"; std::filesystem::exists(path, ec)) {
      cosmos_txt = game::parse_cosmos_txt(read_file(path));
    }
  }
  // `flash.pop` in the user folder sets the developer autoplay flag
  // (0x90b8c): the AI plays the player's race too (`docs/12-oracle.md`,
  // "Autoplay runs").
  if (!options.user_dir.empty()) {
    std::error_code ec;
    game.core()->autoplay = std::filesystem::exists(options.user_dir / "flash.pop", ec);
  }
  game.start(static_cast<std::uint32_t>(std::time(nullptr)), cosmos_txt ? &*cosmos_txt : nullptr);
  std::unique_ptr<WinMgr> wm_holder;
  WindowFactory factory = WindowFactory::standard();
  add_screen_windows(factory);
  wm_holder =
      std::make_unique<WinMgr>(UiServices{.files = files,
                                          .gsystem = gs,
                                          .sound = sound,
                                          .game = game,
                                          .user_dir = options.user_dir,
                                          .now_us = [&] { return host.clock().micros(); },
                                          .ticks = [&] { return host.clock().ticks(); },
                                          .idle = [&] { host.clock().idle(); },
                                          .show_pointer = [&](bool shown) { host.set_pointer_shown(shown); },
                                          .set_pointer_frame = [&](int frame) { host.set_pointer_frame(frame); },
                                          .poll_input = [&]() -> const InputState& {
                                            host.pump(true);
                                            if (host.quit_requested()) {
                                              wm_holder->post_message(kMessageQuit, 0, 0);
                                            }
                                            return host.input().poll();
                                          },
                                          .present = [&] { host.present(gs); },
                                          .shown_frame = [&]() -> const Image8& { return host.shown_frame(); },
                                          .warp_pointer = [&](int x, int y) { host.warp_pointer(x, y); }},
                               factory);
  WinMgr& wm = *wm_holder;
  wm.setup_race_shapes();
  host.set_skip_query([&] { return wm.skippable_animation(); });
  if (!options.trace) {
    // A trace of the original clicks on the original opening menu.
    install_options(
        wm, OptionsContext{.options = &settings, .file = options.settings_file, .display = host.display_control()});
  }

  gs.fade_window_colors(VgaPalette{}, kLogoFadeOut);
  wm.start_pending_fade();
  vfx_pane_wipe(gs.screen(), GSystem::kScreenPane, 0);
  gs.load_palette(files, "DATA\\game.pal", 0, 256);
  gs.set_dac_range(0, 256);
  wm.set_screen(0, false);
  if (options.trace) {
    // main: Tutorial_start_replay of the third argument.
    wm.start_replay(read_file(*options.trace), options.trace->filename().string(), false);
  }
  host.set_pointer_shown(true);

  // The original's exit (below), also when the system sends the program to
  // the background or ends it (a remake addition): from now on, once
  // `resume.gam` was read.
  const auto save_on_exit = [&] {
    wm.save_config();
    if (!game.save_resume()) {
      log_warning("resume.gam not saved");
    }
  };
  std::optional<LifecycleWatch> watch;
  if (!options.user_dir.empty()) {
    watch.emplace([&](LifecycleEvent e) {
      // Called from SDL (C code): nothing may be thrown through it.
      try {
        host.lifecycle(e, save_on_exit);
      } catch (const std::exception& error) {
        log_error("{}: {}", lifecycle_event_name(e), error.what());
      }
    });
  }

  while (wm.running()) {
    wm.run_pass();
  }
  watch.reset();
  save_on_exit();
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
