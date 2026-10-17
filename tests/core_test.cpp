// The Watcom C library ports: rand/srand and qsort.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <vector>

#include "core/fixed_width.h"
#include "core/watcom_qsort.h"
#include "core/watcom_random.h"

namespace oa {
namespace {

TEST_CASE("Watcom rand starts from state 1 and follows the LCG 0x41c64e6d, 0x3039", "[core]") {
  WatcomRandom r;
  // The C library's first values with the default seed 1.
  const std::array<int, 5> expected{16838, 5758, 10113, 17515, 31051};
  for (const int v : expected) {
    CHECK(r.rand() == v);
  }
}

TEST_CASE("Watcom srand stores the seed as is", "[core]") {
  WatcomRandom r;
  r.srand(0);
  // state = 0 * a + 0x3039 = 12345 -> (12345 >> 16) & 0x7fff = 0.
  CHECK(r.rand() == 0);
  CHECK(r.state() == 0x3039u);
  // Restoring a state replays the sequence.
  const int next = r.rand();
  r.srand(0x3039);
  CHECK(r.rand() == next);
}

struct Item {
  int key;
  int tag;
};

int by_key(const Item& a, const Item& b) { return a.key - b.key; }

std::vector<int> tags(const std::vector<Item>& items) {
  std::vector<int> out;
  for (const Item& i : items) {
    out.push_back(i.tag);
  }
  return out;
}

TEST_CASE("Watcom qsort below 16 elements: gap-3 pass then insertion, not stable", "[core]") {
  // Gap 3 compares elements 0 and 3 first and swaps them: the two 0 keys and
  // the two 1 keys end in the opposite order of a stable sort.
  std::vector<Item> items{{1, 0}, {0, 1}, {1, 2}, {0, 3}};
  watcom_qsort(std::span(items), by_key);
  CHECK(tags(items) == std::vector<int>{3, 1, 2, 0});
}

TEST_CASE("Watcom qsort sorts large arrays of 4-byte and larger elements", "[core]") {
  for (const int n : {16, 29, 30, 42, 43, 100, 1000}) {
    std::vector<Item> items;
    std::vector<int> keys;
    WatcomRandom r(static_cast<std::uint32_t>(n));
    for (int i = 0; i < n; ++i) {
      items.push_back({r.rand() % 10, i});
      keys.push_back(items.back().key);
    }
    std::vector<int> ints = keys;
    watcom_qsort(std::span(items), by_key);
    watcom_qsort(std::span(ints), [](int a, int b) { return a - b; });
    std::ranges::sort(keys);
    std::vector<int> sorted;
    for (const Item& i : items) {
      sorted.push_back(i.key);
    }
    CHECK(sorted == keys);
    CHECK(ints == keys);
    // Every element is still there once.
    std::vector<int> t = tags(items);
    std::ranges::sort(t);
    for (int i = 0; i < n; ++i) {
      CHECK(t[static_cast<std::size_t>(i)] == i);
    }
  }
}

TEST_CASE("fixed-width helpers wrap like the original's fields", "[core]") {
  CHECK(wrap_i8(-129) == 127);
  CHECK(wrap_i16(0x18000) == -32768);
  CHECK(wrap_u16(-1) == 0xffff);
  CHECK(wrap_i32(0x80000000LL) == INT32_MIN);
  CHECK(watcom_trunc(-2.7) == -2);
}

}  // namespace
}  // namespace oa
