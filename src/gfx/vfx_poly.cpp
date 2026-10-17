#include "gfx/vfx_poly.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace oa {

namespace {

std::int32_t wadd(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t wsub(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t imul(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
// a * b >> 16 on 64 bits (`imul; shrd 16`, no rounding).
std::int32_t mul_floor(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>((static_cast<std::int64_t>(a) * b) >> 16);
}
// (a << 16) / dy with a 16-bit `a`, as the original's x slopes.
std::int32_t x_slope(std::int32_t a, std::int32_t dy) {
  return static_cast<std::int32_t>((std::int64_t{static_cast<std::int16_t>(a)} << 16) / dy);
}

// The clip rectangle as the rasterizers compute it: the origin clamped to 0
// and the extent.
struct Clip {
  std::int32_t x0 = 0;
  std::int32_t y0 = 0;
  std::int32_t max_x = -1;  // last column, relative to x0
  std::int32_t max_y = -1;  // last row, relative to y0
};

bool make_clip(const Image8& window, const Pane& pane, Clip& clip) {
  if (window.width() <= 0 || window.height() <= 0) {
    return false;
  }
  clip.x0 = std::max(pane.x0, 0);
  clip.y0 = std::max(pane.y0, 0);
  clip.max_x = std::min(pane.x1, window.width() - 1) - clip.x0;
  clip.max_y = std::min(pane.y1, window.height() - 1) - clip.y0;
  return clip.max_x >= 0 && clip.max_y >= 0;
}

// One polygon edge being walked: from vertex `cur` toward `next`.
struct Edge {
  std::size_t cur = 0;
  std::size_t next = 0;
  std::int32_t rem = 0;  // rows left on this edge
  std::int32_t x = 0;    // 16.16
  std::int32_t dx = 0;
};

class Walker {
 public:
  Walker(std::span<const PolyPoint> v, bool left) : v_(v), left_(left) {}

  std::size_t step(std::size_t i) const {
    if (left_) {
      return i == 0 ? v_.size() - 1 : i - 1;
    }
    return i + 1 >= v_.size() ? 0 : i + 1;
  }

  // Sets up the first edge from the top vertex, skipping edges wholly above
  // row 0 and horizontal ones.
  void first(Edge& e, std::size_t top) const {
    std::size_t cur = top;
    std::size_t next = step(cur);
    while ((v_[cur].y < 0 && v_[next].y <= 0) || v_[next].y == v_[cur].y) {
      cur = next;
      next = step(cur);
    }
    load(e, cur, next, wsub(v_[next].y, v_[cur].y));
  }

  // Moves to the edge after the current one (a horizontal edge takes a row).
  void advance(Edge& e) const {
    const std::size_t cur = e.next;
    const std::size_t next = step(cur);
    const std::int32_t dy = wsub(v_[next].y, v_[cur].y);
    load(e, cur, next, dy == 0 ? 1 : dy);
  }

 private:
  void load(Edge& e, std::size_t cur, std::size_t next, std::int32_t dy) const {
    e.cur = cur;
    e.next = next;
    e.rem = dy;
    e.dx = x_slope(wsub(v_[next].x, v_[cur].x), dy);
    e.x = wadd(imul(v_[cur].x, 0x10000), 0x8000);
  }

  std::span<const PolyPoint> v_;
  bool left_;
};

// Scan-converts `v` (relative to the clip origin) and calls `span(row, xa,
// xb)` with each visible row's clipped extent.
template <typename SpanFn>
void scan_polygon(const Clip& clip, std::span<const PolyPoint> v, SpanFn span) {
  if (v.size() < 3) {
    return;
  }
  std::int32_t ymin = 0x7fff;
  std::int32_t ymax = -0x8000;
  std::size_t top = 0;
  unsigned and_code = 0xf;
  for (std::size_t i = 0; i < v.size(); ++i) {
    const PolyPoint& p = v[i];
    if (p.y <= ymin) {
      ymin = p.y;
      top = i;
    }
    if (p.y >= ymax) {
      ymax = p.y;
    }
    const unsigned code =
        (p.x < 0 ? 8u : 0u) | (clip.max_x - p.x < 0 ? 4u : 0u) | (p.y < 0 ? 2u : 0u) | (clip.max_y - p.y < 0 ? 1u : 0u);
    and_code &= code;
  }
  if (and_code != 0 || ymin == ymax) {
    return;
  }
  const Walker left_walker(v, true);
  const Walker right_walker(v, false);
  Edge left;
  Edge right;
  left_walker.first(left, top);
  right_walker.first(right, top);

  std::int32_t count = clip.max_y - ymin;
  if (ymax <= clip.max_y) {
    count += ymax - clip.max_y;
  }
  std::int32_t row = ymin;
  if (ymin < 0) {
    count += ymin;
    row = 0;
    for (Edge* e : {&left, &right}) {
      const std::int32_t y = v[e->cur].y;
      e->rem = wadd(e->rem, y);
      e->x = wadd(e->x, mul_floor(e->dx, imul(-y, 0x10000)));
    }
  }
  for (;;) {
    const Edge& a = right.x <= left.x ? right : left;
    const Edge& b = right.x <= left.x ? left : right;
    std::int32_t xa = a.x >> 16;
    std::int32_t xb = b.x >> 16;
    if (xa <= clip.max_x && xb >= 0) {
      span(row, std::max(xa, 0), std::min(xb, clip.max_x));
    }
    ++row;
    if (--count < 0) {
      return;
    }
    if (count == 0) {
      left.x = wadd(left.x, left.dx);
      right.x = wadd(right.x, right.dx);
      continue;
    }
    if (--left.rem == 0) {
      left_walker.advance(left);
    } else {
      left.x = wadd(left.x, left.dx);
    }
    if (--right.rem == 0) {
      right_walker.advance(right);
    } else {
      right.x = wadd(right.x, right.dx);
    }
  }
}

// Returns the vertices made relative to the clip origin (the pane's origin
// clamped to the window).
std::vector<PolyPoint> to_clip(const Pane& pane, const Clip& clip, std::span<const PolyPoint> v) {
  std::vector<PolyPoint> out(v.begin(), v.end());
  for (PolyPoint& p : out) {
    p.x += pane.x0 - clip.x0;
    p.y += pane.y0 - clip.y0;
  }
  return out;
}

// A corner of the textured quad of the transformed draw: screen position
// and texel.
struct Vertex {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t u = 0;
  std::int32_t v = 0;
};

struct QuadClip {
  int left;
  int top;
  int right;
  int bottom;
};

unsigned outcode(std::int64_t x, std::int64_t y, const QuadClip& c) {
  return (x < c.left ? 8u : 0u) | (x > c.right ? 4u : 0u) | (y < c.top ? 2u : 0u) | (y > c.bottom ? 1u : 0u);
}

std::int32_t start16(std::int32_t v) {
  return static_cast<std::int32_t>((static_cast<std::uint32_t>(v) << 16) + 0x8000u);
}

// One edge of the textured quad.
struct QuadEdge {
  int to = 0;
  std::int32_t remaining = 0;
  std::int32_t x = 0, u = 0, v = 0;
  std::int32_t dx = 0, du = 0, dv = 0;

  void start(const std::array<Vertex, 4>& q, int from, int next, std::int32_t dy) {
    to = next;
    remaining = dy;
    dx = x_slope(q[next].x - q[from].x, dy);
    du = x_slope(q[next].u - q[from].u, dy);
    dv = x_slope(q[next].v - q[from].v, dy);
    x = start16(q[from].x);
    u = start16(q[from].u);
    v = start16(q[from].v);
  }
  void step() {
    x += dx;
    u += du;
    v += dv;
  }
};

// The textured-quad rasterizer of `vfx_shape_transform_draw`: maps `tex`
// (0xff transparent) onto the quad `q` (v0..v3 in order), clipped.
void draw_textured_quad(Image8& dst, const QuadClip& clip, const std::array<Vertex, 4>& q, const Image8& tex) {
  std::int32_t ymin = 0x7fff;
  std::int32_t ymax = -0x8000;
  int top = 0;
  unsigned outside = 0xf;
  for (int i = 0; i < 4; ++i) {
    if (q[i].y <= ymin) {
      ymin = q[i].y;
      top = i;
    }
    if (ymax <= q[i].y) {
      ymax = q[i].y;
    }
    outside &= outcode(q[i].x, q[i].y, clip);
  }
  if (outside != 0 || ymax == ymin) {
    return;
  }
  auto first_edge = [&](QuadEdge& e, int dir) {
    int from = top;
    for (int guard = 0; guard < 4; ++guard) {
      const int next = (from + dir + 4) % 4;
      const std::int32_t dy = q[next].y - q[from].y;
      if (!((q[from].y < clip.top && q[next].y <= clip.top) || dy == 0)) {
        e.start(q, from, next, dy);
        return from;
      }
      from = next;
    }
    return -1;
  };
  QuadEdge a;
  QuadEdge b;
  const int a_from = first_edge(a, -1);
  const int b_from = first_edge(b, +1);
  if (a_from < 0 || b_from < 0) {
    return;
  }
  std::int32_t rows = clip.bottom - ymin;
  if (ymax <= clip.bottom) {
    rows += ymax - clip.bottom;
  }
  if (ymin < clip.top) {
    rows -= clip.top - ymin;
    auto advance = [&](QuadEdge& e, int from) {
      const std::int32_t k = clip.top - q[from].y;
      e.remaining -= k;
      e.x += mul_floor(e.dx, k << 16);
      e.u += mul_floor(e.du, k << 16);
      e.v += mul_floor(e.dv, k << 16);
    };
    advance(a, a_from);
    advance(b, b_from);
    ymin = clip.top;
  }
  const std::int64_t tex_w = tex.width();
  const std::int64_t tex_h = tex.height();
  for (std::int32_t y = ymin;; ++y) {
    const QuadEdge& l = b.x <= a.x ? b : a;
    const QuadEdge& r = b.x <= a.x ? a : b;
    std::int32_t xs = l.x >> 16;
    std::int32_t xe = r.x >> 16;
    if (xs <= clip.right && xe >= clip.left) {
      std::int32_t us = l.u;
      std::int32_t vs = l.v;
      std::int32_t du = 0;
      std::int32_t dv = 0;
      if (xe != xs) {
        const std::int64_t n = std::int64_t{static_cast<std::int32_t>(static_cast<std::uint32_t>(xe - xs) << 16)};
        du = static_cast<std::int32_t>((std::int64_t{r.u - l.u} << 16) / n);
        dv = static_cast<std::int32_t>((std::int64_t{r.v - l.v} << 16) / n);
        if (xs < clip.left) {
          const std::int32_t k = clip.left - xs;
          xs = clip.left;
          us += mul_floor(du, k << 16);
          vs += mul_floor(dv, k << 16);
        }
        if (xe > clip.right) {
          xe = clip.right;
        }
      }
      // The original steps a texel pointer by the integer parts plus the
      // carries of the fractions: the same as flooring u and v here.
      std::int64_t u = static_cast<std::uint32_t>(us);
      std::int64_t v = static_cast<std::uint32_t>(vs);
      auto row = dst.row(y);
      for (std::int32_t x = xs; x <= xe; ++x, u += du, v += dv) {
        const std::int64_t tu = u >> 16;
        const std::int64_t tv = v >> 16;
        // Texels outside the scratch read other memory in the original.
        if (tu < 0 || tv < 0 || tu >= tex_w || tv >= tex_h) {
          continue;
        }
        const std::uint8_t c = tex.at(static_cast<int>(tu), static_cast<int>(tv));
        if (c != 0xff) {
          row[static_cast<std::size_t>(x)] = c;
        }
      }
    }
    if (--rows < 0) {
      return;
    }
    if (rows == 0) {
      a.step();
      b.step();
      continue;
    }
    auto walk = [&](QuadEdge& e, int dir) {
      if (--e.remaining == 0) {
        const int from = e.to;
        const int next = (from + dir + 4) % 4;
        const std::int32_t dy = q[next].y - q[from].y;
        e.start(q, from, next, dy == 0 ? 1 : dy);
      } else {
        e.step();
      }
    };
    walk(a, -1);
    walk(b, +1);
  }
}

}  // namespace

void vfx_flat_polygon(Image8& window, const Pane& pane, std::span<const PolyPoint> v, std::uint8_t color) {
  Clip clip;
  if (v.empty() || !make_clip(window, pane, clip)) {
    return;
  }
  const std::vector<PolyPoint> p = to_clip(pane, clip, v);
  scan_polygon(clip, p, [&](std::int32_t row, std::int32_t xa, std::int32_t xb) {
    auto line = window.row(clip.y0 + row);
    std::fill(line.begin() + clip.x0 + xa, line.begin() + clip.x0 + xb + 1, color);
  });
}

void vfx_translate_polygon(Image8& window, const Pane& pane, std::span<const PolyPoint> v, const HazeTable& table) {
  Clip clip;
  if (v.empty() || !make_clip(window, pane, clip)) {
    return;
  }
  const std::vector<PolyPoint> p = to_clip(pane, clip, v);
  scan_polygon(clip, p, [&](std::int32_t row, std::int32_t xa, std::int32_t xb) {
    auto line = window.row(clip.y0 + row);
    for (std::int32_t x = xa; x <= xb; ++x) {
      std::uint8_t& px = line[static_cast<std::size_t>(clip.x0 + x)];
      px = table[px];
    }
  });
}

void vfx_shape_transform_draw(Image8& window, const Pane& pane, const Shape& shape, int x, int y, std::int32_t scale_x,
                              std::int32_t scale_y, const HazeTable* table) {
  if (scale_x == kScaleOne && scale_y == kScaleOne) {
    if (table != nullptr) {
      vfx_shape_translate_draw(window, pane, shape, x, y, *table);
    } else {
      vfx_shape_draw(window, pane, shape, x, y);
    }
    return;
  }
  if (shape.empty()) {
    return;
  }
  // The scratch picture: the shape (translated) on 0xff, which is
  // transparent, as are the shape's own 0xff pixels.
  const DecodedShape d = decode_shape(shape);
  Image8 scratch(d.pixels.width(), d.pixels.height(), 0xff);
  for (int sy = 0; sy < scratch.height(); ++sy) {
    for (int sx = 0; sx < scratch.width(); ++sx) {
      if (d.opaque[static_cast<std::size_t>(sy * scratch.width() + sx)]) {
        const std::uint8_t c = d.pixels.at(sx, sy);
        scratch.set(sx, sy, table != nullptr ? (*table)[c] : c);
      }
    }
  }
  if (window.width() <= 0 || window.height() <= 0) {
    return;
  }
  const QuadClip clip{std::max(pane.x0, 0), std::max(pane.y0, 0), std::min(pane.x1, window.width() - 1),
                      std::min(pane.y1, window.height() - 1)};
  if (clip.right < clip.left || clip.bottom < clip.top) {
    return;
  }
  const int w = scratch.width();
  const int h = scratch.height();
  // The corners of the box scale about the hotspot, at (-x0, -y0) in the
  // scratch: the offset in the high word times the factor, the high dword
  // of the product (+0x8000) kept.
  const auto scale_offset = [](std::int32_t offset, std::int32_t factor) {
    const std::int64_t p = static_cast<std::int64_t>(imul(offset, 0x10000)) * factor;
    return static_cast<std::int32_t>((p + 0x8000) >> 32);
  };
  const std::int32_t cx = -shape.x0;
  const std::int32_t cy = -shape.y0;
  const std::int32_t ox = x + shape.x0 + pane.x0;
  const std::int32_t oy = y + shape.y0 + pane.y0;
  const std::array<std::array<std::int32_t, 2>, 4> tex{{{0, 0}, {w - 1, 0}, {w - 1, h - 1}, {0, h - 1}}};
  std::array<Vertex, 4> quad;
  for (std::size_t i = 0; i < 4; ++i) {
    const std::int32_t px = scale_offset(tex[i][0] - cx, scale_x) + cx;
    const std::int32_t py = scale_offset(tex[i][1] - cy, scale_y) + cy;
    quad[i] = {px + ox, py + oy, tex[i][0], tex[i][1]};
  }
  draw_textured_quad(window, clip, quad, scratch);
}

ShapeBounds vfx_shape_bounds(const Shape& shape) {
  ShapeBounds b;
  if (shape.empty()) {
    return b;
  }
  b = {0x7fffffff, 0x7fffffff, static_cast<int>(0x80000000u), static_cast<int>(0x80000000u)};
  std::size_t p = 0;
  for (int y = shape.y0; y <= shape.y1; ++y) {
    int x = shape.x0;
    for (;;) {
      const std::uint8_t c = shape.rows[p++];
      const int n = c >> 1;
      if (n == 0) {
        if ((c & 1) == 0) {
          break;
        }
        x += shape.rows[p++];
        continue;
      }
      b.x0 = std::min(b.x0, x);
      x += n;
      b.x1 = std::max(b.x1, x);
      b.y0 = std::min(b.y0, y);
      b.y1 = std::max(b.y1, y);
      p += (c & 1) != 0 ? static_cast<std::size_t>(n) : 1;
    }
  }
  return b;
}

PolyPoint center_shape(const Pane& pane, const Shape& shape) {
  const ShapeBounds b = vfx_shape_bounds(shape);
  // C division: toward zero.
  const int x = ((pane.x1 - pane.x0) - (b.x1 - b.x0)) / 2;
  const int y = ((pane.y1 - pane.y0) - (b.y1 - b.y0)) / 2;
  return {x - b.x0, y - b.y0};
}

}  // namespace oa
