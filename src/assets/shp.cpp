#include "assets/shp.h"

#include <algorithm>
#include <format>
#include <string>

#include "core/byte_reader.h"
#include "core/error.h"

namespace oa {

namespace {

constexpr std::size_t kShapeHeader = 0x18;
// Boxes beyond this are the empty-shape marker (x0 = 0x7fffffb9 etc.).
constexpr std::int32_t kEmptyThreshold = 0x40000000;

}  // namespace

bool Shape::empty() const { return x1 < x0 || y1 < y0 || rows.empty() || std::max(x0, y0) >= kEmptyThreshold; }

ShapeTable::ShapeTable(std::vector<std::uint8_t> data, std::string_view context) : data_(std::move(data)) {
  ByteReader r(data_, context);
  const std::string version = r.fixed_string(4);
  if (!version.starts_with("1.")) {
    r.fail(std::format("not a VFX shape table (version '{}')", version));
  }
  const std::uint32_t count = r.u32();
  std::vector<std::uint32_t> offsets(count);
  for (std::uint32_t& o : offsets) {
    o = r.u32();
    r.u32();  // palette offset, always 0
  }
  // A shape's data ends where the next one in file order starts.
  std::vector<std::uint32_t> starts = offsets;
  std::ranges::sort(starts);
  starts.push_back(static_cast<std::uint32_t>(data_.size()));
  for (const std::uint32_t off : offsets) {
    const std::uint32_t end = *std::ranges::upper_bound(starts, off);
    r.seek(off);
    Shape s;
    s.bounds_h = r.u16();
    s.bounds_w = r.u16();
    s.origin_x = r.u16();
    s.origin_y = r.u16();
    s.x0 = r.i32();
    s.y0 = r.i32();
    s.x1 = r.i32();
    s.y1 = r.i32();
    if (end > off + kShapeHeader) {
      s.rows = std::span<const std::uint8_t>(data_).subspan(off + kShapeHeader, end - off - kShapeHeader);
    }
    shapes_.push_back(s);
  }
}

DecodedShape decode_shape(const Shape& shape) {
  DecodedShape out;
  if (shape.empty()) {
    return out;
  }
  const int w = shape.width();
  const int h = shape.height();
  out.x0 = shape.x0;
  out.y0 = shape.y0;
  out.pixels = Image8(w, h);
  out.opaque.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), false);
  ByteReader r(shape.rows, "shape rows");
  for (int y = 0; y < h; ++y) {
    int x = 0;
    for (;;) {
      const std::uint8_t b = r.u8();
      const int n = b >> 1;
      if (n == 0) {
        if (b == 0) {
          break;
        }
        x += r.u8();
        continue;
      }
      if (x + n > w) {
        r.fail(std::format("row {} overruns the shape width {}", y, w));
      }
      const bool literal = (b & 1) != 0;
      const std::uint8_t run = literal ? 0 : r.u8();
      for (int i = 0; i < n; ++i, ++x) {
        out.pixels.set(x, y, literal ? r.u8() : run);
        out.opaque[static_cast<std::size_t>(y * w + x)] = true;
      }
    }
  }
  return out;
}

}  // namespace oa
