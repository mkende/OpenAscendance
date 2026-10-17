// The star map (`coswnd.cpp`, `docs/11-screens.md` "Star map"): `CosWnd`
// (`windows.txt` TYPE 2 COSMOSWnd, ctor 0x21bc4, vtable 0x85a34), the 3D
// galaxy of the Galactic Display (screen 1), turning in the new-game screen
// (3), behind the species circle (7) and as the diplomacy mini map (13); and
// the "Species" button of screen 1 with the known species' flags
// (`RacesButton`, TYPE 5, vtable 0x85a1c) and the race slots of the species
// selection (`RaceSelButton`, TYPE 6).
//
// Each galaxy change (activation, new day) rebuilds the display items (the
// stars, a ship marker after each star with ships, the lanes the player
// knows, the ships between stars); each camera change projects them and
// sorts the visible ones farthest first (the Watcom `qsort`), and the draw
// walks that list.
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "screens/camera3d.h"
#include "ui/winmgr.h"

namespace oa {

class CosWnd final : public GWindow {
 public:
  // The display items' capacity (`+0xb9`) and the draw list's (`+0x172`).
  static constexpr std::size_t kMaxItems = 355;
  static constexpr std::size_t kMaxDrawn = 256;
  // The default camera (`CosWnd_reset_camera` 0x21f2c): the field of view,
  // the clip depths and the distance from the galaxy's centre.
  static constexpr float kFov = 35.0F;
  static constexpr float kNear = 1.0F;
  static constexpr float kFar = 100000.0F;
  static constexpr float kDistance = 1500.0F;
  // The zoom limits (degrees of field of view, 0x80fb4 / 0x80fb8).
  static constexpr float kMinFov = 20.0F;
  static constexpr float kMaxFov = 60.0F;

  // `CosWnd_build_items` (0x24450): the display items of the current
  // galaxy (also after the new-game screen's new galaxies).
  void build_items();

  // The messages of the child buttons and of the window's own timer.
  enum Message : int {
    kMessageKnownRaces = 26,  // the known races' mask = p1
    kMessageRotateLeft = 50,  // p1: the mouse button (4 held, 5 latched)
    kMessageRotateRight = 51,
    kMessageRotateUp = 52,
    kMessageRotateDown = 53,
    kMessageZoomIn = 54,
    kMessageZoomOut = 55,
    kMessageCenter = 56,
    kMessageLanes = 57,
    kMessageShips = 58,
    kMessageTick = 59,  // the per-pass timer: the continuous actions
    kMessageEmpire = 60,
    kMessageColonies = 61,
    kMessageRaceFilter = 62,  // p1: the race's bit
    kMessageStopAction = 63,  // p1: the action bits to stop
  };

  // The continuous actions (`+0xc0`).
  enum Action : std::uint32_t {
    kActionRotateY = 0x01,
    kActionRight = 0x02,
    kActionRotateX = 0x04,
    kActionDown = 0x08,
    kActionZoom = 0x10,
    kActionOut = 0x20,
  };

  enum class ItemKind : std::uint8_t { kStar = 0, kLane = 1, kShip = 2 };

  // A display item (`CW DISPLAYITEMS`, 0x15 bytes): its object (star, lane
  // or ship index), the projected point(s), window-relative, and the view
  // depth. Items keep their last projection when clipped.
  struct Item {
    ItemKind kind = ItemKind::kStar;
    std::int16_t index = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t x2 = 0;  // a lane's second end
    std::int16_t y2 = 0;
    float depth = 0;
    bool clipped = false;
  };

  CosWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  // `CosWnd_handle_event` (0x21fc4): activation and deactivation per screen,
  // the pointer and hover (6, 7, 8), clicks (4, 5) and help (0xc), the
  // rotation speed keys (- and =), the new day (0xd) and the messages.
  bool handle_event(int type, int a, int b) override;
  // `CosWnd_draw` (0x23310): the items, the new-game box over it on screen
  // 3, the species circle on screen 7, the player's flag and the hovered
  // star's label.
  void draw(int mode) override;
  // `CosWnd_serialize` (0x246ec): the actions, toggles, masks, camera and
  // field of view (0xbe bytes; the last four, stale stack bytes in the
  // original, as `stale_cluster_bytes`).
  bool save_state(std::span<std::uint8_t> out) const override;
  void load_state(std::span<const std::uint8_t> in) override;
  // Begin New Game: `CosWnd_reset_camera`.
  void new_game() override { reset_camera(); }

  const Camera3d& camera() const { return camera_; }
  float fov() const { return fov_; }
  std::uint32_t actions() const { return actions_; }
  std::uint8_t race_filter() const { return race_filter_; }
  std::uint8_t known_races() const { return known_races_; }
  std::span<const Item> items() const { return std::span(items_).first(item_count_); }
  // The visible items' indices, farthest first (`CosDrawList`).
  std::span<const std::int32_t> draw_list() const { return std::span(drawn_).first(drawn_count_); }
  // The hovered item, -1 for none (`+0xbb`).
  int hovered() const { return hovered_; }
  const ShapeTable& star_shapes() const { return *star_shapes_; }
  // The species circle's portrait centres (`0xc8452`), window-relative.
  const std::array<ScreenPoint, 7>& circle() const { return circle_; }

 private:
  // The stale last 4 bytes of the saved record: what `Cosmos_save` left on
  // the stack (the last cluster's star indices 82 and 83, halves).
  std::uint32_t stale_cluster_bytes() const;
  // The window rectangle and the projection centre (`+0x576`, `+0x578`).
  void set_rect(int x0, int y0, int x1, int y1);
  // `Camera_set_projection` with the window's field of view.
  void set_projection();
  // `CosWnd_reset_camera` (0x21f2c).
  void reset_camera();
  // `CosWnd_project_items` (0x2394c), then the depth sort
  // (`CosDrawList_sort` 0x24bfc).
  void project_items();
  // `CosWnd_rotate_step` (0x23874): about the y axis or the x axis.
  void rotate_step(bool y_axis, float degrees);
  // `CosWnd_zoom_step` (0x238b8): `dir` -1 in, 1 out.
  void zoom_step(int dir);
  // `CosWnd_update_hover` (0x24068), window-relative.
  void update_hover(int x, int y);
  // `CosWnd_click_item` (0x24300).
  void click_item();
  // `CosDrawList_draw` (0x24c18).
  void draw_items();
  // `CosWnd_draw_race_portraits` (0x236f4).
  void draw_race_portraits();
  // The hovered star's name and type at the map's bottom-right corner, on
  // `background` (the font's for the draw, none for the hover).
  void draw_label(int background);
  // Activation (event 1) and deactivation (2).
  void activate(int a, int b);
  void deactivate(int a, int b);

  const ShapeTable* star_shapes_;  // +0xab STAR_SHAPES
  const ShapeTable* ship_shapes_;  // +0xaf SHIP_SHAPES
  std::array<Item, kMaxItems> items_{};
  std::size_t item_count_ = 0;
  int hovered_ = -1;
  std::uint8_t background_ = 0;  // +0xbf BACKGRNDCOLOR
  std::uint32_t actions_ = 0;    // +0xc0
  // The toggles (0 / -1): lanes, ships, colonies' rings and dots, home
  // flags, species mode (screen 7), empire lane colours.
  std::int32_t lanes_ = -1;          // +0xc4
  std::int32_t ships_ = -1;          // +0xc8
  std::int32_t colonies_ = -1;       // +0xcc
  std::int32_t flags_ = -1;          // +0xd0
  std::int32_t species_mode_ = 0;    // +0xd4
  std::int32_t empire_ = 0;          // +0x58a
  std::uint8_t race_filter_ = 0xff;  // +0xd8 RACECONTROLS
  std::uint8_t known_races_ = 1;     // +0xd9
  Camera3d camera_;                  // +0xda
  std::array<std::int32_t, kMaxDrawn> drawn_{};
  std::size_t drawn_count_ = 0;
  std::int16_t cx_ = 0;   // +0x576
  std::int16_t cy_ = 0;   // +0x578
  float fov_ = kFov;      // +0x57a
  int cursor_shape_ = 2;  // +0x57e CURSORSHAPE
  std::array<ScreenPoint, 7> circle_{};
};

// TYPE 5 (RACES): the button, then the flags of the player and of every
// species it has met, 16 pixels apart from x 47 (`RacesButton_draw`
// 0x2536c).
class RacesButton final : public GWindow {
 public:
  using GWindow::GWindow;

  void draw(int mode) override;
};

// TYPE 6 (RACE_1..RACE_7, screen 7): a race slot of the species selection
// (`RaceSelButton`, ctor 0x253d8, vtable 0x85a04): the player's count of
// known species, a met species' name in its colour with its flag and its war
// or alliance mark, "Extinct", or nothing for a species not met. A click on
// a met, living species makes it the one dealt with (`0x8675d`) before the
// button's message opens the diplomacy screen.
class RaceSelButton final : public GWindow {
 public:
  using GWindow::GWindow;

  int slot() const { return slot_; }

  // `RaceSelButton_handle_event` (0x25720): 1 takes the slot from the name's
  // digit (inactive past the race count), 2 wipes the button, 4/5 pick the
  // species, 7 / 6 draw hot / not, 0xb sets the slot, 0xd redraws.
  bool handle_event(int type, int a, int b) override;
  // `RaceSelButton_draw` (0x25438): `mode` 0 or the background colour.
  void draw(int mode) override;
  // `RaceSelButton_draw_hilite` (0x256d4): drawn on 0x96.
  void draw_hilite(int /*mode*/) override;

 private:
  int slot_ = 0;  // +0xab
};

// Adds the star map's classes to `factory`.
void add_star_map_windows(WindowFactory& factory);

}  // namespace oa
