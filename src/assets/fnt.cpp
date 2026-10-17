#include "assets/fnt.h"

#include <format>
#include <string>

#include "core/byte_reader.h"

namespace oa {

Font parse_font(std::span<const std::uint8_t> data, std::string_view context) {
  ByteReader r(data, context);
  const std::string version = r.fixed_string(4);
  if (!version.starts_with("1.")) {
    r.fail(std::format("not a VFX font (version '{}')", version));
  }
  const std::uint32_t count = r.u32();
  const std::uint32_t height = r.u32();
  Font font;
  font.background = r.u32();
  if (count > 256 || height > 256) {
    r.fail(std::format("bad font header ({} chars, height {})", count, height));
  }
  font.height = static_cast<int>(height);
  std::vector<std::uint32_t> offsets(count);
  for (std::uint32_t& o : offsets) {
    o = r.u32();
  }
  for (const std::uint32_t off : offsets) {
    r.seek(off);
    const std::uint32_t w = r.u32();
    if (w > 1024) {
      r.fail(std::format("glyph width {}", w));
    }
    const auto pixels = r.bytes(std::size_t{w} * height);
    font.widths.push_back(static_cast<int>(w));
    font.glyphs.emplace_back(pixels.begin(), pixels.end());
  }
  return font;
}

}  // namespace oa
