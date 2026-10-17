#include "assets/sound_tables.h"

#include <format>

#include "assets/tokens.h"

namespace oa {

std::vector<std::string> parse_music_table(std::span<const std::uint8_t> data) {
  constexpr int kMaxTunes = 32;
  constexpr std::size_t kMaxName = 23;
  TokenReader r(data, "music.txt");
  const int count = r.integer();
  if (count < 0 || count > kMaxTunes) {
    r.fail(std::format("{} tunes (at most {})", count, kMaxTunes));
  }
  std::vector<std::string> names;
  for (int i = 0; i < count; ++i) {
    names.push_back(r.word());
    if (names.back().size() > kMaxName) {
      r.fail(std::format("tune name '{}' longer than {} characters", names.back(), kMaxName));
    }
  }
  return names;
}

SoundFxTable parse_soundfx_table(std::span<const std::uint8_t> data) {
  constexpr int kMaxSounds = 100;
  TokenReader r(data, "soundfx.txt");
  SoundFxTable table;
  for (SoundFxTable::CacheClass& c : table.cache_classes) {
    c.slots = r.integer();
    c.slot_bytes = r.integer();
  }
  const int count = r.integer();
  if (count < 0 || count >= kMaxSounds) {
    r.fail(std::format("{} sounds (fewer than {} allowed)", count, kMaxSounds));
  }
  for (int i = 0; i < count; ++i) {
    table.names.push_back(r.word());
  }
  return table;
}

}  // namespace oa
