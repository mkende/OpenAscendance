// The original's pseudo-random generator: the Watcom C library `rand`
// (0x736ef) and `srand` (0x73713), `docs/09-simulation.md`.
#pragma once

#include <cstdint>

namespace oa {

// The Watcom `rand` sequence: a 32-bit LCG (`state * 0x41c64e6d + 0x3039`)
// returning bits 16..30 (0..0x7fff). One generator serves the whole game;
// its state is the CRT's per-thread `u32` at 0x89718, 1 before any `srand`.
class WatcomRandom {
 public:
  static constexpr int kRandMax = 0x7fff;

  WatcomRandom() = default;
  explicit WatcomRandom(std::uint32_t seed) : state_(seed) {}

  // Stores `seed` as the state, as is.
  void srand(std::uint32_t seed) { state_ = seed; }
  // Returns the generator state (which `srand` restores).
  std::uint32_t state() const { return state_; }

  // Returns the next value in [0, kRandMax].
  int rand() {
    state_ = state_ * 0x41c64e6du + 0x3039u;
    return static_cast<int>((state_ >> 16) & 0x7fffu);
  }

 private:
  std::uint32_t state_ = 1;
};

}  // namespace oa
