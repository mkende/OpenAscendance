// The game's digital audio (`docs/02-audio-formats.md`): everything is 8-bit
// unsigned mono PCM at 22050 Hz, uncompressed.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace oa {

inline constexpr int kSampleRate = 22050;

// Returns the samples of a `.voc` sound effect as `SoundSystem_cache_sfx`
// (0x4b550) plays it: bytes `[42, size - 1)` whatever the header (a 26-byte
// Creative header plus a 16-byte type-9 block header, then the terminator).
// Throws `DataError` naming `context` when the file is 43 bytes or shorter.
std::vector<std::uint8_t> sfx_samples(std::span<const std::uint8_t> voc, std::string_view context = "voc");

}  // namespace oa
