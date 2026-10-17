// The star-system view of the System Display (`batmode.cpp`, `batfx.cpp`,
// `batfxa.cpp`; `docs/08-combat.md` "The view", "Effects and sounds"):
// `BatModeWnd` (`windows.txt` TYPE 4, ctor 0x15ed4, vtable 0x858d0), the 3D
// scene of the selected star (`Cosmos::selected_star`): its sun, planets, lane
// ends, the ships in space and the galaxy's backdrops, over a grid with
// elevation lines, seen by a camera orbiting a target point and saved per
// star; the player's pointing (selection, targets, the two-click moves) and
// the effect animations of the battle actions (`batfx.txt`).
//
// Each scene change rebuilds the display items (`build_scene`); each camera
// or item change projects them and sorts the visible ones into three depth
// lists (`refresh`): the backdrops, the items below the grid plane and those
// above it, drawn back to front around the grid (`draw`).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "game/state.h"
#include "gfx/vfx_poly.h"
#include "screens/camera3d.h"
#include "ui/winmgr.h"

namespace oa {

class BatControlWnd;

class BatModeWnd final : public GWindow {
 public:
  // The display items' capacity (`+0x1d2e`), the effect items (`+0x4f44`).
  static constexpr std::size_t kMaxItems = 128;
  static constexpr std::size_t kMaxEffectItems = 15;
  // The per-star camera arrays (indexed by star index).
  static constexpr std::size_t kMaxStars = 100;
  // The haze tables of the view (`BATHAZE`): 31 `BLACKnn.HAZ`, `SLANE03.HAZ`
  // and `RACEA03..RACEG03.HAZ`.
  static constexpr std::size_t kHazeTables = 39;
  static constexpr std::size_t kLaneHaze = 31;
  static constexpr std::size_t kRaceHaze = 32;
  // The serialized state (`BatModeWnd_serialize` 0x19e2c).
  static constexpr std::size_t kStateSize = 0x1a08;
  // The orbit of a view drag, in degrees per screen pixel.
  static constexpr float kDragDegrees = 0.5F;

  // The messages of the child buttons, the window's timers and the
  // `BatControlWnd`.
  enum Message : int {
    kMessageRotateLeft = 0x32,  // p1: the mouse button (4 held, 5 latched)
    kMessageRotateRight = 0x33,
    kMessageZoomIn = 0x36,
    kMessageZoomOut = 0x37,
    kMessageTick = 0x3a,        // the per-pass timer: rotation and zoom
    kMessageStopAction = 0x3b,  // p1: the action bits to stop
    kMessageRecenter = 0x47,
    kMessageGrid = 0x4a,
    kMessageElevation = 0x4c,
    kMessagePlanetWindow = 0x50,  // p1: the orbit + 1 of the planet to open
    kMessageTarget = 0x51,        // targeting state (a gizmo or weapon chosen)
    kMessageEffectTick = 0x52,    // the effect timer
    kMessageSelect = 0x53,        // the BatControlWnd selection, from its list
    kMessageAbandon = 0x54,
  };

  // The input states (`+0xb7`).
  enum class State : std::uint8_t { kIdle = 0, kTargeting = 1, kMovePoint = 2, kMoveElevation = 3, kAnimating = 4 };

  // The display item kinds (item `+0`).
  enum class ItemKind : std::uint8_t {
    kStar = 0,
    kPlanet = 1,
    kShip = 2,
    kLaneEnd = 3,
    kBackdrop = 4,
    kSpot = 5,  // picked within a radius (effects)
    kCursor = 6,
    kEffect = 7,
    kBeam = 8,
    kFree = 0xff,
  };

  // The item flags (`+0x33`).
  enum ItemFlag : std::uint8_t {
    kFlagStarSideA = 0x01,   // a lane end on the lane's star A side
    kFlagNoLine = 0x02,      // no elevation line
    kFlagCustomDraw = 0x04,  // drawn by the effect (`BatModeWnd_fx_draw_item`)
    kFlagHidden = 0x08,
    kFlagFxPosition = 0x10,  // positioned by the effect positions array
  };

  // A display item (0x35 bytes, `BATDISPLAYITEMS`).
  struct Item {
    ItemKind kind = ItemKind::kFree;
    // +1 the object: the star, planet, ship or lane index, the backdrop
    // number, or for a beam the item it joins.
    std::int32_t object = 0;
    // +5 the shape table (null: none, slot 0xffff) and +7 its frame.
    const ShapeTable* shapes = nullptr;
    std::int32_t frame = 0;
    float scale = 1.0F;           // +0xb
    std::int32_t draw_scale = 0;  // +0xf, 16.16
    // +0x13..+0x19 the frame's opaque box (`vfx_shape_bounds`).
    std::int16_t bx0 = 0;
    std::int16_t by0 = 0;
    std::int16_t bx1 = 0;
    std::int16_t by1 = 0;
    std::int32_t unprojected = -1;  // +0x1b: -1 until projected
    std::int32_t above = 0;         // +0x1f: -1 above the grid plane
    std::int32_t line = 0;          // +0x23: -1 within the grid (elevation line)
    std::int16_t x = 0;             // +0x27 window-relative
    std::int16_t y = 0;             // +0x29
    std::int16_t foot_x = 0;        // +0x2b the elevation line's foot
    std::int16_t foot_y = 0;        // +0x2d
    float depth = 0;                // +0x2f
    std::uint8_t fx_position = 0;   // +0x34 the effect positions index
    std::uint8_t flags = 0;         // +0x33 `ItemFlag`
  };

  // An elevation-line foot quad (100 bytes, `PolyList`): the four corners and
  // the race of the ship standing on it (-1 for none).
  struct Quad {
    std::array<PolyPoint, 4> corners{};
    std::int32_t race = -1;
  };

  BatModeWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  // `BatModeWnd_handle` (0x16348).
  bool handle_event(int type, int a, int b) override;
  // `BatModeWnd_draw` (0x18ca8): `mode` 1 during the effects.
  void draw(int mode) override;
  void draw_hilite(int /*mode*/) override {}
  // `BatModeWnd_serialize` (0x19e2c): the per-star cameras, the current one
  // and the toggles (`kStateSize` bytes).
  bool save_state(std::span<std::uint8_t> out) const override;
  void load_state(std::span<const std::uint8_t> in) override;
  // Begin New Game: every star's camera forgotten.
  void new_game() override { star_visited_.fill(0); }
  // The remake's gestures on the view (inside its rectangle, not while an
  // animation plays): a horizontal drag orbits the camera as the rotation
  // buttons do, `kDragDegrees` per pixel (the system follows the finger;
  // the original has no other rotation); a pinch zooms within the
  // original's limits.
  bool takes_view_gesture(int x, int y) const override;
  void view_drag(int dx, int dy) override;
  void view_pinch(float ratio) override;

  // `BatModeWnd_build_scene` (0x187ec).
  void build_scene();
  // `BatModeWnd_refresh` (0x18c2c): projection, depth sort and, with `draw`,
  // the drawing.
  void refresh(bool redraw = true);
  // `BatModeWnd_center_on_item` (0x17e88): the camera target on item `i`.
  void center_on_item(int i);
  // `BatModeWnd_find_display_item` (0x12ae4): the item of the ship (`ship`
  // true) or planet `index`, -1 for none.
  int find_item(ItemKind kind, std::int32_t object) const;
  // `BatModeWnd_start_effect` (0x12674): the effect of the BatControlWnd's
  // pending action starts at the next tick.
  void start_effect();
  // The effect timer (`BatModeWnd_effect_tick` 0x126f8); public for the
  // tests, which drive the animations.
  void effect_tick();
  // Selects the item of the BatControlWnd's selection (message 0x53).
  void select_from_control();

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }
  int selected() const { return selected_; }
  void set_selected(int item) { selected_ = item; }
  int hovered() const { return hovered_; }
  // The move target point (`+0x1d0a`).
  const Vec3& move_point() const { return move_point_; }
  std::span<const Item> items() const { return std::span(items_).first(item_count_); }
  const Camera3d& camera() const { return camera_; }
  const Vec3& target() const { return target_; }
  float distance() const { return distance_; }
  float grid_radius() const { return grid_radius_; }
  // The effect being played (`+0x4f37`, 0xff none) and its tick.
  std::uint8_t effect() const { return effect_; }

 private:
  // The `batfx.txt` effect rows: shapes (file numbers) and sounds.
  struct EffectRow {
    std::int16_t shape1 = 0;
    std::int16_t shape2 = 0;
    std::int16_t sfx1 = 0;
    std::int16_t sfx2 = 0;
  };

  BatControlWnd* control() const;
  game::Session* core() const { return wm_.services().game.core(); }
  // `BatModeWnd_init` (0x16120): `batfx.txt`.
  void load_effects();
  // `BatModeWnd_load_resources` (0x18370): the haze tables.
  void load_haze();
  // `BatDisplayItem_set_shape` (0x156c0) of item `i` with `batfx.txt` shape
  // file `file` (`BatModeWnd_set_item_shape` 0x1826c) or the table `shapes`.
  void set_item_shape(Item& item, const ShapeTable& shapes, int frame);
  void set_item_file(Item& item, int file, int frame);
  const ShapeTable& file_shapes(int file) const;
  // `BatDisplayItem_next_frame` (0x12618): returns true after the last
  // frame (back to 0).
  bool next_frame(Item& item);

  // `BatModeWnd_open_star` (0x186b8) / `BatModeWnd_save_star_camera`
  // (0x185cc).
  void open_star();
  void save_star_camera();
  // `BatModeWnd_init_camera` (0x17ff0), `BatModeWnd_reset_camera` (0x17f54),
  // `BatModeWnd_update_clip_planes` (0x181f0), `BatModeWnd_zoom` (0x182b8),
  // `BatModeWnd_rotate_step` (0x17e50).
  void init_camera();
  void reset_camera();
  void update_clip_planes();
  void zoom(int step);
  void rotate_step(int degrees);
  // The orbit step by `angle` degrees (the rotation speed applied), and the
  // camera at `distance` from the target (kept between the focal length
  // and the default distance, as `zoom`).
  void orbit(float angle);
  void set_distance(float distance);
  // `BatModeWnd_project_scene` (0x17430).
  void project_scene();
  // `DrawList_draw` (0x15948) of `list`.
  void draw_list(const std::vector<int>& list);
  // `PolyList_fill` (0x15da4) / `PolyList_outline_all` (0x15e1c).
  void fill_quad(const Quad& quad);
  void outline_quads();
  // `BatModeWnd_draw_grid` (0x1936c), `BatModeWnd_draw_stars` (0x19ae8), the
  // hover caption.
  void draw_grid();
  void draw_stars();
  void draw_caption();
  // `DrawList_pick` (0x15808) and `BatModeWnd_pick_item` (0x17204).
  int pick(const std::vector<int>& list, int x, int y) const;
  void pick_item(int x, int y);
  // `BatModeWnd_pick_grid_point` (0x16fc0), `BatModeWnd_pick_elevation`
  // (0x171d0), window-relative.
  void pick_grid_point(int x, int y);
  void pick_elevation(int y);
  // `BatModeWnd_update_cursor` (0x17260).
  void update_cursor();
  // `BatModeWnd_click_item` (0x16d6c): `left` the left button.
  void click_item(bool left);
  // `BatModeWnd_abandon_selected` (0x172b0).
  void abandon_selected();
  // The BatControlWnd's object kind of item `i` (3 ship, 2 planet, 4 lane, 0).
  int control_kind(int i) const;
  // Activation (event 1) and deactivation (2).
  void activate(int a, int b);
  void deactivate(int a, int b);

  // The effect players (`BatModeWnd_fx_*`); each returns true when done.
  bool fx_move();
  bool fx_self();
  bool fx_projectile();
  bool fx_tractor();
  bool fx_hyperswap();
  bool fx_lane_blocker();
  bool fx_condensor();
  bool fx_remote_repair();
  bool fx_grav_catapult();
  bool fx_lane_magnetron();
  bool fx_transfer();
  bool fx_positron_bouncer();
  bool fx_disperser();
  bool fx_recaller();
  bool fx_self_destruct();
  bool fx_smart_bomb();
  // `BatModeWnd_fx_draw_item` (0x139e0): an item with `kFlagCustomDraw`.
  void fx_draw_item(const Item& item);
  // `BatFx_step_toward` (0x12480): `p` moved `step` toward `target`;
  // returns true (`p` = `target`) when it is that close.
  static bool step_toward(Vec3& p, const Vec3& target, float step);
  // The effect sprite (display item 1, the scene's free one) set up as
  // effect item `slot` at effect position `position`.
  Item& effect_sprite();
  // The acting ship of an effect (the BatControlWnd's selection) and the
  // gizmo of its action slot.
  game::Ship& fx_ship();
  int fx_gizmo();
  // The display item of the ship (`kind` 0), planet (1) or lane (2) at the
  // runtime address `raw`, -1 for none.
  int fx_item_at(game::RawAddress raw) const;
  // The effects moving a crowd of ships (`fx_condensor`, `fx_disperser`):
  // every ship in space at `star` but `except` added as an effect item at
  // the next effect position.
  void fx_add_ships(game::StarIndex star, int except);

  // --- Resources.
  std::uint8_t background_ = 0;            // +0x4f2e BACKGRNDCOLOR
  int cursor_shape_ = 2;                   // +0xb8 CURSORSHAPE
  std::vector<std::string> effect_files_;  // +0x19c6 (batfx.txt shape files)
  std::vector<EffectRow> effect_rows_;     // +0x1a92 / +0x1bce
  std::vector<HazeTable> haze_;            // +0x19be

  // --- The scene.
  std::array<Item, kMaxItems> items_{};  // +0x1d2a
  std::size_t item_count_ = 0;           // +0x1d32
  int cursor_item_ = -1;                 // +0x1d16
  std::vector<int> front_;               // +0x1d36 above the grid
  std::vector<int> back_;                // +0x253a below it
  std::vector<int> backdrops_;           // +0x2d3e
  std::vector<Quad> quads_;              // +0x3542 (64 at most)
  int hovered_ = -1;                     // +0xaf
  int selected_ = -1;                    // +0xb3
  State state_ = State::kIdle;           // +0xb7
  std::int16_t cursor_quad_ = -1;        // +0xbc
  Vec3 move_point_{};                    // +0x1d0a

  // --- The camera (saved).
  std::array<Vec3, kMaxStars> star_positions_{};                      // +0xbe
  std::array<std::array<float, 9>, kMaxStars> star_rotations_;        // +0x56e
  std::array<std::int32_t, kMaxStars> star_visited_{};                // +0x137e
  std::array<Vec3, kMaxStars> star_targets_{};                        // +0x150e
  Vec3 target_{};                                                     // +0x1d1e
  Camera3d camera_;                                                   // +0x4e46
  Vec3 default_position_{};                                           // +0x4ede
  std::array<float, 9> default_rotation_{1, 0, 0, 0, 1, 0, 0, 0, 1};  // +0x4eea
  float near_limit_ = 0;                                              // +0x4f0e
  float far_limit_ = 0;                                               // +0x4f12
  float distance_ = 0;                                                // +0x4f16
  float default_distance_ = 0;                                        // +0x4f1a
  float radius_ = 0;                                                  // +0x4f1e scene radius
  std::int16_t cx_ = 0;                                               // +0x4f22 projection centre
  std::int16_t cy_ = 0;                                               // +0x4f24
  std::int32_t fov_ = 0;                                              // +0x4f26 degrees
  float grid_radius_ = 0;                                             // +0x4f2a
  std::uint32_t actions_ = 0;  // +0x4f2f rotate/zoom bits (1 rotate, 2 right, 4 zoom, 8 out)
  // The toggles (0 / -1): grid, unknown, elevation lines, foot quads,
  // backdrops.
  std::int32_t grid_ = -1;          // +0x503a
  std::int32_t unknown_503e_ = 0;   // +0x503e
  std::int32_t elevation_ = -1;     // +0x5042
  std::int32_t quads_on_ = -1;      // +0x5046
  std::int32_t backdrops_on_ = -1;  // +0x504a

  // --- The effect.
  std::uint8_t effect_ = 0xff;  // +0x4f37
  std::int32_t tick_ = 0;       // +0x4f38
  std::int32_t phase_ = 0;      // +0x4f3c
  std::int32_t fade_ = 0;       // +0x4f40 the translucent sprites' level
  // The condensors' and dispersers' progress (globals 0x8a0c0, 0x8a0c4).
  float condense_ = 0;
  float disperse_ = 0;
  std::array<int, kMaxEffectItems> fx_items_{};  // +0x4f44
  std::array<Vec3, kMaxEffectItems> fx_pos_{};   // +0x4f80
  std::int16_t fx_count_ = 0;                    // +0x5034
  bool skip_ = false;                            // +0x5036
};

}  // namespace oa
