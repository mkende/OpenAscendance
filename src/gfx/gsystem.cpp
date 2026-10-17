#include "gfx/gsystem.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>

#include "core/error.h"

namespace oa {

namespace {

// One stepped-fade move of `cur` toward `target` with window `w` (signed
// 8-bit arithmetic as in the original: 6-bit values never overflow it).
void step_toward(std::uint8_t& cur, int target, int w) {
  const int d = cur - target;
  if (d < 0) {
    if (d + w < 0) {
      ++cur;
    }
  } else if (d - w > 0) {
    --cur;
  }
}

void check_range(int first, int count) {
  if (first < 0 || count < 0 || first + count > 256) {
    throw std::out_of_range(std::format("palette range {}+{}", first, count));
  }
}

}  // namespace

GSystem::GSystem(VblankWait wait_vblank) : wait_vblank_(std::move(wait_vblank)) {}

void GSystem::wait_vblank() const {
  if (wait_vblank_) {
    wait_vblank_();
  }
}

void GSystem::load_palette(const GameFiles& files, std::string_view file, int first, int count) {
  const std::vector<std::uint8_t> data = files.load(file);
  const int n = oa::load_palette(shadow_, data, first, count, file);
  set_dac_range(first, n, std::span(shadow_.dac).subspan(static_cast<std::size_t>(first) * 3));
}

void GSystem::set_palette_entries(int first, int count, std::span<const std::uint8_t> rgb) {
  check_range(first, count);
  std::copy_n(rgb.begin(), std::min<std::size_t>(rgb.size(), static_cast<std::size_t>(count) * 3),
              shadow_.dac.begin() + first * 3);
}

void GSystem::set_dac_range(int first, int count, std::span<const std::uint8_t> rgb) {
  check_range(first, count);
  for (int i = 0; i < count; ++i) {
    const auto o = static_cast<std::size_t>(i) * 3;
    if (rgb.size() >= o + 3) {
      set_dac(first + i, rgb[o], rgb[o + 1], rgb[o + 2]);
    } else {
      set_dac(first + i, 0, 0, 0);
    }
  }
}

void GSystem::set_dac(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  const auto o = static_cast<std::size_t>(index) * 3;
  dac_.dac[o] = r & 0x3f;
  dac_.dac[o + 1] = g & 0x3f;
  dac_.dac[o + 2] = b & 0x3f;
}

void GSystem::read_dac_into_shadow(int first, int count) {
  check_range(first, count);
  std::copy_n(dac_.dac.begin() + first * 3, count * 3, shadow_.dac.begin() + first * 3);
}

void GSystem::fade_to_palette_step(int k) {
  if (static_cast<std::int8_t>(k) == 0) {
    dac_copy_ = dac_;
  }
  const int w = 64 - k;
  for (std::size_t i = 0; i < dac_copy_.dac.size(); ++i) {
    step_toward(dac_copy_.dac[i], shadow_.dac[i], w);
  }
  set_dac_range(0, 256, dac_copy_.dac);
}

void GSystem::fade_to_color_step(int k, std::span<const std::uint8_t> rgb) {
  if (static_cast<std::int8_t>(k) == 0) {
    dac_copy_ = dac_;
  }
  const std::array<int, 3> c = rgb.size() >= 3 ? std::array<int, 3>{rgb[0], rgb[1], rgb[2]} : std::array<int, 3>{};
  const int w = std::max(0, 64 - k);
  for (std::size_t i = 0; i < dac_copy_.dac.size(); ++i) {
    step_toward(dac_copy_.dac[i], c[i % 3], w);
  }
  set_dac_range(0, 256, dac_copy_.dac);
}

void GSystem::fade_in_range_step(int first, int count) {
  check_range(first, count);
  for (int i = first * 3; i < (first + count) * 3; ++i) {
    auto& v = dac_.dac[static_cast<std::size_t>(i)];
    const std::uint8_t t = shadow_.dac[static_cast<std::size_t>(i)];
    for (int n = 0; n < 2; ++n) {
      if (v < t) {
        ++v;
      }
    }
  }
}

void GSystem::fade_out_range_step(int first, int count) {
  check_range(first, count);
  for (int i = first * 3; i < (first + count) * 3; ++i) {
    auto& v = dac_.dac[static_cast<std::size_t>(i)];
    for (int n = 0; n < 2; ++n) {
      if (v > 0) {
        --v;
      }
    }
  }
}

void GSystem::fade_window_colors(const VgaPalette& target, int vblanks) {
  std::array<bool, 256> used{};
  for (const std::uint8_t p : screen_.pixels()) {
    used[p] = true;
  }
  // Per used component: its distance and direction.
  struct Component {
    std::size_t index;
    int delta;
    int dir;
    int acc;
  };
  std::vector<Component> comps;
  int m = 0;
  for (std::size_t c = 0; c < used.size(); ++c) {
    if (!used[c]) {
      continue;
    }
    for (std::size_t k = 0; k < 3; ++k) {
      const std::size_t i = c * 3 + k;
      const int d = target.dac[i] - dac_.dac[i];
      comps.push_back({i, std::abs(d), d < 0 ? -1 : 1, 0});
      m = std::max(m, std::abs(d));
    }
  }
  if (m == 0) {
    return;
  }
  // The original starts only some accumulators at M / 2 (a bug leaving the
  // others at 0 or stale values); all start at M / 2 here.
  for (Component& c : comps) {
    c.acc = m / 2;
  }
  std::uint32_t frac = 0x8000;
  const std::uint32_t inc = (static_cast<std::uint32_t>(vblanks) << 16) / static_cast<std::uint32_t>(m);
  for (int step = 0; step < m; ++step) {
    for (Component& c : comps) {
      c.acc += c.delta;
      if (c.acc >= m) {
        c.acc -= m;
        dac_.dac[c.index] = static_cast<std::uint8_t>(dac_.dac[c.index] + c.dir);
      }
    }
    frac += inc;
    while ((frac >> 16) >= 1) {
      wait_vblank();
      frac -= 0x10000;
    }
  }
}

}  // namespace oa
