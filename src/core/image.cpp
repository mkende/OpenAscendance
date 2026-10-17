#include "core/image.h"

#include <algorithm>

namespace oa {

Palette Palette::from_vga(const VgaPalette& vga) {
  Palette p;
  const auto scale = [](std::uint8_t v) { return static_cast<std::uint8_t>((v & 0x3f) * 255 / 63); };
  for (std::size_t i = 0; i < 256; ++i) {
    p.colors[i] = {scale(vga.dac[i * 3]), scale(vga.dac[i * 3 + 1]), scale(vga.dac[i * 3 + 2])};
  }
  return p;
}

Image8::Image8(int width, int height, std::uint8_t fill)
    : width_(width),
      height_(height),
      pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {}

void Image8::fill(std::uint8_t c) { std::ranges::fill(pixels_, c); }

}  // namespace oa
