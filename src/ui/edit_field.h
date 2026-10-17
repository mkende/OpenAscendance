// The edit field (`gwindow.cpp`, class `EditField`, ctor 0x2ebac, vtable
// 0x85c28): one line of text typed in the input box (`WinMgr_input_box`
// 0x50be4); not a `windows.txt` type.
#pragma once

#include <string>

#include "ui/gwindow.h"

namespace oa {

class EditField final : public GWindow {
 public:
  // The widest text that still takes characters, in pixels.
  static constexpr int kMaxWidth = 95;
  // The text cursor's colour.
  static constexpr std::uint8_t kCursorColor = 0x98;

  EditField(WinMgr& wm, const Pane& pane) : GWindow(wm, "EDITWND", pane) {}

  // `EditField_set_buffer` (0x2f0c4): edits `text` (at most `max_length`
  // characters), the cursor at its end.
  void set_buffer(std::string text, int max_length);
  const std::string& text() const { return text_; }
  int cursor() const { return cursor_; }

  // `EditField_handle_event` (0x2edd0): the first key typed replaces the
  // text; digits, letters, space and `-` insert while the text is narrower
  // than `kMaxWidth`; Backspace, Left, Right, Del edit; Enter posts message
  // 6 with 3.
  bool handle_event(int type, int a, int b) override;
  // `EditField_draw` (0x2ec60): the box, the text and the cursor line.
  void draw(int mode) override;
  void draw_hilite(int /*mode*/) override {}

 private:
  std::string text_;
  int max_length_ = 0;
  int cursor_ = 0;
  bool replace_ = true;  // +0xbd
};

}  // namespace oa
