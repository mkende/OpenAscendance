#include "ui/edit_field.h"

#include "ui/winmgr.h"

namespace oa {

namespace {

// BIOS scan codes of the editing keys.
constexpr int kScanBackspace = 0x0e;
constexpr int kScanEnter = 0x1c;
constexpr int kScanLeft = 0x4b;
constexpr int kScanRight = 0x4d;
constexpr int kScanDelete = 0x53;

// The result of Enter (message 6).
constexpr int kResultEnter = 3;

bool accepted(int c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ' ' || c == '-';
}

}  // namespace

void EditField::set_buffer(std::string text, int max_length) {
  max_length_ = max_length;
  if (static_cast<int>(text.size()) > max_length) {
    text.resize(static_cast<std::size_t>(max_length));
  }
  text_ = std::move(text);
  cursor_ = static_cast<int>(text_.size());
}

bool EditField::handle_event(int type, int a, int b) {
  if (type != kEventKey) {
    return GWindow::handle_event(type, a, b);
  }
  const bool replace = replace_;
  replace_ = false;
  const int c = static_cast<char>(a);
  if (accepted(c)) {
    if (replace) {
      text_.clear();
      cursor_ = 0;
    }
    if (static_cast<int>(text_.size()) < max_length_ && wm_.large_font().string_width(text_) < kMaxWidth) {
      text_.insert(static_cast<std::size_t>(cursor_), 1, static_cast<char>(c));
      ++cursor_;
      draw(kDrawNormal);
    }
    return true;
  }
  switch (b) {
    case kScanBackspace:
      if (cursor_ > 0) {
        --cursor_;
        text_.erase(static_cast<std::size_t>(cursor_), 1);
        draw(kDrawNormal);
      }
      return true;
    case kScanEnter:
      wm_.post_message(kMessageEndModal, kResultEnter, 0);
      return true;
    case kScanLeft:
      if (cursor_ > 0) {
        --cursor_;
        draw(kDrawNormal);
      }
      return true;
    case kScanRight:
      if (cursor_ < static_cast<int>(text_.size())) {
        ++cursor_;
        draw(kDrawNormal);
      }
      return true;
    case kScanDelete:
      if (cursor_ < static_cast<int>(text_.size())) {
        text_.erase(static_cast<std::size_t>(cursor_), 1);
        draw(kDrawNormal);
      }
      return true;
    default:
      return GWindow::handle_event(type, a, b);
  }
}

void EditField::draw(int /*mode*/) {
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, pane_, kBackColor);
  draw_border(kTextColor);
  GameFont& font = wm_.large_font();
  font.pane() = pane_;
  font.draw_wrapped(screen, 2, 4, text_, 0, kTextColor, 0xff, 0);
  const int x = font.string_width(std::string_view(text_).substr(0, static_cast<std::size_t>(cursor_))) + 2;
  vfx_line_draw(screen, pane_, x, 3, x, pane_.y1 - pane_.y0 - 3, kCursorColor);
}

}  // namespace oa
