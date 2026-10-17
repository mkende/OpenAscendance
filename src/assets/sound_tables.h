// The sound index tables of `ASCEND00.COB` (`docs/02-audio-formats.md`):
// `music.txt` (tune ids) and `soundfx.txt` (sound ids, SFX cache classes).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace oa {

// Returns the tune file names of `music.txt` (`count` then `count` names),
// indexed by tune id, as `SoundSystem_init_music` (0x4aa6c) reads them.
// Throws `DataError` when the text is short or passes the original's limits
// (32 names of at most 23 characters).
std::vector<std::string> parse_music_table(std::span<const std::uint8_t> data);

// `soundfx.txt`.
struct SoundFxTable {
  struct CacheClass {
    int slots = 0;
    int slot_bytes = 0;
  };
  // The SFX cache classes, three `count size` pairs.
  std::array<CacheClass, 3> cache_classes{};
  // The sound names (`data\<name>.voc`), indexed by sound id.
  std::vector<std::string> names;
};

// Returns the `soundfx.txt` table; throws `DataError` when it is short or has
// 100 sounds or more.
SoundFxTable parse_soundfx_table(std::span<const std::uint8_t> data);

}  // namespace oa
