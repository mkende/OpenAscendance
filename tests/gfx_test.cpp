// The VFX blitters, the font drawers and the palette fades.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>

#include "assets/fnt.h"
#include "assets/shp.h"
#include "gfx/font.h"
#include "gfx/gsystem.h"
#include "gfx/vfx.h"

namespace oa {
namespace {

// A one-shape table: box (x0, y0)-(x0 + 2, y0 + 1), row 0 "run of 3 x 7",
// row 1 "skip 1, literal 8 9".
ShapeTable small_table(int x0, int y0) {
  std::vector<std::uint8_t> d{'1', '.', '1', '0', 1, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0};
  std::vector<std::uint8_t> header(0x18, 0);
  const auto put32 = [&](std::size_t at, std::int32_t v) {
    for (int k = 0; k < 4; ++k) {
      header[at + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(v) >> (8 * k));
    }
  };
  put32(8, x0);
  put32(12, y0);
  put32(16, x0 + 2);
  put32(20, y0 + 1);
  d.insert(d.end(), header.begin(), header.end());
  for (const int b : {6, 7, 0, 1, 1, 5, 8, 9, 0}) {
    d.push_back(static_cast<std::uint8_t>(b));
  }
  return ShapeTable(d);
}

TEST_CASE("vfx_shape_draw: hotspot-relative box, pane origin, clipping, transparency", "[gfx]") {
  const ShapeTable t = small_table(-1, -1);
  Image8 w(8, 8, 1);
  vfx_shape_draw(w, Pane{2, 2, 7, 7}, t.at(0), 1, 1);
  // Hotspot at (3, 3): the box starts at (2, 2).
  CHECK(w.at(2, 2) == 7);
  CHECK(w.at(4, 2) == 7);
  CHECK(w.at(2, 3) == 1);  // skipped: transparent
  CHECK(w.at(3, 3) == 8);
  CHECK(w.at(4, 3) == 9);
  // Clipped by the pane.
  Image8 c(8, 8, 1);
  vfx_shape_draw(c, Pane{3, 0, 7, 7}, t.at(0), 0, 1);
  // The box starts at (0 + 3 - 1, 1 + 0 - 1) = (2, 0); x 2 is left of the pane.
  CHECK(c.at(2, 0) == 1);
  CHECK(c.at(3, 0) == 7);
}

TEST_CASE("vfx_shape_translate_draw maps the shape's pixels through the table", "[gfx]") {
  const ShapeTable t = small_table(0, 0);
  HazeTable map{};
  for (std::size_t i = 0; i < map.size(); ++i) {
    map[i] = static_cast<std::uint8_t>(i);
  }
  map[7] = 0x40;
  Image8 w(4, 4, 7);
  vfx_shape_translate_draw(w, Pane{0, 0, 3, 3}, t.at(0), 0, 0, map);
  CHECK(w.at(0, 0) == 0x40);
  CHECK(w.at(0, 1) == 7);  // transparent: the window's 7 is not remapped
  CHECK(w.at(1, 1) == 8);
}

// A 2-pixel high font: 'A' 2 wide of 0xf3, ' ' 1 wide blank (0xff).
Font tiny_font() {
  Font f;
  f.height = 2;
  f.background = 0xff;
  f.widths.assign(128, 0);
  f.glyphs.assign(128, {});
  f.widths['A'] = 2;
  f.glyphs['A'] = {0xf3, 0xf3, 0xf3, 0xff};
  f.widths[' '] = 1;
  f.glyphs[' '] = {0xff, 0xff};
  return f;
}

TEST_CASE("font strings: no spacing, |nnn| colour escapes, centring by truncation", "[gfx]") {
  GameFont font(tiny_font());
  CHECK(font.string_width("A A") == 5);
  CHECK(font.string_width("|200|AA") == 4);
  Image8 w(20, 10, 0);
  font.pane() = Pane{0, 0, 19, 9};
  font.draw_string(w, 1, 1, "A|200|A", 0, kDefault, 0xff);
  CHECK(w.at(1, 1) == 0xf3);
  CHECK(w.at(3, 1) == 200);
  CHECK(w.at(2, 2) == 0);  // 0xff pixels are not drawn
  // Centred in a 20x10 pane (x1 - x0 = 19, y1 - y0 = 9): x = (19 - 2) / 2 = 8,
  // y = (9 - (2 + 2 + 1)) / 2 = 2.
  Image8 c(20, 10, 0);
  font.draw_wrapped(c, 0, 0, "A", kFontCenterH | kFontCenterV, kDefault, 0xff, 0);
  CHECK(c.at(8, 2) == 0xf3);
  CHECK(c.at(7, 2) == 0);
  CHECK(c.at(8, 1) == 0);
}

TEST_CASE("font_draw_wrapped breaks at the last space that fits", "[gfx]") {
  GameFont font(tiny_font());
  font.pane() = Pane{0, 0, 5, 19};
  Image8 w(6, 20, 0);
  const int h = font.draw_wrapped(w, 0, 0, "AA AA AA", 0, kDefault, 0xff, 5);
  // "AA AA" is 9 wide: each word goes on a line of its own, 4 apart.
  CHECK(h == 3 * (2 + 2));
  CHECK(w.at(0, 0) == 0xf3);
  CHECK(w.at(4, 0) == 0);
  CHECK(w.at(0, 8) == 0xf3);
}

TEST_CASE("stepped palette fades: closed form and the final step", "[gfx]") {
  GSystem gs(nullptr);
  std::vector<std::uint8_t> target(768, 0);
  target[0] = 40;
  target[3] = 10;
  gs.set_palette_entries(0, 256, target);
  std::vector<std::uint8_t> start(768, 0);
  start[3] = 63;
  gs.set_dac_range(0, 256, start);
  for (int k = 0; k <= 30; ++k) {
    gs.fade_to_palette_step(k);
  }
  // After step k a component is at most 64 - k away from its target.
  CHECK(gs.dac().dac[0] == 40 - (64 - 30));
  CHECK(gs.dac().dac[3] == 10 + (64 - 30));
  for (int k = 31; k <= 64; ++k) {
    gs.fade_to_palette_step(k);
  }
  CHECK(gs.dac().dac[0] == 40);
  CHECK(gs.dac().dac[3] == 10);
  // Step 63 stops 1 short (the FLIC's fades end there).
  gs.set_dac_range(0, 256, start);
  for (int k = 0; k <= 63; ++k) {
    gs.fade_to_palette_step(k);
  }
  CHECK(gs.dac().dac[0] == 39);
}

TEST_CASE("subtitle fades move 2 per step, toward the shadow or black", "[gfx]") {
  GSystem gs(nullptr);
  const std::vector<std::uint8_t> sub{5, 4, 0};
  gs.set_palette_entries(240, 1, sub);
  gs.set_dac_range(240, 1);
  gs.fade_in_range_step(240, 1);
  CHECK(gs.dac().dac[240 * 3] == 2);
  gs.fade_in_range_step(240, 1);
  gs.fade_in_range_step(240, 1);
  CHECK(gs.dac().dac[240 * 3] == 5);
  CHECK(gs.dac().dac[240 * 3 + 1] == 4);
  gs.fade_out_range_step(240, 1);
  CHECK(gs.dac().dac[240 * 3] == 3);
}

TEST_CASE("fade_window_colors reaches the target of the colours in use only", "[gfx]") {
  int vblanks = 0;
  GSystem gs([&] { ++vblanks; });
  gs.screen().fill(5);
  VgaPalette target;
  target.dac.fill(30);
  gs.fade_window_colors(target, 25);
  CHECK(gs.dac().dac[5 * 3] == 30);
  CHECK(gs.dac().dac[6 * 3] == 0);
  // 30 steps over 25 vertical blanks.
  CHECK(vblanks == 25);
}

TEST_CASE("vfx_line_draw steps the minor axis on the carries of a 32.32 accumulator", "[gfx]") {
  // (0, 0)-(10, 3): slope floor(3 * 2^32 / 10), the accumulator from one
  // half; at x = 5 the exact 1.5 stays 1 (the slope is rounded down).
  Image8 img(16, 8);
  vfx_line_draw(img, Pane{0, 0, 15, 7}, 0, 0, 10, 3, 1);
  const std::vector<int> ys = {0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3};
  for (int x = 0; x <= 10; ++x) {
    INFO(x);
    for (int y = 0; y < 8; ++y) {
      CHECK(img.at(x, y) == (y == ys[static_cast<std::size_t>(x)] ? 1 : 0));
    }
  }
  // Drawn from the first end: the reversed line rounds the other way.
  Image8 back(16, 8);
  vfx_line_draw(back, Pane{0, 0, 15, 7}, 10, 3, 0, 0, 1);
  CHECK(back.at(5, 2) == 1);
  CHECK(back.at(5, 1) == 0);
}

TEST_CASE("vfx_line_draw clips the ends to the pane, keeping the unclipped steps", "[gfx]") {
  // A long shallow line and a steep one, from outside the pane through it,
  // against the same lines drawn whole in a larger picture.
  for (const auto& [x0, y0, x1, y1] : {std::array{-20, -6, 29, 13}, std::array{-3, -25, 9, 40}}) {
    Image8 clipped(32, 16);
    vfx_line_draw(clipped, Pane{0, 0, 31, 15}, x0, y0, x1, y1, 1);
    Image8 whole(128, 128);
    vfx_line_draw(whole, Pane{0, 0, 127, 127}, x0 + 50, y0 + 50, x1 + 50, y1 + 50, 1);
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 32; ++x) {
        INFO(x << "," << y);
        CHECK(clipped.at(x, y) == whole.at(x + 50, y + 50));
      }
    }
  }
}

TEST_CASE("vfx_ellipse_draw: the midpoint outline of a radius-4 circle", "[gfx]") {
  Image8 img(16, 16);
  vfx_ellipse_draw(img, Pane{0, 0, 15, 15}, 8, 8, 4, 4, 1);
  std::vector<std::string> rows;
  for (int y = 4; y <= 12; ++y) {
    std::string row;
    for (int x = 4; x <= 12; ++x) {
      row += img.at(x, y) != 0 ? '#' : '.';
    }
    rows.push_back(row);
  }
  CHECK(rows == std::vector<std::string>{"...###...", ".##...##.", ".#.....#.", "#.......#", "#.......#", "#.......#",
                                         ".#.....#.", ".##...##.", "...###..."});
}

}  // namespace
}  // namespace oa
