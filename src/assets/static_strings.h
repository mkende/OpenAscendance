// `static.txt`, the game's string table (`static_load_strings` 0x1cd3c,
// `static_string` 0x1cea8): quoted strings by index, read until the line
// `end`.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace oa {

// Returns the strings of `static.txt` in file order: `\n` becomes a line
// break, `\x` the character `x`, a string going on past its line continues
// on the next (the line break dropped). Throws `DataError` when the file
// ends before `end` or inside a string.
std::vector<std::string> parse_static_strings(std::span<const std::uint8_t> data);

}  // namespace oa
