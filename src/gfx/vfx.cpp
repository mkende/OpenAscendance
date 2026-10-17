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
  if (c.x1 < c.x0 || c.y1 < c.y0) {
    return;
  }
  x0 += pane.x0;
  x1 += pane.x0;
  y0 += pane.y0;
  y1 += pane.y0;
  const int dx = x1 - x0;
  const int dy = y1 - y0;
  // The sign masks (0 or -1) of the deltas and of their product.
  const std::uint32_t sx = dx < 0 ? 0xffffffffU : 0;
  const std::uint32_t sy = dy < 0 ? 0xffffffffU : 0;
  const std::uint32_t s = sx ^ sy;
  const int adx = std::abs(dx);
  const int ady = std::abs(dy);
  const auto put_run = [&](int x, int y, int step_x, int step_y, int count) {
    for (; count > 0; --count, x += step_x, y += step_y) {
      window.set(x, y, color);
    }
  };
  if (dx == 0) {
    if (x0 < c.x0 || x0 > c.x1 || std::max(y0, y1) < c.y0 || std::min(y0, y1) > c.y1) {
      return;
    }
    const int a = std::clamp(y0, c.y0, c.y1);
    const int b = std::clamp(y1, c.y0, c.y1);
    put_run(x0, a, 0, sy != 0 ? -1 : 1, std::abs(b - a) + 1);
    return;
  }
  if (dy == 0) {
    if (y0 < c.y0 || y0 > c.y1 || std::max(x0, x1) < c.x0 || std::min(x0, x1) > c.x1) {
      return;
    }
    const int a = std::clamp(x0, c.x0, c.x1);
    const int b = std::clamp(x1, c.x0, c.x1);
    put_run(a, y0, sx != 0 ? -1 : 1, 0, std::abs(b - a) + 1);
    return;
  }
  // The minor / major slope as a 0.32 fraction (all ones for a diagonal).
  const std::uint32_t slope = adx == ady
                                  ? 0xffffffffU
                                  : static_cast<std::uint32_t>((static_cast<std::uint64_t>(std::min(adx, ady)) << 32) /
                                                               static_cast<std::uint32_t>(std::max(adx, ady)));
  // The clip roundings: the minor offset at major distance `d` (rounded to
  // nearest), the major distance where the minor offset reaches `d` (the
  // start: rounded up; the end: rounded down).
  const auto mul_round = [&](int d) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(d)) * slope + 0x80000000U) >> 32);
  };
  const auto div_start = [&](int d) {
    const std::uint64_t n = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(d - 1)) << 32) | 0x80000000U;
    return static_cast<std::uint32_t>(n / slope + (n % slope != 0 ? 1 : 0));
  };
  const auto div_end = [&](int d) {
    const std::uint64_t n = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(d)) << 32) | 0x80000000U;
    return static_cast<std::uint32_t>(n / slope - (n % slope == 0 ? 1 : 0));
  };
  // `(v ^ m) - m`: v negated when the mask is -1.
  const auto apply = [](std::uint32_t v, std::uint32_t m) { return static_cast<int>((v ^ m) - m); };
  const auto outcode = [&](int x, int y) {
    return (x < c.x0 ? 8 : 0) | (x > c.x1 ? 4 : 0) | (y < c.y0 ? 2 : 0) | (y > c.y1 ? 1 : 0);
  };
  const bool x_major = adx >= ady;
  int ax = x0;
  int ay = y0;
  int bx = x1;
  int by = y1;
  for (;;) {
    const int c0 = outcode(ax, ay);
    const int c1 = outcode(bx, by);
    if (c0 == 0 && c1 == 0) {
      break;
    }
    if ((c0 & c1) != 0) {
      return;
    }
    if ((c0 & 8) != 0) {
      ax = c.x0;
      ay = y0 + apply(x_major ? mul_round(c.x0 - x0) : div_start(c.x0 - x0), s);
    } else if ((c0 & 4) != 0) {
      ax = c.x1;
      ay = y0 + apply(x_major ? mul_round(x0 - c.x1) : div_start(x0 - c.x1), ~s);
    } else if ((c0 & 2) != 0) {
      ay = c.y0;
      ax = x0 + apply(x_major ? div_start(c.y0 - y0) : mul_round(c.y0 - y0), s);
    } else if ((c0 & 1) != 0) {
      ay = c.y1;
      ax = x0 + apply(x_major ? div_start(y0 - c.y1) : mul_round(y0 - c.y1), ~s);
    } else if ((c1 & 8) != 0) {
      bx = c.x0;
      by = y0 + apply(x_major ? mul_round(x0 - c.x0) : div_end(x0 - c.x0), ~s);
    } else if ((c1 & 4) != 0) {
      bx = c.x1;
      by = y0 + apply(x_major ? mul_round(c.x1 - x0) : div_end(c.x1 - x0), s);
    } else if ((c1 & 2) != 0) {
      by = c.y0;
      bx = x0 + apply(x_major ? div_end(y0 - c.y0) : mul_round(y0 - c.y0), ~s);
    } else {
      by = c.y1;
      bx = x0 + apply(x_major ? div_end(c.y1 - y0) : mul_round(c.y1 - y0), s);
    }
  }
  const int step_x = sx != 0 ? -1 : 1;
  const int step_y = sy != 0 ? -1 : 1;
  if (adx == ady) {
    put_run(ax, ay, step_x, step_y, std::abs(bx - ax) + 1);
    return;
  }
  // The accumulator at the (clipped) start; each carry steps the minor axis.
  const int skip = x_major ? std::abs(ax - x0) : std::abs(ay - y0);
  std::uint32_t acc = static_cast<std::uint32_t>(skip) * slope + 0x80000000U;
  int count = x_major ? std::abs(bx - ax) + 1 : std::abs(by - ay) + 1;
  int x = ax;
  int y = ay;
  for (; count > 0; --count) {
    window.set(x, y, color);
    const std::uint32_t next = acc + slope;
    const bool carry = next < acc;
    acc = next;
    if (x_major) {
      x += step_x;
      y += carry ? step_y : 0;
    } else {
      y += step_y;
      x += carry ? step_x : 0;
    }
  }
}

void vfx_pixel_write(Image8& window, const Pane& pane, int x, int y, std::uint8_t color) {
  const ClipBox c = clip_box(window, pane);
  x += pane.x0;
  y += pane.y0;
  if (c.contains(x, y)) {
    window.set(x, y, color);
  }
}

void vfx_ellipse_draw(Image8& window, const Pane& pane, int x, int y, int rx, int ry, std::uint8_t color) {
  if (rx == 0 || ry == 0) {
    vfx_line_draw(window, pane, x - rx, y - ry, x + rx, y + ry, color);
    return;
  }
  const ClipBox c = clip_box(window, pane);
  if (c.x1 < c.x0 || c.y1 < c.y0) {
    return;
  }
  const int cx = x + pane.x0;
  const int cy = y + pane.y0;
  const auto plot4 = [&](int px, int py) {
    for (const auto& [ex, ey] : {std::pair{cx + px, cy + py}, std::pair{cx + px, cy - py}, std::pair{cx - px, cy + py},
                                 std::pair{cx - px, cy - py}}) {
      if (c.contains(ex, ey)) {
        window.set(ex, ey, color);
      }
    }
  };
  // The 32-bit error terms of the original (wrapping arithmetic).
  const std::uint32_t b2 = static_cast<std::uint32_t>(ry) * static_cast<std::uint32_t>(ry);
  const std::uint32_t two_b2 = b2 << 1;
  const std::uint32_t a2 = static_cast<std::uint32_t>(rx) * static_cast<std::uint32_t>(rx);
  const std::uint32_t two_a2 = a2 << 1;
  int px = 0;
  int py = ry;
  int rows = ry;
  std::uint32_t ex = 0;
  std::uint32_t ey = two_a2 * static_cast<std::uint32_t>(ry);
  std::uint32_t p = (a2 >> 2) + b2 - a2 * static_cast<std::uint32_t>(ry);
  const auto negative = [](std::uint32_t v) { return static_cast<std::int32_t>(v) < 0; };
  while (negative(ex - ey)) {
    plot4(px, py);
    if (!negative(p)) {
      --py;
      --rows;
      ey -= two_a2;
      p -= ey;
    }
    ++px;
    ex += two_b2;
    p += ex + b2;
  }
  const std::uint32_t d = a2 - b2;
  p += static_cast<std::uint32_t>(
      static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(d) >> 1) + d - ex - ey) >> 1);
  for (;;) {
    plot4(px, py);
    if (negative(p)) {
      ++px;
      ex += two_b2;
      p += ex;
    }
    --py;
    ey -= two_a2;
    p -= ey - a2;
    if (--rows < 0) {
      return;
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
