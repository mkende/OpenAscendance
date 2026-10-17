// The System Display's control panel (`batcon.cpp`, `docs/08-combat.md`
// "Turn order in the System Display"): `BatControlWnd` (`windows.txt` TYPE
// 19, ctor 0x10a3c, vtable 0x85838), with no area of its own, owns the panel
// on the right (the system's name and controlling race, or the selected ship
// or planet), its list (`BatListWnd`: the player's or the others' ships and
// planets, a ship's gizmos of a category, a planet's orbital weapons), the
// Pass, Repeat and Auto buttons and the battle's turn loop
// (`game::BattleControl`): the races present play in turn, each chosen action
// animated by the view (`BatModeWnd`) before it is applied. The My Items
// button with the player's flag (`FlagButton`, TYPE 32) is here too.
#pragma once

#include <cstdint>
#include <optional>

#include "game/battle_control.h"
#include "game/float_math.h"
#include "game/state.h"
#include "ui/list_box.h"
#include "ui/shape_button.h"
#include "ui/winmgr.h"

namespace oa {

class BatModeWnd;

class BatControlWnd final : public GWindow {
 public:
  // The kinds of the objects the panel and the view exchange (the
  // selection `+0xab`, the target kinds of messages 0x322 and 0x326).
  enum ObjectKind : int {
    kObjectNone = 0,
    kObjectPlanet = 2,
    kObjectShip = 3,
    kObjectLane = 4,
    kObjectPoint = 5,  // the view's move target point
  };
  // The list modes (`+0xb9`).
  enum ListMode : std::uint8_t {
    kListNone = 0,
    kListMine = 1,     // the player's ships in space and planets
    kListGizmos = 2,   // the selected ship's gizmos of a category
    kListOrbital = 3,  // the selected planet's orbital weapons
    kListOthers = 4,   // the other races' ships in space and planets
  };
  // The chosen actions (`+0xb0`).
  enum Action : std::uint8_t { kActionMove = 0, kActionGizmo = 1, kActionPlanetWeapon = 2 };
  // The messages of the buttons.
  enum Message : int {
    kMessageMyItems = 804,
    kMessageOtherItems = 805,
    kMessagePass = 807,
    kMessageAuto = 808,
    kMessageRepeat = 809,
    kMessageWeapons = 810,
    kMessageShields = 811,
    kMessagePassive = 812,
    kMessageSpecials = 813,
    kMessageListHelp = 0x1c02,
  };

  // The selection (`+0xab` kind, `+0xac` object index).
  struct Selection {
    int kind = kObjectNone;
    int object = 0;
  };

  BatControlWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  // `BatControlWnd_handle` (0x10b24).
  bool handle_event(int type, int a, int b) override;
  // `BatControlWnd_draw` (0x11870): the list filled again (unless `mode` is
  // the list pick 0x1c01), the panel's top: the selected object, or the
  // system's sun, name, type and controlling race.
  void draw(int mode) override;
  void draw_hilite(int /*mode*/) override {}

  // Message 800: the view's selection (`kind`, `object`); the list shows
  // its gizmos or weapons, or the player's items.
  void select(int kind, int object);
  // Message 0x322: whether the chosen gizmo or planet weapon can be used on
  // `object` of `kind` (its action block filled and checked).
  bool check_target(int kind, int object);
  // The bytes a move order's action block takes from the stack: message
  // 0x326 copies a block whose gizmo slot, and target for a point, it never
  // sets, so they keep what the calls before it left there (observed in the
  // original's runs for each way of ordering a move).
  struct StaleBlock {
    game::RawAddress slot = 0;
    std::optional<game::RawAddress> target;  // none: the ship's own is kept
  };
  // The view's grid point picked by its second (height) click at screen
  // row `y`, an item clicked in the view, Repeat Move.
  static StaleBlock stale_from_view_point(int y) { return {0, static_cast<game::RawAddress>(y + 8) << 16}; }
  static constexpr StaleBlock kStaleFromViewItem{0x08000000, std::nullopt};
  static constexpr StaleBlock kStaleFromRepeat{0x10000000, 0xfd8c0000};

  // Message 0x326: the move or the checked action on `object` of `kind`
  // (the view's move point, or `point` when given, for `kObjectPoint`),
  // then its animation; a move's action block takes `stale`.
  void execute(int kind, int object, const StaleBlock& stale = kStaleFromViewItem, const game::Vec3* point = nullptr);
  // `BatControlWnd_finish_action` (0x12238) at the end of an animation:
  // the action applied, then (`interactive`) the list and the next turns,
  // else the rest of the round at once (the display closing).
  void finish_action(bool interactive);

  const Selection& selection() const { return selection_; }
  ListMode list_mode() const { return list_mode_; }
  const game::BattleControl& battle() const { return battle_; }

 private:
  BatModeWnd* mode_wnd() const;
  ListBox* list() const;
  game::Session* core() const { return wm_.services().game.core(); }
  // `BatControlWnd_fill_list` (0x11ba4): the rows of the list mode, sorted,
  // drawn.
  void fill_list();
  // `BatControlWnd_update_category_buttons` (0x12140).
  void update_category_buttons();
  // `BatControlWnd_run_turns` (0x1229c) with animations: `turns` runs the
  // next races' turns (`game::BattleControl`); the panel is redrawn after a
  // round (with the selection of before), the next action animated.
  template <typename Turns>
  void run_turns(Turns turns);
  // The list's row drawers (`BatControlWnd_draw_object_item` 0x10010,
  // `_draw_gizmo_item` 0x10668, `_draw_orbital_item` 0x10878); `planet`
  // for a planet row (data bit 31).
  void draw_object_item(const Pane& row, int object, bool planet, bool hilite);
  void draw_gizmo_item(const Pane& row, int slot, int status, bool hilite);
  void draw_orbital_item(const Pane& row, int square, int status, bool hilite);
  // A list row picked (`a` the row, `b` the button).
  void list_pick(int row, int button);
  void activate(int a, int b);

  Selection selection_;
  Action action_ = kActionMove;     // +0xb0
  int action_slot_ = 0;             // +0xb1 gizmo slot or planet square
  ListMode list_mode_ = kListNone;  // +0xb9
  std::uint8_t category_ = 0;       // +0xba
  // The repeat move (`+0xbf` ship, -1 none; `+0xc3` kind; `+0xc4` target;
  // `+0xc8` point).
  int repeat_ship_ = -1;
  int repeat_kind_ = kObjectNone;
  int repeat_object_ = 0;
  game::Vec3 repeat_point_{};
  game::BattleControl battle_;
};

// TYPE 32 (My^Items): the race-coloured button with the player's flag
// (`FlagButton_draw` 0x1240c).
class FlagButton final : public RaceColorButton {
 public:
  using RaceColorButton::RaceColorButton;

  void draw(int mode) override;
};

// Adds the System Display's classes (TYPE 4, 19, 32) to `factory`.
void add_star_system_windows(WindowFactory& factory);

}  // namespace oa
