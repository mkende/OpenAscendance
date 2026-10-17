// The program core (`corn.cpp`): `main` (0x1c424) from the graphics start-up
// on: the logo, the sound, the window manager, the opening menu and the main
// loop (`docs/05-subsystems.md`, "Startup and main loop";
// `docs/07-ui-framework.md`, "Main loop and timing").
//
// Left out: the DOS banner and its 2.5 s wait, the memory, CD and VESA
// checks, the developer flag files but `cosmos.txt`.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "core/install.h"
#include "platform/display.h"
#include "platform/input_event.h"

namespace oa {

struct GameOptions {
  Scaling scaling = Scaling::kSmooth;
  // Input fed one step per main-loop pass from the start.
  std::vector<InputEvent> keys;
  // Screenshot mode: runs headless (SDL dummy drivers, a virtual clock) until
  // `frames` frames were shown, saves the last one there and exits.
  std::optional<std::filesystem::path> screenshot;
  int frames = 2;
  // The writable folder of the saves, `ascend.cfg` and the Alt+G
  // screenshots (the original's current directory); empty for none.
  std::filesystem::path user_dir;
  // The seed of a `cosmos.txt` of 50 stars and 7 races; else that file is
  // read from `user_dir` when there.
  std::optional<std::int32_t> seed;
};

// Runs the game from `install` until the player quits; returns the exit
// status. Throws `DataError` when a game file is missing or malformed,
// `std::runtime_error` when SDL fails.
int run_game(const GameOptions& options, const Install& install);

}  // namespace oa
