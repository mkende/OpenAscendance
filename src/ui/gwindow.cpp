#include "ui/gwindow.h"

#include "ui/winmgr.h"

namespace oa {

GWindow::GWindow(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes)
    : wm_(wm),
      pane_{record.x0, record.y0, record.x1, record.y1},
      shapes_(shapes),
      frame_(record.shape_frame),
      name_(record.name),
      mouse_focus_(record.mouse_focus),
      message_(record.message),
      param1_(record.param1),
      param2_(record.param2),
      help_(record.help),
      sound_(record.sound),
      items_(record.items) {}

void GWindow::broadcast_to_children(int type, int a, int b) {
  for (GWindow* child : children_) {
    child->handle_event(type, a, b);
  }
}

bool GWindow::handle_event(int type, int a, int b) {
  switch (type) {
    case kEventActivate:
      active_ = true;
      shown_ = true;
      draw(kDrawNormal);
      broadcast_to_children(kEventActivate, a, b);
      return false;
    case kEventDeactivate:
      broadcast_to_children(type, a, b);
      active_ = false;
      shown_ = false;
      wm_.release_hot(id_);
      wm_.kill_timer(id_, -1);
      return false;
    case kEventLeftClick:
    case kEventRightClick: {
      if (!contains(a, b)) {
        return false;
      }
      const int msg = message_ == kParamMouseButton ? type : message_;
      const int p1 = param1_ == kParamMouseButton ? type : param1_;
      const int p2 = param2_ == kParamMouseButton ? type : param2_;
      if (sound_ != -1) {
        wm_.services().sound.play_sfx(sound_);
      }
      if (parent_ == 0) {
        wm_.post_message(msg, p1, p2);
      } else if (msg == 998) {
        wm_.post_message(p1, p2, 0);
      } else {
        wm_.send_event(parent_, msg, p1, p2);
      }
      return true;
    }
    case kEventMouseLeave:
      if (active_ && shown_ && mouse_focus_) {
        draw(kDrawNotHot);
      }
      return true;
    case kEventMouseEnter:
      if (active_ && shown_ && mouse_focus_) {
        draw(kDrawHot);
      }
      return true;
    case kEventShow:
      shown_ = true;
      active_ = true;
      return false;
    case kEventHide:
      shown_ = false;
      return false;
    case kEventOk:
    case kEventCancel:
      wm_.post_message(6, type, 0);
      return true;
    default:
      // 0xc (help click) waits for the help windows.
      return false;
  }
}

void GWindow::draw_text(std::uint8_t background) {
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, pane_, background);
  GameFont& font = wm_.large_font();
  font.pane() = pane_;
  const int flags = (text_x_ == -1 ? kFontCenterH : 0) | (text_y_ == -1 ? kFontCenterV : 0);
  font.draw_wrapped(screen, text_x_, text_y_, name_, flags, kDefault, 0xff, 0);
  if (border_) {
    const int w = pane_.x1 - pane_.x0;
    const int h = pane_.y1 - pane_.y0;
    vfx_line_draw(screen, pane_, 0, 0, w, 0, kTextColor);
    vfx_line_draw(screen, pane_, 0, 0, 0, h, kTextColor);
    vfx_line_draw(screen, pane_, w, h, 0, h, kTextColor);
    vfx_line_draw(screen, pane_, w, h, w, 0, kTextColor);
  }
}

void GWindow::draw(int mode) {
  if (!active_) {
    return;
  }
  if ((wm_.hot_window() == id_ || mode == kDrawHot) && mouse_focus_ && mode != kDrawNotHot) {
    draw_hilite(mode);
  } else if (frame_ != -1) {
    if (shapes_ != nullptr) {
      vfx_shape_draw(wm_.screen(), pane_, shapes_->at(static_cast<std::size_t>(frame_)), 0, 0);
    }
  } else if (!items_.empty()) {
    draw_items(false);
  } else {
    draw_text(kBackColor);
  }
  if (!children_.empty()) {
    draw_children();
  }
}

void GWindow::draw_hilite(int /*mode*/) {
  if (frame_ != -1) {
    if (shapes_ != nullptr) {
      vfx_shape_translate_draw(wm_.screen(), pane_, shapes_->at(static_cast<std::size_t>(frame_)), 0, 0,
                               wm_.focus_map());
    }
  } else if (!items_.empty()) {
    draw_items(true);
  } else {
    draw_text(kHotBackColor);
  }
}

void GWindow::draw_items(bool hot) {
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, pane_, hot ? kHotBackColor : kBackColor);
  for (const WindowItem& item : items_) {
    if (item.kind == 0) {
      GameFont& font = wm_.large_font();
      font.pane() = pane_;
      font.draw_wrapped(screen, item.x, item.y, item.text, item.flags, kTextColor, 0xff, 0);
    } else {
      const ShapeTable& table = wm_.shapes().get(item.text);
      vfx_shape_draw(screen, pane_, table.at(static_cast<std::size_t>(item.flags)), item.x, item.y);
    }
  }
}

void GWindow::draw_children() {
  for (GWindow* child : children_) {
    if (child->active()) {
      child->draw(kDrawNormal);
    }
  }
}

}  // namespace oa
