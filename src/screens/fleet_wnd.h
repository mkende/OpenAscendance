// The Ships list (screen 18, `status.cpp`, class `FleetWnd`, `windows.txt`
// TYPE 17 FLEETWND, ctor 0x4cf74, vtable 0x85f90): the player's ships in the
// STATSHIPLST list box, oldest first, each row with its picture, name,
// experience, strength bars, hull and power gauges, location and its
// planet or star picture; a click opens where the ship is, a right click
// shows its design (`docs/11-screens.md`, "Ships list").
#pragma once

#include <cstdint>
#include <string>

#include "game/address.h"
#include "game/state.h"
#include "ui/gwindow.h"
#include "ui/list_box.h"
#include "ui/winmgr.h"

namespace oa {

namespace game {
struct Session;
}  // namespace game

class FleetWnd final : public GWindow {
 public:
  // The rows' height (`ListBox_add_item` height 100) and the list's line
  // height (`+0x8c9`).
  static constexpr int kRowHeight = 100;
  static constexpr int kLineHeight = 0x6a;

  FleetWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) : GWindow(wm, record, shapes) {}

  // `FleetWnd_handle_event` (0x4d134): 1 fills the list with the player's
  // ships (`Race_list_ships` order, then sorted again by built day),
  // scrolled to the row kept at the last deactivation (2); 0x1c01 (a row
  // picked) with the right button opens the ship read-only in the design
  // screen (8), otherwise the planet screen (17) of a ship under
  // construction, refitted or in orbit, the star system (16) of a ship in a
  // system, or goes back (message 3) for a ship in a starlane.
  bool handle_event(int type, int a, int b) override;
  // A long press on a ship's row: the right button (the ship's design).
  LongPress child_long_press(const GWindow& child, int x, int y) const override;
  // `FleetWnd_draw` (0x4cfd4): the title "<species>  (N Ships, M more
  // allowed)" in the player's colour with its flag, then the list.
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }

  // The ship of each row, in list order.
  int row_count() const { return list_ != nullptr ? list_->count() : 0; }
  game::ShipIndex row_ship(int row) const { return static_cast<game::ShipIndex>(list_->item(row).data); }

  // The location line of a row (static strings 143..150): where the ship
  // is, a starlane's destination and days left, its standing order in a
  // system.
  static std::string location_text(WinMgr& wm, const game::Session& session, const game::Ship& ship);
  // The days left to the end of the starlane of a ship in one (the ETA of
  // `FleetWnd_draw_ship_item`): `chop((length - travelled) / speed) + 1`.
  static std::int32_t lane_eta(const game::Session& session, const game::Ship& ship);

 private:
  void fill();
  // `FleetWnd_draw_ship_item` (0x4c180).
  void draw_row(const Pane& row, const game::Ship& ship, bool hilite);

  ListBox* list_ = nullptr;  // +0xab
  int top_ = 0;              // +0xaf, the list's top row kept between visits
};

// Adds `FleetWnd` (TYPE 17) to `factory`.
void add_fleet_windows(WindowFactory& factory);

}  // namespace oa
