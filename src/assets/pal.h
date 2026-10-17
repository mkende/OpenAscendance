// Palettes (`.pal`: raw 6-bit VGA DAC triplets, no header) and haze tables
// (`.haz`: 256-byte colour remaps), `docs/02-graphics-formats.md`.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/image.h"

namespace oa {

// Copies the colours of the `.pal` file `data` into `palette` from index
// `first`, at most `count` colours (all of the file's when `count` is
// negative), as `GSystem_load_palette` (0x2b878) fills its palette shadow.
// Returns the number of colours copied. Throws `DataError` naming `context`
// when the file is not a whole number of 6-bit triplets or does not fit.
int load_palette(VgaPalette& palette, std::span<const std::uint8_t> data, int first = 0, int count = -1,
                 std::string_view context = "palette");

// A haze table: entry `i` is the palette index replacing index `i`.
using HazeTable = std::array<std::uint8_t, 256>;

// Returns the haze table in `data`; throws `DataError` naming `context` when
// it is not 256 bytes.
HazeTable parse_haze_table(std::span<const std::uint8_t> data, std::string_view context = "haze table");

}  // namespace oa
