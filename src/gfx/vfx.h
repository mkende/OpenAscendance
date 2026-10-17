// The Miles Design VFX 2D primitives the game draws with (`vfx`,
// 0x56760-0x5fcfb): panes, wipes, lines, RLE shapes (plain and translated)
// and bitmap characters. A pane is a rectangle of a window (here an 8-bit
// image); drawing coordinates are relative to its origin and clipped to the
// pane intersected with the window.
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "assets/fnt.h"
#include "assets/pal.h"
#include "assets/shp.h"
#include "core/image.h"

namespace oa {

// A VFX PANE: inclusive window coordinates.
struct Pane {
  int x0 = 0;
  int y0 = 0;
  int x1 = kScreenWidth - 1;
  int y1 = kScreenHeight - 1;

  bool operator==(const Pane&) const = default;
};

// `vfx_pane_wipe` (0x59de0): fills the visible part of `pane` with `color`.
void vfx_pane_wipe(Image8& window, const Pane& pane, std::uint8_t color);

// `vfx_line_draw` (0x57411): draws the line (x0, y0)-(x1, y1), pane-relative
// and inclusive, in `color`.
void vfx_line_draw(Image8& window, const Pane& pane, int x0, int y0, int x1, int y1, std::uint8_t color);

// `vfx_shape_draw` (0x57f4c): draws `shape` with its hotspot at (x, y) of
// `pane`; transparent pixels leave the window as it is. Throws `DataError`
// when its rows are malformed.
void vfx_shape_draw(Image8& window, const Pane& pane, const Shape& shape, int x, int y);

// `vfx_shape_translate_draw` (0x58436) after `vfx_shape_lookaside` (0x58417):
// the same, each pixel `v` written as `table[v]`.
void vfx_shape_translate_draw(Image8& window, const Pane& pane, const Shape& shape, int x, int y,
                              const HazeTable& table);

// `vfx_character_draw` (0x5ba83): draws character `c` of `font` at (x, y) of
// `pane`, each glyph pixel `v` through `table` (`table[v]`, not drawn when
// 0xff); returns the glyph's width (the advance).
int vfx_character_draw(Image8& window, const Pane& pane, int x, int y, const Font& font, std::uint8_t c,
                       const HazeTable& table);

// `vfx_string_draw` (0x5bc16): draws `text` from (x, y), each character
// advancing by its width (no spacing); returns the total width.
int vfx_string_draw(Image8& window, const Pane& pane, int x, int y, const Font& font, std::string_view text,
                    const HazeTable& table);

}  // namespace oa
