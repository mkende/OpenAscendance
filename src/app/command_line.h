// The command line of the game's executable (app/main.cpp), adapted from
// SilentHeritage's:
//
//   open_ascendance [--data <install dir> | --setup] [--sharp] [--keys <k1,k2,...>]
//                   [--screenshot <file.bmp> [--frames N] [--save-dir <dir>]] [--seed N]
//
// Without `--data` (or when it is not an install) the game files are those
// remembered by the install setup (app/install_setup.h), which runs when
// there are none or they are gone; `--setup` runs it again.
// `--sharp` scales the picture with nearest-neighbour pixels instead of the
// default bilinear smoothing.
// `--keys` feeds the given input, one step per main-loop pass: key names up,
// down, left, right, pgup, pgdn, home, end, enter, esc, tab, space,
// backspace, comma, minus, period, kpplus, kpminus, f1..f12 or a single
// character (an upper-case letter adds Shift), optionally prefixed by
// "shift+" or "alt+" (the modifier released on the next pass); "-" waits
// one pass and "-N" N passes; "click:X:Y"
// clicks the left button at (X, Y) (press and release on two passes),
// "rclick:X:Y" the right one, "shiftclick:X:Y" a left click with Shift held
// (help mode: Shift down, move, press, release, Shift up),
// "drag:X1:Y1:X2:Y2" drags with the left button, "move:X:Y" moves the mouse.
// `--screenshot` runs headless (SDL's dummy video and audio drivers, a
// virtual clock) until N frames have been shown (default 2; a frame is shown
// at each main-loop pass and at each vertical blank a fade waits for), saves
// the last one as an 8-bit BMP and exits; it reads no save of the install
// and writes nothing, unless `--save-dir` names the folder of the saves,
// `ascend.cfg` and the Alt+G screenshots.
// `--seed` starts as with a `cosmos.txt` of 50 stars, 7 races and that seed
// (the original's developer file, read from the user folder otherwise): the
// galaxies of the oracle's runs (`docs/12-oracle.md`).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "platform/display.h"
#include "platform/input_event.h"

namespace oa {

// The usage line's options, after the program's name.
inline constexpr std::string_view kUsageOptions =
    "[--data <install dir> | --setup] [--sharp] [--keys <k1,k2,...>] [--screenshot <file.bmp> [--frames N] "
    "[--save-dir <dir>]] [--seed N]";

struct CommandLine {
  std::optional<std::string> data_dir;
  bool setup = false;
  Scaling scaling = Scaling::kSmooth;
  std::vector<InputEvent> keys;
  std::optional<std::string> screenshot;
  int frames = 2;
  std::optional<std::string> save_dir;
  std::optional<std::int32_t> seed;
};

// Parses the arguments after the program name. Throws `std::runtime_error`
// with the usage or the faulty value on a bad command line.
CommandLine parse_command_line(std::span<const std::string_view> args);

}  // namespace oa
