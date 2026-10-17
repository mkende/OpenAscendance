// Miles VFX shape tables ("1.10"): the `.shp` files and the full-screen
// `.tmp` backgrounds (one-shape tables), `docs/02-graphics-formats.md`.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "core/image.h"

namespace oa {

// One shape of a table: its header and RLE rows (a view into the table's
// data). The box `x0..x1`, `y0..y1` (inclusive) is relative to the hotspot:
// pixel (x, y) of the box goes to (draw_x + x, draw_y + y).
struct Shape {
  std::uint16_t bounds_h = 0;  // original picture height - 1 (informational)
  std::uint16_t bounds_w = 0;  // original picture width - 1
  std::uint16_t origin_x = 0;  // hotspot in the original picture
  std::uint16_t origin_y = 0;
  std::int32_t x0 = 0;
  std::int32_t y0 = 0;
  std::int32_t x1 = 0;
  std::int32_t y1 = 0;
  // One RLE row per y in y0..y1: control byte `b`, `n = b >> 1`: `b = 0` end
  // of row, `b = 1` skip (next byte = transparent pixels), even `b` run of
  // `n` copies of the next byte, odd `b` literal of the next `n` bytes.
  std::span<const std::uint8_t> rows;

  int width() const { return x1 - x0 + 1; }
  int height() const { return y1 - y0 + 1; }
  // Whether the shape has no pixels: an inverted box, no row data, or the
  // box near 2^31 marking the empty frames of animations.
  bool empty() const;
};

// A VFX shape table loaded whole (as the shape cache does).
class ShapeTable {
 public:
  // Parses `data` (kept); throws `DataError` naming `context` when the header
  // or a shape header is malformed (the rows are checked when drawn or
  // decoded).
  explicit ShapeTable(std::vector<std::uint8_t> data, std::string_view context = "shape table");
  // The shapes view the data: moves keep them valid, copies would not.
  ShapeTable(const ShapeTable&) = delete;
  ShapeTable& operator=(const ShapeTable&) = delete;
  ShapeTable(ShapeTable&&) = default;
  ShapeTable& operator=(ShapeTable&&) = default;

  std::size_t size() const { return shapes_.size(); }
  // Returns shape `n`; throws `std::out_of_range` when there is none.
  const Shape& at(std::size_t n) const { return shapes_.at(n); }

 private:
  std::vector<std::uint8_t> data_;
  std::vector<Shape> shapes_;
};

// A decoded shape: its box as an image with a transparency mask.
struct DecodedShape {
  int x0 = 0;  // top-left corner relative to the hotspot
  int y0 = 0;
  Image8 pixels;
  std::vector<bool> opaque;  // row-major, per pixel
};

// Returns the pixels of `shape` (for tests and tools); throws `DataError` when
// a row overruns the box or the data.
DecodedShape decode_shape(const Shape& shape);

}  // namespace oa
