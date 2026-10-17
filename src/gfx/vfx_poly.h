// The VFX polygon rasterizers and the scaled shape blitter: `vfx_flat_polygon`
// (0x5cc10), `vfx_translate_polygon` (0x5e1b5) and `vfx_shape_transform_draw`
// (0x58a41). The three are byte-identical to the Silent Hunter routines ported
// by SilentHeritage (`gfx_fill_polygon` 0xaf760, `gfx_remap_polygon` 0xb0d05,
// `sprite_draw_transformed` 0xab591), from which these ports come: convex
// polygons scan-converted from the top vertex with 16.16 edge steppers,
// vertices inclusive, clipped to the pane.
#pragma once

#include <cstdint>
#include <span>

#include "assets/pal.h"
#include "assets/shp.h"
#include "core/image.h"
#include "gfx/vfx.h"

namespace oa {

// A polygon vertex, pane-relative.
struct PolyPoint {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

// `vfx_flat_polygon`: fills the convex polygon `v` with `color`. A polygon
// wholly outside one side of the pane or with no height draws nothing.
void vfx_flat_polygon(Image8& window, const Pane& pane, std::span<const PolyPoint> v, std::uint8_t color);

// `vfx_translate_polygon`: replaces every pixel `p` the convex polygon `v`
// covers with `table[p]` (the haze tables' translucent fills).
void vfx_translate_polygon(Image8& window, const Pane& pane, std::span<const PolyPoint> v, const HazeTable& table);

// The 16.16 scale 1.0 of `vfx_shape_transform_draw`.
inline constexpr std::int32_t kScaleOne = 0x10000;

// `vfx_shape_transform_draw` without rotation (no caller of the screens
// rotates): draws `shape` scaled by the 16.16 factors (`scale_x`, `scale_y`)
// about its hotspot at (x, y) of `pane`, through `table` when given (after
// `vfx_shape_lookaside`). At scale 1.0 it is `vfx_shape_draw` (or the
// translated draw); otherwise the shape is first rendered into a scratch
// picture, whose corners are mapped as a textured quad.
void vfx_shape_transform_draw(Image8& window, const Pane& pane, const Shape& shape, int x, int y, std::int32_t scale_x,
                              std::int32_t scale_y, const HazeTable* table = nullptr);

// The box of a shape's opaque pixels: x1 is one past the last column, y1
// the last row (`vfx_shape_bounds` 0x595ed, relative to the hotspot).
struct ShapeBounds {
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
};

// `vfx_shape_bounds` (0x595ed) of `shape` drawn at (0, 0), unmirrored.
ShapeBounds vfx_shape_bounds(const Shape& shape);

// `GSystem_center_shape` (0x2b360): the position at which `shape` drawn in
// `pane` has its opaque box centred (half of the free extent, rounded toward
// zero, the pane's extent being `x1 - x0`).
PolyPoint center_shape(const Pane& pane, const Shape& shape);

}  // namespace oa
