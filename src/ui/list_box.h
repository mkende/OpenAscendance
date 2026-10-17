// The list box (`gwindow.cpp`, class `ListBox`, `windows.txt` TYPE 10, ctor
// 0x2d968, vtable 0x85c58): rows of text (or drawn by a callback) with a
// hovered-row highlight, a scroll bar whose arrows repeat while held, and the
// pick message 0x1c01 to the parent (`recon/notes/ui.md`, "ListBox").
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ui/gwindow.h"

namespace oa {

class ListBox final : public GWindow {
 public:
  static constexpr int kMaxItems = 256;
  // The first auto-repeat delay of the scroll arrows, in ticks; each repeat
  // shortens it to `period * 2 / 3 - 1` (at least 0).
  static constexpr std::uint32_t kRepeatDelay = 30;
  // `gwshare.txt` index of `listbox.shp` (the arrows).
  static constexpr int kListBoxShapes = 36;

  struct Item {
    std::string text;
    int data = 0;
  };
  // Draws `item` in `row` (inclusive screen coordinates), highlighted when
  // hovered (the `+0x8fa` callback).
  using DrawItem = std::function<void(const Pane& row, const Item& item, bool hilite)>;
  // The height of `item`'s row (the `+0x8f6` callback); without one, every
  // row is the font height + 3.
  using ItemHeight = std::function<int(const Item& item)>;
  // `qsort` comparison of two items (the `+0x8fe` callback).
  using Compare = std::function<int(const Item& a, const Item& b)>;

  // `ListBox_init` (0x2d9d8): `listbox.shp`, style 1, the text rows of the
  // large font.
  ListBox(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  // `ListBox_set_style` (0x2e0ec): the arrows' frames (style 1: 0 and 1,
  // otherwise 2 and 3), applied again at each activation.
  void set_style(int style);
  // The colours of the border, the background and the scroll bar
  // (`+0xc4..0xc6`: 0x31, 0xf2, 0).
  void set_colors(std::uint8_t border, std::uint8_t back, std::uint8_t bar);
  // `+0xaf`: the scroll bar stays when the list is cleared.
  void set_keep_scroll_bar(bool keep) { keep_scroll_bar_ = keep; }
  // `+0xb3`: the scroll bar shows.
  void set_scroll_bar(bool on);
  // `+0xb7`: the window grows downward with its rows instead of scrolling.
  void set_grow(bool grow) { grow_ = grow; }
  // `+0xbb`: the hovered row is redrawn at each mouse move inside it.
  void set_redraw_hover(bool redraw) { redraw_hover_ = redraw; }
  void set_draw_item(DrawItem draw) { draw_item_ = std::move(draw); }
  void set_item_height(ItemHeight height) { item_height_ = std::move(height); }

  // `ListBox_add_item` (0x2e1ac): appends a row of `height` pixels (-1: the
  // line height), drawing the list again with `redraw`; returns its index.
  int add_item(std::string text, int height = -1, bool redraw = false);
  // `ListBox_set_item_data` / `ListBox_get_item_data` / `get_item_text`.
  void set_item_data(int index, int data);
  const Item& item(int index) const { return items_.at(static_cast<std::size_t>(index)); }
  int count() const { return static_cast<int>(items_.size()); }
  // `ListBox_clear` (0x2e46c).
  void clear();
  // `ListBox_scroll_to` (0x2e3fc): makes `row` the top one when the rows
  // overflow the window.
  void scroll_to(int row);
  // `ListBox_sort` (0x2e900): the Watcom `qsort` of the rows.
  void sort(const Compare& compare);

  int top() const { return top_; }
  int hover_row() const { return hover_; }
  bool scroll_bar() const { return scroll_bar_; }
  int line_height() const { return line_height_; }
  // The rows' default height (`+0x8c9`), set by the screens (LoadWnd).
  void set_line_height(int height) { line_height_ = height; }
  // The arrow state (`+0x8e5`): 0/1 idle, 2 up held, 3 down held.
  int arrow_state() const { return arrow_; }

  // `ListBox_handle_event` (0x2daf4).
  bool handle_event(int type, int a, int b) override;
  // `ListBox_draw` (0x2dde0): background, border, scroll bar, the rows from
  // the top one.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

  // `ListBox_draw_text_item` (0x2d928): the default row drawer.
  void draw_text_item(const Pane& row, const Item& item, bool hilite);

 private:
  int height_of(const Item& item) const;
  // `ListBox_draw_scrollbar` (0x2df88).
  void draw_scroll_bar();
  // `ListBox_track_mouse` (0x2e4e8): the hovered row and arrow under (x, y).
  void track_mouse(int x, int y);
  // `ListBox_click` (0x2e7b0): picks the hovered row, or starts an arrow's
  // auto-repeat.
  void click(int x, int y, int button);
  // Shortens the auto-repeat period of `msg` and scrolls one row.
  void repeat_scroll(int msg);
  void draw_row(int index, bool hilite);

  std::vector<Item> items_;
  DrawItem draw_item_;
  ItemHeight item_height_;
  int style_ = 1;
  std::uint8_t border_color_ = 0x31;
  std::uint8_t back_color_ = kBackColor;
  std::uint8_t bar_color_ = 0;
  bool keep_scroll_bar_ = false;
  bool scroll_bar_ = false;
  bool grow_ = false;
  bool redraw_hover_ = false;
  int bar_width_ = 0;       // +0xbf: the arrows' width + 3
  int line_height_ = 0;     // +0x8c9
  int content_height_ = 0;  // +0x8cb
  int top_ = 0;             // +0x8cd
  int hover_ = -1;          // +0x8cf
  Pane row_;                // +0x8d1: the hovered row
  int arrow_ = 0;           // +0x8e5
};

}  // namespace oa
