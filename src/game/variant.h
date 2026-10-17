// Game variants (`docs/HANDOFF.md`): the rules and AI of `PATCH.EXE` v1.6.5
// (Standard) or of `ANTAG.EXE` (Antagonizer), chosen for a new game and kept
// in its saves (`game/save.h`). The list is open-ended: a later variant gets
// an enum value and an entry of `kVariants`.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace oa::game {

enum class Variant : std::uint32_t { kStandard = 0, kAntagonizer = 1 };

struct VariantInfo {
  Variant variant;
  // The name in the settings file and on the command line.
  std::string_view key;
  // The name shown to the player.
  std::string_view label;
};

// Every variant, in the order the options screen lists them.
inline constexpr std::array<VariantInfo, 2> kVariants{{
    {Variant::kStandard, "standard", "Standard"},
    {Variant::kAntagonizer, "antagonizer", "Antagonizer"},
}};

// Returns the entry of `variant` (the first one, Standard, for a value
// outside the list).
constexpr const VariantInfo& variant_info(Variant variant) {
  for (const VariantInfo& v : kVariants) {
    if (v.variant == variant) {
      return v;
    }
  }
  return kVariants.front();
}

// Returns the variant whose key is `key`, nothing for an unknown key.
constexpr std::optional<Variant> parse_variant(std::string_view key) {
  for (const VariantInfo& v : kVariants) {
    if (v.key == key) {
      return v.variant;
    }
  }
  return std::nullopt;
}

}  // namespace oa::game
