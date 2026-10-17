// Helpers reproducing the original's fixed-width integer arithmetic (Watcom
// C/C++32: 32-bit `int`, two's complement, 8- and 16-bit fields in the game
// structures).
#pragma once

#include <cmath>
#include <cstdint>

namespace oa {

// Returns `v` stored into a signed 8/16-bit field (wrapping, as the original's
// `mov byte/word ptr` truncation).
constexpr std::int8_t wrap_i8(std::int64_t v) { return static_cast<std::int8_t>(static_cast<std::uint8_t>(v & 0xff)); }
constexpr std::int16_t wrap_i16(std::int64_t v) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xffff));
}
// Returns `v` stored into an unsigned 8/16-bit field.
constexpr std::uint8_t wrap_u8(std::int64_t v) { return static_cast<std::uint8_t>(v & 0xff); }
constexpr std::uint16_t wrap_u16(std::int64_t v) { return static_cast<std::uint16_t>(v & 0xffff); }
// Returns the 32-bit `int` result of `v` (wrapping arithmetic of `imul`/`add`).
constexpr std::int32_t wrap_i32(std::int64_t v) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(v & 0xffffffff));
}

// Returns the Watcom `__CHP` float-to-int conversion: truncation toward zero
// (`docs/09-simulation.md`, conventions). `v` must be in `int` range.
inline std::int32_t watcom_trunc(double v) { return static_cast<std::int32_t>(std::trunc(v)); }

}  // namespace oa
