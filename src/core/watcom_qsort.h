// The Watcom C library `qsort` (0x72366, helpers med3 0x7230e and swap
// 0x722e8), ported exactly: the game sorts with comparators that tie (ship
// lists, draw lists, the intro script's events), so only the original
// algorithm reproduces its order. It is not stable.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <type_traits>

namespace oa {

// Compares two elements: negative, 0 or positive (`cmp(a, b)`).
using QsortCompare = std::function<int(const std::uint8_t* a, const std::uint8_t* b)>;

// Sorts the `n` elements of `size` bytes at `base` with `cmp`, as the Watcom
// `qsort` does: Bentley-McIlroy three-way quicksort (median of three from 30
// elements, ninther from 43), a gap-3-then-1 insertion pass below 16, and an
// explicit stack. Elements of 4 bytes are partitioned against a copy of the
// pivot, other sizes against the element moved to `base`; the original also
// takes the second way when its array's address is not a multiple of 4
// (`aligned` false; the tie order differs).
void watcom_qsort(std::uint8_t* base, std::size_t n, std::size_t size, const QsortCompare& cmp, bool aligned = true);

// Sorts `items` with `cmp(const T&, const T&)` through `watcom_qsort`
// (`aligned`: as above).
template <typename T, typename Cmp>
  requires std::is_trivially_copyable_v<T>
void watcom_qsort(std::span<T> items, Cmp cmp, bool aligned = true) {
  watcom_qsort(
      reinterpret_cast<std::uint8_t*>(items.data()), items.size(), sizeof(T),
      [&cmp](const std::uint8_t* a, const std::uint8_t* b) {
        T x;
        T y;
        std::memcpy(&x, a, sizeof(T));
        std::memcpy(&y, b, sizeof(T));
        return static_cast<int>(cmp(x, y));
      },
      aligned);
}

}  // namespace oa
