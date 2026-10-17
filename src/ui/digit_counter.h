// The digit counter (`gwindow.cpp`, class `DigitCounter`, `windows.txt`
// TYPE 15, ctor 0x2ea14, vtable 0x85c40): the star map's day counter
// (TURNCOUNT), five digit shapes in the player's colour.
#pragma once

#include "ui/gwindow.h"

namespace oa {

class DigitCounter final : public GWindow {
 public:
  DigitCounter(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes)
      : GWindow(wm, record, shapes), digit_frames_(record.digit_frames) {}

  // `DigitCounter_handle_event` (0x2eb98): 0xd (a new day) redraws.
  bool handle_event(int type, int a, int b) override;
  // `DigitCounter_draw` (0x2ea74): the frame DIGITFRAMES + 12, then the day
  // as `%05d`, digit `d` being frame DIGITFRAMES + d at x 2 + 31 i.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

 private:
  int digit_frames_;
};

}  // namespace oa
