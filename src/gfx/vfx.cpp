#include "gfx/vfx.h"

#include <algorithm>
#include <cstdlib>
#include <format>

#include "core/byte_reader.h"

namespace oa {

namespace {

// The clip box of `pane`: the pane intersected with the window.
struct ClipBox {
  int x0;
  int y0;
  int x1;
  int y1;

  bool contains(int x, int y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

ClipBox clip_box(const Image8& window, const Pane& pane) {
  return {std::max(pane.x0, 0), std::max(pane.y0, 0), std::min(pane.x1, window.width() - 1),
          std::min(pane.y1, window.height() - 1)};
}

// Draws the RLE rows of `shape` at window position (ox, oy) of its box's
// top-left corner, each pixel through `put(x, y, v)` when inside `clip`.
template <typename Put>
void draw_rle(const Shape& shape, int ox, int oy, const ClipBox& clip, Put put) {
  if (shape.empty()) {
    return;
  }
  const int h = shape.height();
  // Rows entirely outside the clip box still have to be walked.
  ByteReader r(shape.rows, "shape rows");
  for (int y = 0; y < h; ++y) {
    const int wy = oy + y;
    int x = ox;
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
      if ((b & 1) != 0) {
        for (int i = 0; i < n; ++i, ++x) {
          const std::uint8_t v = r.u8();
          if (clip.contains(x, wy)) {
            put(x, wy, v);
          }
        }
      } else {
        const std::uint8_t v = r.u8();
        for (int i = 0; i < n; ++i, ++x) {
          if (clip.contains(x, wy)) {
            put(x, wy, v);
          }
        }
      }
    }
    if (wy > clip.y1) {
      return;
    }
  }
}

}  // namespace

void vfx_pane_wipe(Image8& window, const Pane& pane, std::uint8_t color) {
  const ClipBox c = clip_box(window, pane);
  for (int y = c.y0; y <= c.y1; ++y) {
    for (int x = c.x0; x <= c.x1; ++x) {
      window.set(x, y, color);
    }
  }
}

void vfx_line_draw(Image8& window, const Pane& pane, int x0, int y0, int x1, int y1, std::uint8_t color) {
  const ClipBox c = clip_box(window, pane);
  x0 += pane.x0;
  x1 += pane.x0;
  y0 += pane.y0;
  y1 += pane.y0;
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (c.contains(x0, y0)) {
      window.set(x0, y0, color);
    }
    if (x0 == x1 && y0 == y1) {
      return;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void vfx_shape_draw(Image8& window, const Pane& pane, const Shape& shape, int x, int y) {
  draw_rle(shape, x + pane.x0 + shape.x0, y + pane.y0 + shape.y0, clip_box(window, pane),
           [&](int px, int py, std::uint8_t v) { window.set(px, py, v); });
}

void vfx_shape_translate_draw(Image8& window, const Pane& pane, const Shape& shape, int x, int y,
                              const HazeTable& table) {
  draw_rle(shape, x + pane.x0 + shape.x0, y + pane.y0 + shape.y0, clip_box(window, pane),
           [&](int px, int py, std::uint8_t v) { window.set(px, py, table[v]); });
}

int vfx_character_draw(Image8& window, const Pane& pane, int x, int y, const Font& font, std::uint8_t c,
                       const HazeTable& table) {
  const int w = font.char_width(c);
  if (w == 0) {
    return 0;
  }
  const ClipBox clip = clip_box(window, pane);
  const std::vector<std::uint8_t>& glyph = font.glyphs[c];
  const int ox = x + pane.x0;
  const int oy = y + pane.y0;
  for (int gy = 0; gy < font.height; ++gy) {
    for (int gx = 0; gx < w; ++gx) {
      const std::uint8_t v = table[glyph[static_cast<std::size_t>(gy * w + gx)]];
      if (v != 0xff && clip.contains(ox + gx, oy + gy)) {
        window.set(ox + gx, oy + gy, v);
      }
    }
  }
  return w;
}

int vfx_string_draw(Image8& window, const Pane& pane, int x, int y, const Font& font, std::string_view text,
                    const HazeTable& table) {
  const int start = x;
  for (const char ch : text) {
    x += vfx_character_draw(window, pane, x, y, font, static_cast<std::uint8_t>(ch), table);
  }
  return x - start;
}

void vfx_shape_draw_half(Image8& window, const Pane& pane, const Shape& shape, int x, int y) {
  if (shape.empty()) {
    return;
  }
  const DecodedShape d = decode_shape(shape);
  const auto source = [](int dest) { return 2 * dest + (dest >= 0 ? 1 : 0); };
  for (int dy = d.y0 / 2 - 1; dy <= (d.y0 + d.pixels.height()) / 2; ++dy) {
    const int sy = source(dy) - d.y0;
    if (sy < 0 || sy >= d.pixels.height()) {
      continue;
    }
    for (int dx = d.x0 / 2 - 1; dx <= (d.x0 + d.pixels.width()) / 2; ++dx) {
      const int sx = source(dx) - d.x0;
      if (sx < 0 || sx >= d.pixels.width() || !d.opaque[static_cast<std::size_t>(sy * d.pixels.width() + sx)]) {
        continue;
      }
      const int px = pane.x0 + x + dx;
      const int py = pane.y0 + y + dy;
      if (px >= pane.x0 && px <= pane.x1 && py >= pane.y0 && py <= pane.y1 && window.contains(px, py)) {
        window.set(px, py, d.pixels.at(sx, sy));
      }
    }
  }
}

}  // namespace oa
