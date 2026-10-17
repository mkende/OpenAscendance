#include "ui/toggle_button.h"

#include <algorithm>

#include "ui/winmgr.h"

namespace oa {

bool ToggleButton::save_state(std::span<std::uint8_t> out) const {
  std::fill(out.begin(), out.end(), on_ ? 0xff : 0);
  return true;
}

void ToggleButton::load_state(std::span<const std::uint8_t> in) {
  on_ = std::any_of(in.begin(), in.end(), [](std::uint8_t b) { return b != 0; });
}

bool ToggleButton::handle_event(int type, int a, int b) {
  if ((type == kEventLeftClick || type == kEventRightClick) && contains(a, b)) {
    on_ = !on_;
    draw_hilite(kDrawNormal);
  }
  return GWindow::handle_event(type, a, b);
}

void ToggleButton::draw_shape(std::uint8_t on_color, bool remap) {
  const Shape* s = shape(frame_);
  if (s == nullptr) {
    return;
  }
  HazeTable& map = wm_.focus_map();
  if (on_) {
    map[kBackColor] = on_color;
    vfx_shape_translate_draw(wm_.screen(), pane_, *s, 0, 0, map);
    map[kBackColor] = kHotBackColor;
  } else if (remap) {
    vfx_shape_translate_draw(wm_.screen(), pane_, *s, 0, 0, map);
  } else {
    vfx_shape_draw(wm_.screen(), pane_, *s, 0, 0);
  }
}

void ToggleButton::draw_caption(int x, int y, int flags, std::uint8_t background) {
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, pane_, background);
  GameFont& font = wm_.large_font();
  font.pane() = pane_;
  font.draw_wrapped(screen, x, y, name_, flags, kDefault, 0xff, 0);
  if (border_) {
    draw_border(kTextColor);
  }
}

void ToggleButton::draw(int mode) {
  if (!active_) {
    return;
  }
  if ((wm_.hot_window() == id_ || mode == kDrawHot) && mouse_focus_ && mode != kDrawNotHot) {
    draw_hilite(mode);
  } else if (frame_ == -1) {
    if (text_x_ == -1 && text_y_ == -1) {
      draw_caption(0, 0, kFontCenterH | kFontCenterV, kBackColor);
    } else {
      draw_caption(text_x_, text_y_, 0, kBackColor);
    }
  } else {
    draw_shape(kOnColor, false);
  }
  if (!children_.empty()) {
    draw_children();
  }
}

void ToggleButton::draw_hilite(int /*mode*/) {
  if (frame_ == -1) {
    draw_caption(0, 0, kFontCenterH | kFontCenterV, kHotBackColor);
  } else {
    draw_shape(kOnHotColor, true);
  }
}

}  // namespace oa
