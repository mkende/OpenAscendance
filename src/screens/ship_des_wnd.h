// The ship design screen (screen 8, `shipwin.cpp`, class `ShipDesWnd`,
// `windows.txt` TYPE 8 SHIPDESSCREEN, ctor 0x48d3c, vtable 0x85efc): the
// hull in its dock, the isometric slot grid, the component list GIZLIST of
// the category buttons, the selected component and the ship's title, build
// days and strength bars (`docs/11-screens.md`, "Ship design screen"). The
// planet screen opens it on a new ship or a refit, the Ships list on a ship
// to view (read-only).
#pragma once

#include <cstdint>

#include "game/state.h"
#include "ui/gwindow.h"
#include "ui/list_box.h"
#include "ui/winmgr.h"

namespace oa {

namespace game {
struct Session;
}  // namespace game

class ShipDesWnd final : public GWindow {
 public:
  // What the screen edits (`+0xc9`).
  enum Mode : int { kNewDesign = 0, kRefit = -1 };
  // The messages of the buttons (`windows.txt` SENDMESSAGE).
  enum Message : int {
    kMessageHull = 50,
    kMessageWeapons = 51,
    kMessageDefenses = 52,
    kMessageSensors = 53,
    kMessageEngines = 54,
    kMessageEnergy = 55,
    kMessageSpecial = 57,
  };
  // The gizmo categories (`gizmos.txt` Cat).
  enum Category : int { kWeapon = 0, kShield = 1, kDrive = 2, kScanner = 3, kGenerator = 4, kSpecial = 5 };
  // The cost `refit_cost` returns outside a refit.
  static constexpr std::int32_t kNoRefitCost = 10000;

  ShipDesWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes) : GWindow(wm, record, shapes) {}

  // `ShipDesWnd_open` (0x48e8c): the ship the next activation edits, a copy
  // of it kept for a refit's cost; nothing can be changed when `read_only`.
  void open(game::Ship* ship, Mode mode, bool read_only);

  // `ShipDesWnd_handle_event` (0x491c0): the activation selects ENERGY on
  // the first visit; the left / right button fits the selected component
  // into / empties the hovered slot; the deactivation keeps a copy of the
  // ship as the template shown by an entry without one, and forgets the
  // ship unless it was modified.
  bool handle_event(int type, int a, int b) override;
  // `ShipDesWnd_draw` (0x49b68).
  void draw(int mode) override;

  // The edited ship (`+0xab`): null once the screen was left without a
  // change, the template when entered without one.
  game::Ship* ship() const { return ship_; }
  Mode mode() const { return mode_; }
  bool modified() const { return modified_; }
  bool read_only() const { return read_only_; }
  // The selected component (-1 none), the category, the hovered slot (-1).
  int selected_gizmo() const { return selected_; }
  int category() const { return category_; }
  int hovered_slot() const { return hovered_; }
  // The gizmos of the list, in order.
  int gizmo_count() const { return list_ != nullptr ? list_->count() : 0; }
  int gizmo_at(int row) const { return list_->item(row).data; }
  // `ShipDesWnd_refit_cost` (0x4a210): the refit's cost, `kNoRefitCost`
  // when the screen does not refit a ship.
  std::int32_t refit_cost(const game::Session& session) const;

 private:
  game::Session* session() const;
  // `ShipDesWnd_select_category` (0x49a54) / `_fill_gizmo_list` (0x49124).
  void select_category(int category);
  void fill_gizmo_list();
  // A click on the hovered slot: fits (left) or removes (right).
  bool edit_slot(bool install);
  void draw_grid(const game::Ship& ship);
  void draw_info(const game::Session& session, const game::Ship& ship);
  // `ShipDesWnd_draw_gizmo_item` (0x48b34).
  void draw_gizmo_row(const Pane& row, int gizmo, bool hilite);

  game::Ship* ship_ = nullptr;  // +0xab
  std::int8_t selected_ = -1;   // +0xbb
  int category_ = kWeapon;      // +0xbc
  bool redraw_info_ = false;    // +0xbd
  bool redraw_dock_ = false;    // +0xc1
  ListBox* list_ = nullptr;     // +0xc5
  Mode mode_ = kNewDesign;      // +0xc9
  game::Ship original_;         // +0xcd, the ship before a refit
  bool modified_ = false;       // +0x22f
  bool read_only_ = false;      // +0x233
  int hovered_ = -1;            // 0x8695c
  game::Ship template_;         // 0xf84bc, the last ship shown
};

// Adds `ShipDesWnd` (TYPE 8) to `factory`.
void add_ship_design_windows(WindowFactory& factory);

}  // namespace oa
