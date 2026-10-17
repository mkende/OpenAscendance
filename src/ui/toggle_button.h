// The toggle button (`gwindow.cpp`, class `ToggleButton`, `windows.txt`
// TYPE 25, ctor 0x2cb84, vtable 0x85c88): a shape button with an on/off
// state (`+0xab`, BUTTONSTATE), drawn through the focus remap with entry
// 0xf2 patched while on (TOGLANES, NEXTTURNCONT, TOGGRID...).
#pragma once

#include "ui/gwindow.h"

namespace oa {

class ToggleButton final : public GWindow {
 public:
  // The focus remap's entry 0xf2 while on: normal and hot.
  static constexpr std::uint8_t kOnColor = 0x50;
  static constexpr std::uint8_t kOnHotColor = 0xca;

  ToggleButton(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes)
      : GWindow(wm, record, shapes), on_(record.button_state) {}

  bool on() const { return on_; }
  void set_state(bool on) { on_ = on; }

  // `ToggleButton_handle_event` (0x2cbf0): a click inside flips the state
  // and redraws the hot look before the base handling.
  bool handle_event(int type, int a, int b) override;
  // `ToggleButton_draw` (0x2cc48) / `ToggleButton_draw_hilite` (0x2cebc).
  void draw(int mode) override;
  void draw_hilite(int mode) override;

 private:
  void draw_shape(std::uint8_t on_color, bool remap);
  void draw_caption(int x, int y, int flags, std::uint8_t background);

  bool on_;
};

}  // namespace oa
