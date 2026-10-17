#include "core/watcom_qsort.h"

#include <algorithm>
#include <array>

namespace oa {

namespace {

// swap 0x722e8: exchanges `size` bytes.
void swap_bytes(std::uint8_t* p, std::uint8_t* q, std::size_t size) { std::swap_ranges(p, p + size, q); }

// med3 0x7230e: the median of the elements at a, b, c.
std::uint8_t* med3(std::uint8_t* a, std::uint8_t* b, std::uint8_t* c, const QsortCompare& cmp) {
  if (cmp(a, b) > 0) {
    if (cmp(a, c) <= 0) {
      return a;
    }
    return cmp(b, c) > 0 ? b : c;
  }
  if (cmp(a, c) >= 0) {
    return a;
  }
  return cmp(b, c) > 0 ? c : b;
}

}  // namespace

void watcom_qsort(std::uint8_t* base, std::size_t n, std::size_t size, const QsortCompare& cmp) {
  const bool copy_pivot = size == 4;
  // The original's stack has 32 entries and no overflow check; the larger
  // part is always pushed, so the depth stays below log2(n).
  std::array<std::uint8_t*, 32> stack_base{};
  std::array<std::size_t, 32> stack_n{};
  std::size_t sp = 0;
  for (;;) {
    if (n > 1) {
      if (n < 16) {
        std::uint8_t* const end = base + n * size;
        // Gap 3 (only indices 0, 3, 6, 9, 12 take part), then 1.
        for (const std::size_t gap : {3 * size, size}) {
          for (std::uint8_t* p = base + gap; p < end; p += gap) {
            for (std::uint8_t* q = p; q > base; q -= gap) {
              if (cmp(q - gap, q) <= 0) {
                break;
              }
              swap_bytes(q, q - gap, size);
            }
          }
        }
      } else {
        std::uint8_t* m = base + (n >> 1) * size;
        if (n >= 30) {
          std::uint8_t* lo = base;
          std::uint8_t* hi = base + (n - 1) * size;
          if (n >= 43) {
            const std::size_t d = (n >> 3) * size;
            lo = med3(base, base + d, base + 2 * d, cmp);
            m = med3(m - d, m, m + d, cmp);
            hi = med3(hi - 2 * d, hi - d, hi, cmp);
          }
          m = med3(lo, m, hi, cmp);
        }
        std::array<std::uint8_t, 4> pivot_copy{};
        const std::uint8_t* pivot = base;
        if (copy_pivot) {
          std::copy_n(m, 4, pivot_copy.begin());
          pivot = pivot_copy.data();
        } else {
          swap_bytes(base, m, size);
        }
        std::uint8_t* pa = base;
        std::uint8_t* pb = base;
        std::uint8_t* pc = base + (n - 1) * size;
        std::uint8_t* pd = pc;
        for (;;) {
          while (pb <= pc) {
            const int r = cmp(pb, pivot);
            if (r > 0) {
              break;
            }
            if (r == 0) {
              swap_bytes(pa, pb, size);
              pa += size;
            }
            pb += size;
          }
          while (pb <= pc) {
            const int r = cmp(pc, pivot);
            if (r < 0) {
              break;
            }
            if (r == 0) {
              swap_bytes(pc, pd, size);
              pd -= size;
            }
            pc -= size;
          }
          if (pb > pc) {
            break;
          }
          swap_bytes(pb, pc, size);
          pb += size;
          pc -= size;
        }
        std::uint8_t* const end = base + n * size;
        std::size_t s = std::min(static_cast<std::size_t>(pa - base), static_cast<std::size_t>(pb - pa));
        swap_bytes(base, pb - s, s);
        s = std::min(static_cast<std::size_t>(pd - pc), static_cast<std::size_t>(end - pd) - size);
        swap_bytes(pb, end - s, s);
        const auto r1 = static_cast<std::size_t>(pb - pa);
        const auto r2 = static_cast<std::size_t>(pd - pc);
        if (r2 >= r1) {
          stack_base[sp] = end - r2;
          stack_n[sp] = r2 / size;
          ++sp;
          n = r1 / size;
          continue;
        }
        if (r1 > size) {
          stack_base[sp] = base;
          stack_n[sp] = r1 / size;
          ++sp;
          base = end - r2;
          n = r2 / size;
          continue;
        }
      }
    }
    if (sp == 0) {
      return;
    }
    --sp;
    base = stack_base[sp];
    n = stack_n[sp];
  }
}

}  // namespace oa
