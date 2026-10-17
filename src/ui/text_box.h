// The text box (`gwindow.cpp`, class `TextBox`, `windows.txt` TYPE 26, ctor
// 0x2f138, vtable 0x85c10): word-wrapped text on the text-window background
// (ALIENDIP, the message box).
#pragma once

#include <string>

#include "gfx/font.h"
#include "ui/gwindow.h"

namespace oa {

class TextBox final : public GWindow {
 public:
  TextBox(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) : GWindow(wm, record, shapes) {}
  TextBox(WinMgr& wm, std::string name, const Pane& pane) : GWindow(wm, std::move(name), pane) {}

  // The text (`+0xab`, none by default), its font flags (`+0xaf`, 3:
  // centred) and colour (`+0xb1`, 0xf3).
  void set_text(std::string text) { text_ = std::move(text); }
  const std::string& text() const { return text_; }
  void set_flags(int flags) { flags_ = flags; }
  void set_color(int color) { color_ = color; }

  // `TextBox_draw` (0x2f1b8): the pane wiped with 0xf2, the border, the text
  // wrapped to the width - 5.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

 private:
  std::string text_;
  int flags_ = kFontCenterH | kFontCenterV;
  int color_ = kTextColor;
};

}  // namespace oa
