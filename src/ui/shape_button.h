// The windows of `windows.txt` TYPE 31 and 32, plain windows with their own
// draw (`WinMgr_load_windows` 0x52a80 swaps their vtable): the race-coloured
// shape button (ABANDON, vtable 0x8605c, `ShapeButton_draw` 0x2f2c4), and
// the placeholder of the screens not written yet.
#pragma once

#include "ui/gwindow.h"

namespace oa {

// TYPE 31: its shape in the player's colour on the text-window background.
class RaceColorButton : public GWindow {
 public:
  using GWindow::GWindow;

  // `ShapeButton_draw` (0x2f2c4): the pane wiped with 0x96 when hot (0xf2
  // otherwise), the shape race-coloured; never hot when greyed.
  void draw(int mode) override;
  // `ShapeButton_draw_hilite` (0x2f35c): draw(3).
  void draw_hilite(int /*mode*/) override { draw(kDrawHot); }
};

// A window of a screen class not written yet (the star map, the planet, the
// battle...): it handles the base events and draws nothing.
class PlaceholderWnd final : public GWindow {
 public:
  using GWindow::GWindow;

  void draw(int /*mode*/) override {}
  void draw_hilite(int /*mode*/) override {}
};

}  // namespace oa
