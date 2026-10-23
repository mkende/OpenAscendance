#include "ui/list_box.h"

#include <cstdlib>
#include <format>

#include "core/error.h"
#include "core/watcom_qsort.h"
#include "ui/winmgr.h"

namespace oa {

ListBox::ListBox(WinMgr& wm, const WindowRecord& record, const ShapeTable* /*shapes*/)
    : GWindow(wm, record, &wm.shared_shapes(kListBoxShapes)) {
  line_height_ = wm.large_font().font().height + 3;
  draw_item_ = [this](const Pane& row, const Item& item, bool hilite) { draw_text_item(row, item, hilite); };
  set_style(1);
  // SHAPEFRAME is read over the style's frame; the activation sets it back.
  frame_ = record.shape_frame;
}

void ListBox::set_style(int style) {
  frame_ = style == 1 ? 0 : 2;
  style_ = style;
  bar_width_ = shapes_->at(static_cast<std::size_t>(frame_)).width() + 3;
  row_.x0 = pane_.x0 + 1;
  row_.x1 = pane_.x1 - 1 - (scroll_bar_ ? bar_width_ : 0);
}

void ListBox::set_colors(std::uint8_t border, std::uint8_t back, std::uint8_t bar) {
  border_color_ = border;
  back_color_ = back;
  bar_color_ = bar;
}

void ListBox::set_scroll_bar(bool on) {
  if (on != scroll_bar_) {
    row_.x1 += on ? -bar_width_ : bar_width_;
  }
  scroll_bar_ = on;
}

int ListBox::height_of(const Item& item) const { return item_height_ ? item_height_(item) : line_height_; }

int ListBox::add_item(std::string text, int height, bool redraw) {
  if (count() >= kMaxItems) {
    throw std::length_error(std::format("{}: more than {} rows", name_, kMaxItems));
  }
  content_height_ += height == -1 ? line_height_ : height;
  if (pane_.y1 - pane_.y0 - 1 < content_height_) {
    if (grow_) {
      pane_.y1 = content_height_ + pane_.y0 + 2;
    } else if (!scroll_bar_) {
      set_scroll_bar(true);
    }
  }
  items_.push_back(Item{std::move(text), 0, 0});
  if (redraw) {
    draw(kDrawNormal);
  }
  return count() - 1;
}

void ListBox::set_item_data(int index, int data) {
  if (index >= 0 && index < count()) {
    items_[static_cast<std::size_t>(index)].data = data;
  }
}

void ListBox::set_item_object(int index, int object) {
  if (index >= 0 && index < count()) {
    items_[static_cast<std::size_t>(index)].object = object;
  }
}

void ListBox::clear() {
  items_.clear();
  content_height_ = 0;
  hover_ = -1;
  top_ = 0;
  row_.x1 = pane_.x1 - 1;
  if (!keep_scroll_bar_) {
    scroll_bar_ = false;
  }
}

void ListBox::scroll_to(int row) {
  if (pane_.y1 - pane_.y0 - 1 < content_height_ && row >= 0 && row < count()) {
    hover_ = -1;
    top_ = row;
  }
}

void ListBox::sort(const Compare& compare) {
  // The original sorts its 8-byte {text, data} entries: the element size
  // decides the qsort's partitioning.
  struct Entry {
    std::int32_t index;
    std::int32_t unused;
  };
  std::vector<Entry> entries;
  for (int i = 0; i < count(); ++i) {
    entries.push_back({i, 0});
  }
  watcom_qsort(std::span<Entry>(entries), [&](const Entry& a, const Entry& b) {
    return compare(items_[static_cast<std::size_t>(a.index)], items_[static_cast<std::size_t>(b.index)]);
  });
  std::vector<Item> sorted;
  for (const Entry& e : entries) {
    sorted.push_back(std::move(items_[static_cast<std::size_t>(e.index)]));
  }
  items_ = std::move(sorted);
}

void ListBox::draw_text_item(const Pane& row, const Item& item, bool hilite) {
  GameFont& font = wm_.large_font();
  font.pane() = row;
  font.draw_wrapped(wm_.screen(), 1, 1, item.text, 0, kDefault, hilite ? kHotBackColor : kBackColor, 0);
}

void ListBox::draw_row(int index, bool hilite) {
  vfx_pane_wipe(wm_.screen(), row_, back_color_);
  draw_item_(row_, items_[static_cast<std::size_t>(index)], hilite);
}

void ListBox::draw_scroll_bar() {
  Image8& screen = wm_.screen();
  const int x = pane_.x1 - pane_.x0 - bar_width_;
  const int h = pane_.y1 - pane_.y0;
  vfx_line_draw(screen, pane_, x, 0, x, h, border_color_);
  Pane bar{pane_.x1 - bar_width_ + 1, pane_.y0 + 1, pane_.x1 - 1, pane_.y1 - 1};
  vfx_pane_wipe(screen, bar, bar_color_);
  if (arrow_ > 1) {
    const int mid = pane_.y0 + h / 2;
    if (arrow_ == 2) {
      bar.y1 = mid;
    } else {
      bar.y0 = mid;
    }
    vfx_pane_wipe(screen, bar, kHotBackColor);
  }
  vfx_shape_draw(screen, pane_, shapes_->at(static_cast<std::size_t>(frame_)), x + 2, 2);
  vfx_shape_draw(screen, pane_, shapes_->at(static_cast<std::size_t>(frame_ + 1)), x + 2, h - bar_width_ + 2);
}

void ListBox::draw(int /*mode*/) {
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, pane_, back_color_);
  const int w = pane_.x1 - pane_.x0;
  const int h = pane_.y1 - pane_.y0;
  vfx_line_draw(screen, pane_, 0, 0, w, 0, border_color_);
  vfx_line_draw(screen, pane_, 0, 0, 0, h, border_color_);
  vfx_line_draw(screen, pane_, w, h, w, 0, border_color_);
  vfx_line_draw(screen, pane_, w, h, 0, h, border_color_);
  if (scroll_bar_) {
    draw_scroll_bar();
  }
  Pane row{pane_.x0 + 1, pane_.y0 + 1, pane_.x1 - 1 - (scroll_bar_ ? bar_width_ : 0), 0};
  bool last = false;
  for (int i = top_; i < count() && !last; ++i) {
    const Item& item = items_[static_cast<std::size_t>(i)];
    row.y1 = height_of(item) + row.y0 - 1;
    if (row.y1 >= pane_.y1) {
      row.y1 = pane_.y1 - 1;
      last = true;
    }
    draw_item_(row, item, i == hover_);
    row.y0 = row.y1 + 1;
  }
}

std::optional<ListBox::RowSpan> ListBox::row_span(int y) const {
  int y0 = pane_.y0 + 1;
  bool last = false;
  for (int i = top_; i < count() && !last; ++i) {
    int y1 = height_of(items_[static_cast<std::size_t>(i)]) + y0 - 1;
    if (y1 >= pane_.y1) {
      last = true;
      y1 = pane_.y1 - 1;
    }
    if (y >= y0 && y <= y1) {
      return RowSpan{.index = i, .y0 = y0, .y1 = y1};
    }
    y0 = y1 + 1;
  }
  return std::nullopt;
}

int ListBox::row_at(int x, int y) const {
  if (!contains(x, y) || x >= pane_.x1 - (scroll_bar_ ? bar_width_ : 0)) {
    return -1;
  }
  const std::optional<RowSpan> row = row_span(y);
  return row ? row->index : -1;
}

void ListBox::track_mouse(int x, int y) {
  if (hover_ != -1) {
    if (y >= row_.y0 && y <= row_.y1 && x <= row_.x1) {
      if (redraw_hover_) {
        draw_row(hover_, true);
      }
      return;
    }
    draw_row(hover_, false);
    hover_ = -1;
  }
  const int limit = pane_.x1 - (scroll_bar_ ? bar_width_ : 0);
  if (x < limit) {
    if (arrow_ > 1) {
      arrow_ = 1;
      draw_scroll_bar();
      wm_.cancel_notify(id_, kEventScrollStop);
      wm_.kill_timer(id_, -1);
    }
    if (const std::optional<RowSpan> row = row_span(y)) {
      row_.y0 = row->y0;
      row_.y1 = row->y1;
      hover_ = row->index;
      draw_row(hover_, true);
    }
  } else if (scroll_bar_) {
    const int mid = pane_.y0 + (pane_.y1 - pane_.y0) / 2;
    const int arrow = y > mid ? 3 : 2;
    if (arrow_ >= 2 && arrow == arrow_) {
      return;
    }
    arrow_ = arrow;
    draw_scroll_bar();
  }
}

void ListBox::click(int x, int y, int button) {
  if (x < pane_.x1 - bar_width_) {
    if (hover_ != -1) {
      // In help mode (`0x11f0e8`, the window manager's flag) the row asks
      // for its help instead, without the sound.
      if (!wm_.help_mode()) {
        if (sound_ != -1) {
          wm_.services().sound.play_sfx(sound_);
        }
        wm_.send_event(parent_, kEventListPick, hover_, button);
      } else {
        wm_.send_event(parent_, kEventListHelp, hover_, button);
      }
      track_mouse(x, y);
    }
  } else if (scroll_bar_) {
    const int msg = y < (pane_.y1 - pane_.y0) / 2 + pane_.y0 ? kEventScrollUp : kEventScrollDown;
    handle_event(msg, 0, 0);
    wm_.set_timer(id_, msg, 0, 0, kRepeatDelay);
    wm_.request_notify(id_, button == kEventRightClick ? WinMgr::kNotifyRight : WinMgr::kNotifyLeft, kEventScrollStop,
                       0, 0);
  }
}

void ListBox::repeat_scroll(int msg) {
  const int period = wm_.timer_period(id_, msg);
  if (period != -1) {
    wm_.set_timer_period(id_, msg, static_cast<std::uint32_t>(std::max(period * 2 / 3 - 1, 0)));
  }
  scroll_step(msg);
}

bool ListBox::scroll_step(int msg) {
  if (msg == kEventScrollUp) {
    if (top_ == 0) {
      return false;
    }
    --top_;
  } else if (top_ < count() - (pane_.y1 - pane_.y0) / line_height_) {
    ++top_;
  } else {
    return false;
  }
  draw(kDrawNormal);
  return true;
}

void ListBox::drop_hover() {
  if (hover_ != -1) {
    draw_row(hover_, false);
    hover_ = -1;
  }
}

bool ListBox::takes_view_gesture(int x, int y, bool mouse) const { return (!mouse || mouse_drag_) && contains(x, y); }

void ListBox::view_drag(int /*dx*/, int dy) {
  drop_hover();
  drag_ += dy;
  // The finger up: the next rows come in from the bottom; down: the
  // previous ones from the top.
  while (top_ < count() && drag_ <= -height_of(items_[static_cast<std::size_t>(top_)])) {
    const int height = height_of(items_[static_cast<std::size_t>(top_)]);
    if (!scroll_step(kEventScrollDown)) {
      drag_ = 0;
      return;
    }
    drag_ += height;
  }
  while (top_ > 0 && drag_ >= height_of(items_[static_cast<std::size_t>(top_ - 1)])) {
    drag_ -= height_of(items_[static_cast<std::size_t>(top_ - 1)]);
    scroll_step(kEventScrollUp);
  }
  if (top_ == 0 && drag_ > 0) {
    drag_ = 0;
  }
}

void ListBox::view_wheel(int notches) {
  drop_hover();
  const int msg = notches > 0 ? kEventScrollUp : kEventScrollDown;
  for (int i = 0; i < std::abs(notches); ++i) {
    if (!scroll_step(msg)) {
      break;
    }
  }
  track_mouse(wm_.input().x, wm_.input().y);
}

void ListBox::view_end() {
  drag_ = 0;
  const InputState& in = wm_.input();
  if (shown_ && !wm_.in_modal() && contains(in.x, in.y)) {
    track_mouse(in.x, in.y);
  }
}

bool ListBox::handle_event(int type, int a, int b) {
  switch (type) {
    case kEventActivate:
      set_style(style_);
      return GWindow::handle_event(type, a, b);
    case kEventLeftClick:
    case kEventRightClick:
    case kEventHelpClick:
      if (!contains(a, b)) {
        return false;
      }
      if (type == kEventHelpClick && !help_.starts_with('0')) {
        return GWindow::handle_event(type, a, b);
      }
      click(a, b, type);
      return true;
    case kEventMouseLeave:
      wm_.cancel_notify(id_, -1);
      wm_.kill_timer(id_, -1);
      hover_ = -1;
      arrow_ = 0;
      // The original draws a hidden list too (the planet window's leave
      // broadcast before it opens its build list), with the SHAPEFRAME
      // arrows not set yet; its activation draws over it at once.
      if (shown_) {
        draw(kDrawNormal);
      }
      return true;
    case kEventMouseEnter:
      wm_.request_notify(id_, WinMgr::kNotifyMove, kEventMouseMove, 0, 0);
      track_mouse(wm_.input().x, wm_.input().y);
      return true;
    case kEventMouseMove:
      track_mouse(a, b);
      return true;
    case kEventScrollUp:
    case kEventScrollDown:
      repeat_scroll(type);
      return true;
    case kEventScrollStop:
      wm_.cancel_notify(id_, kEventScrollStop);
      wm_.kill_timer(id_, -1);
      return true;
    default:
      return GWindow::handle_event(type, a, b);
  }
}

}  // namespace oa
