// Indexed 8-bit images and palettes: the 6-bit VGA DAC values the original
// programs, and their 8-bit RGB rendering.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace oa {

// The original screen size (VESA 640x480x8).
inline constexpr int kScreenWidth = 640;
inline constexpr int kScreenHeight = 480;

struct Rgb {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  constexpr bool operator==(const Rgb&) const = default;
};

// 256 colours as 6-bit VGA DAC triplets (0..63), as the original keeps them
// (`GSystem+0x133` current DAC, `+0x433` target palette).
struct VgaPalette {
  std::array<std::uint8_t, 768> dac{};

  constexpr bool operator==(const VgaPalette&) const = default;
};

// 256 colours with 8-bit components.
struct Palette {
  std::array<Rgb, 256> colors{};

  // Returns the colours of `vga` scaled as `v * 255 / 63` (the scaling of
  // the exported PNGs, `tools/image.py`).
  static Palette from_vga(const VgaPalette& vga);
};

// An indexed-colour image, row-major, one byte per pixel.
class Image8 {
 public:
  Image8() = default;
  Image8(int width, int height, std::uint8_t fill = 0);

  int width() const { return width_; }
  int height() const { return height_; }
  std::span<std::uint8_t> pixels() { return pixels_; }
  std::span<const std::uint8_t> pixels() const { return pixels_; }
  std::span<std::uint8_t> row(int y) {
    return std::span(pixels_).subspan(index(0, y), static_cast<std::size_t>(width_));
  }
  std::span<const std::uint8_t> row(int y) const {
    return std::span(pixels_).subspan(index(0, y), static_cast<std::size_t>(width_));
  }

  std::uint8_t at(int x, int y) const { return pixels_[index(x, y)]; }
  void set(int x, int y, std::uint8_t c) { pixels_[index(x, y)] = c; }
  bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }

  void fill(std::uint8_t c);

 private:
  std::size_t index(int x, int y) const {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
  }

  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint8_t> pixels_;
};

}  // namespace oa
