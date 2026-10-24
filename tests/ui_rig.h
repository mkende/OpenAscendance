// The window manager over the install as `main` sets it up, on a virtual
// clock, with scripted input: the tests of the toolkit and the screens.
#pragma once

#include <SDL3/SDL_keycode.h>

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <vector>

#include "assets/winevent.h"
#include "audio/mixer.h"
#include "audio/sound_system.h"
#include "gfx/gsystem.h"
#include "platform/clock.h"
#include "platform/input.h"
#include "screens/screens.h"
#include "ui/game_session.h"
#include "ui/winmgr.h"

namespace oa::test {

// The standard window classes with the screens'.
inline WindowFactory full_factory() {
  WindowFactory f = WindowFactory::standard();
  add_screen_windows(f);
  return f;
}

// Appends a 24-byte trace record (`assets/winevent.h`) to `trace`.
inline void append_trace_record(std::vector<std::uint8_t>& trace, std::uint32_t time, TraceEventType type,
                                std::uint32_t a, std::uint32_t b) {
  const auto put = [&](std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
      trace.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
  };
  put(time, 4);
  put(0, 4);
  put(static_cast<std::uint16_t>(type), 2);
  put(0, 2);
  put(a, 4);
  put(b, 4);
  put(0, 4);
}

struct UiRig {
  // A stand-in game core by default; `session` replaces it. `user_dir` is
  // the folder of `ascend.cfg` and the screenshots (none by default).
  explicit UiRig(const GameFiles& f, GameSession* session = nullptr, std::filesystem::path user_dir = {})
      : files(f),
        gs([this] { clock.wait_vblank(); }),
        sound(f, &mixer),
        game(session != nullptr ? *session : stub),
        wm(UiServices{.files = f,
                      .gsystem = gs,
                      .sound = sound,
                      .game = game,
                      .user_dir = std::move(user_dir),
                      .now_us = [this] { return clock.micros(); },
                      .ticks = [this] { return clock.ticks(); },
                      .idle = [this] { clock.idle(); },
                      .show_pointer = [this](bool shown) { pointer = shown; },
                      .set_pointer_frame = [this](int frame) { pointer_frame = frame; },
                      .poll_input = [this]() -> const InputState& { return next_input(); },
                      .present = [this] { ++frames; }},
           full_factory()) {
    gs.load_palette(files, "DATA\\game.pal", 0, 256);
    wm.set_screen(0, false);
  }

  // The input of the next pass: one queued event fed first.
  const InputState& next_input() {
    if (!pending.empty()) {
      input.feed(pending.front());
      pending.pop_front();
    }
    return input.poll();
  }

  // One main-loop pass with `event` queued first.
  void pass(std::optional<InputEvent> event = std::nullopt) {
    if (event) {
      pending.push_back(*event);
    }
    wm.run_pass();
  }
  // Runs passes until the queued events are used.
  void drain() {
    while (!pending.empty()) {
      wm.run_pass();
    }
  }

  void move(int x, int y) { pass(InputEvent{.type = EventType::kMouseMove, .x = x, .y = y}); }
  void click(int x, int y, MouseButton button = MouseButton::kLeft) {
    pass(InputEvent{.type = EventType::kMouseDown, .x = x, .y = y, .button = button});
    pass(InputEvent{.type = EventType::kMouseUp, .x = x, .y = y, .button = button});
  }
  // Queues a click without running the passes (for a modal loop to use).
  void queue_click(int x, int y, MouseButton button = MouseButton::kLeft) {
    pending.push_back(InputEvent{.type = EventType::kMouseDown, .x = x, .y = y, .button = button});
    pending.push_back(InputEvent{.type = EventType::kMouseUp, .x = x, .y = y, .button = button});
  }
  // Queues a help click (Shift and a click) at (x, y) without running the
  // passes.
  void queue_help_click(int x, int y) {
    pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_LSHIFT, .mods = SDL_KMOD_LSHIFT});
    pending.push_back(InputEvent{.type = EventType::kMouseMove, .x = x, .y = y});
    queue_click(x, y);
    pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = SDLK_LSHIFT, .mods = 0});
  }
  // Presses `sdl_key` with `mods`, then releases the modifiers.
  void key(std::uint32_t sdl_key, std::uint16_t mods = 0) {
    pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = sdl_key, .mods = mods});
    if (mods != 0) {
      pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = sdl_key, .mods = 0});
    }
    drain();
  }

  const GameFiles& files;
  Clock clock{true};
  Mixer mixer;
  GSystem gs;
  SoundSystem sound;
  StubGameSession stub;
  GameSession& game;
  Input input;
  std::deque<InputEvent> pending;
  WinMgr wm;
  bool pointer = true;
  int pointer_frame = 0;
  int frames = 0;
};

}  // namespace oa::test
