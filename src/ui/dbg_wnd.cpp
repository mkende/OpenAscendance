#include "ui/dbg_wnd.h"

#include <array>
#include <format>

#include "core/error.h"
#include "gfx/font.h"
#include "ui/list_box.h"
#include "ui/winmgr.h"

namespace oa {

namespace {

// The rows' text colours by their data (`0x86a50`): 0 most rows, 1 the
// shape cache's entries.
constexpr std::array<std::uint8_t, 2> kRowColors = {0xf3, 0x13};
// The rows' backgrounds: normal and hovered; a palette row's when it is the
// picked colour, and when hovered.
constexpr std::uint8_t kRowBack = 0xf2;
constexpr std::uint8_t kRowHotBack = 0x96;
constexpr std::uint8_t kPickedBack = 0xfe;
constexpr std::uint8_t kPaletteHotBack = 0xf1;
// The rows' font flags, as the original passes them.
constexpr int kRowFontFlags = 0x40;
// The palette row's swatch, right of its number.
constexpr int kSwatchX = 0x23;
// The title's rows in the client rectangle.
constexpr int kTitleHeight = 50;
// A right click picks a palette row (the list's pick carries the button).
constexpr int kRightButton = kEventRightClick;

// Adds `text` with `data` (the rows past the list's capacity are left out).
void add_row(ListBox& list, std::string text, int data = 0) {
  if (list.count() < ListBox::kMaxItems) {
    list.set_item_data(list.add_item(std::move(text)), data);
  }
}

}  // namespace

DbgWnd::DbgWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) : GWindow(wm, record, shapes) {
  const auto value = [&](const char* key) {
    const auto it = record.extras.find(key);
    return it == record.extras.end() ? 0 : std::stoi(it->second);
  };
  client_ = Pane{value("cX0"), value("cY0"), value("cX1"), value("cY1")};
}

ListBox& DbgWnd::list() const {
  auto* list = dynamic_cast<ListBox*>(wm_.find_window("DBGWNDLIST"));
  if (list == nullptr) {
    throw DataError("windows.txt: no DBGWNDLIST");
  }
  return *list;
}

bool DbgWnd::handle_event(int type, int a, int b) {
  switch (type) {
    case kEventActivate:
    case kEventDeactivate:
    case kEventMouseLeave:
    case kEventMouseEnter:
      return GWindow::handle_event(type, a, b);
    case kEventModalOpen:
      fill(Mode::kWindows);
      return false;
    case kEventOk:
    case kEventCancel:
      wm_.post_message(kMessageEndModal, type, 0);
      return true;
    case kMessageWindows:
    case kMessageMemory:
    case kMessageCache:
    case kMessagePalette:
    case kMessageTunes:
      if (const auto mode = static_cast<Mode>(type - kMessageWindows); mode != mode_) {
        fill(mode);
        draw(kDrawNormal);
      }
      return true;
    case kMessagePalettePick:
      if (mode_ == Mode::kPalette) {
        list().scroll_to(wm_.picked_color());
        draw(kDrawNormal);
      }
      return true;
    case kEventListPick:
      if (mode_ == Mode::kPalette) {
        if (b == kRightButton) {
          wm_.focus_map()[kRowBack] = static_cast<std::uint8_t>(a);
        }
      } else if (mode_ == Mode::kTunes) {
        wm_.services().sound.play_tune(a, false);
      }
      return true;
    default:
      return false;
  }
}

void DbgWnd::fill(Mode mode) {
  mode_ = mode;
  ListBox& rows = list();
  rows.clear();
  if (mode == Mode::kPalette) {
    // DbgWnd_draw_palette_item 0x558f4: the index, then a swatch of it.
    rows.set_draw_item([this](const Pane& row, const ListBox::Item& item, bool hilite) {
      const bool picked = item.object == wm_.picked_color();
      std::uint8_t back = picked ? kPickedBack : kRowBack;
      if (hilite) {
        back = kPaletteHotBack;
      }
      GameFont& font = wm_.large_font();
      font.pane() = row;
      font.draw_wrapped(wm_.screen(), 1, 1, std::format("{:03d}", item.object), kRowFontFlags, picked ? 0 : kTextColor,
                        back, 0);
      vfx_pane_wipe(wm_.screen(), Pane{row.x0 + kSwatchX, row.y0 + 1, row.x1, row.y1 - 1},
                    static_cast<std::uint8_t>(item.object));
    });
    for (int i = 0; i < 256; ++i) {
      rows.set_item_object(rows.add_item(""), i);
    }
    return;
  }
  // DbgWnd_draw_list_item 0x5588c.
  rows.set_draw_item([this](const Pane& row, const ListBox::Item& item, bool hilite) {
    vfx_pane_wipe(wm_.screen(), row, hilite ? kRowHotBack : kRowBack);
    GameFont& font = wm_.large_font();
    font.pane() = row;
    font.draw_wrapped(wm_.screen(), 1, 1, item.text, kRowFontFlags, kRowColors.at(static_cast<std::size_t>(item.data)),
                      0xff, 0);
  });
  switch (mode) {
    case Mode::kWindows:
      list_windows(rows);
      break;
    case Mode::kMemory:
      list_memory(rows);
      break;
    case Mode::kCache:
      list_cache(rows);
      break;
    case Mode::kTunes:
      list_tunes(rows);
      break;
    case Mode::kPalette:
      break;
  }
}

void DbgWnd::list_windows(ListBox& list) const {
  // `C` marks the children.
  for (const int id : wm_.screen_windows(wm_.current_screen())) {
    const GWindow& w = *wm_.window(id);
    add_row(list, std::format("{} HWND{:03d} {}", w.parent() != 0 ? 'C' : ' ', id, w.name()));
  }
}

void DbgWnd::list_memory(ListBox& list) const {
  // The original walks its heap: the blocks it tracks (`mem_track_object`,
  // a tick, a size and a name each: the windows, the shape tables and the
  // other allocations), with totals, and the heap's free bytes. The remake
  // has no such heap: the closest are its windows (of no tracked size) and
  // the shape tables loaded.
  int windows = 0;
  for (int id = 1; id <= wm_.window_slots(); ++id) {
    if (const GWindow* w = wm_.window(id)) {
      add_row(list, std::format("{:06d} S{:07d} WND[{}]", 0, 0, w->name()));
      ++windows;
    }
  }
  int shapes = 0;
  std::size_t shape_bytes = 0;
  for (const ShapeCache::Entry& e : wm_.shapes().entries()) {
    add_row(list, std::format("{:06d} S{:07d} {}", 0, e.bytes, e.file));
    ++shapes;
    shape_bytes += e.bytes;
  }
  add_row(list, std::format("** {} WNDS USING {} BYTES", windows, 0));
  add_row(list, std::format("** {} SHAPES USING {} BYTES", shapes, shape_bytes));
}

void DbgWnd::list_cache(ListBox& list) const {
  // The original lists its cache buffer's entries, then its named slots,
  // then its counts and sizes. The remake's cache keeps every table it
  // loads (`ShapeCache`): the slots are the entries, the buffer has no
  // free space.
  const std::vector<ShapeCache::Entry> entries = wm_.shapes().entries();
  std::size_t total = 0;
  for (const ShapeCache::Entry& e : entries) {
    add_row(list, std::format("{:07d} {}", e.bytes, e.file), 1);
    total += e.bytes;
  }
  add_row(list, " ");
  add_row(list, std::format("{:03d} ITEMS IN CACHE", entries.size()));
  add_row(list, std::format("{:03d} ITEMS LOADED", entries.size()));
  add_row(list, " ");
  add_row(list, std::format("{:07d} TOTAL CACHE SIZE", total));
  add_row(list, std::format("{:07d} USED", total));
  add_row(list, std::format("{:07d} FREE", 0));
}

void DbgWnd::list_tunes(ListBox& list) const {
  const SoundSystem& sound = wm_.services().sound;
  for (int t = 0; t < sound.tune_count(); ++t) {
    add_row(list, std::format("{:02d}. {:<24} ({})", t, sound.tune_file(t), sound.saved_position(t)));
  }
}

void DbgWnd::draw(int /*mode*/) {
  // The original also draws a shape and wipes the client rectangle, of
  // which nothing shows (the oracle's `dev_modes` run): its `SHAPEFILE`
  // (`TESTDLG.SHP`) is not drawn.
  // The tunes' list is titled as the palette's.
  const char* title = mode_ == Mode::kWindows  ? "WINDOWS"
                      : mode_ == Mode::kMemory ? "MEMORY"
                      : mode_ == Mode::kCache  ? "CACHE"
                                               : "PALETTE";
  GameFont& font = wm_.large_font();
  font.pane() = Pane{client_.x0, client_.y0, client_.x1, client_.y0 + kTitleHeight};
  font.draw_wrapped(wm_.screen(), 1, 1, title, 0, kDefault, kDefault, 0);
  draw_children();
}

}  // namespace oa
