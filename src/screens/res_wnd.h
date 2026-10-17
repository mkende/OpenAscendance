// The research screen (screen 20, `reswin.cpp`): `ResWnd` (TYPE 9 RESWIN,
// size 0x9dd, vtable 0x85e98), the research tree as rings in 3D around a
// vertical axis, the info column of the hovered item (or of the current
// project) and the title bar; its children RESROTLEFT/RIGHT and
// RESMOVEUP/DOWN are plain buttons sending messages 50..53
// (`docs/11-screens.md`, "Research screen").
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "screens/camera3d.h"
#include "ui/gwindow.h"
#include "ui/winmgr.h"

namespace oa::game {
struct Session;
}  // namespace oa::game

namespace oa {

class ResWnd final : public GWindow {
 public:
  static constexpr std::size_t kMaxItems = 100;
  static constexpr std::uint16_t kNone = 0xffff;
  // The messages of the window: the buttons' (p1 = 4 left / 5 right
  // button), the end of a held button (p1 = the motion bits to clear) and
  // the timer.
  enum Message : int {
    kRotateLeft = 50,
    kRotateRight = 51,
    kMoveUp = 52,
    kMoveDown = 53,
    kStopMotion = 0x38,
    kTimer = 0x3a,
  };
  // The motion flags (`0xf64ac`).
  enum Motion : std::uint32_t { kMoving = 1, kMovingDown = 2, kRotating = 4, kRotatingRight = 8 };
  // The window-relative centre of the projection.
  static constexpr std::int32_t kCenterX = 235;
  static constexpr std::int32_t kCenterY = 240;

  // `ResWnd_ctor` 0x443a0 / `ResWnd_init` 0x4250c: the camera (30 degree
  // field of view, at (0, 0, -150)); the tree is laid out at the first use
  // of a game (`layout_tree`).
  ResWnd(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  // `ResWnd_handle_event` 0x42a84.
  bool handle_event(int type, int a, int b) override;
  // `ResWnd_draw` 0x4331c: the tree, then the info column and the title bar
  // when they are marked for redrawing.
  void draw(int mode) override;
  // `ResWnd_serialize` 0x44374: the camera (0x98 bytes) in the saves'
  // window state.
  bool save_state(std::span<std::uint8_t> out) const override;
  void load_state(std::span<const std::uint8_t> in) override;
  // "Begin New Game": the camera's orientation back to the identity.
  void new_game() override;

  // The tree layout of `ResWnd_init` without `reswin.txt`: the level of an
  // item is 0 without prerequisites, else 1 + the highest level of its
  // prerequisites; the items of a level go around the axis (see the doc).
  // `prereqs[i]` are item i's (0xff-terminated) prerequisites.
  static std::vector<Vec3> layout_tree(const std::vector<std::array<std::uint8_t, 5>>& prereqs);

  const Camera3d& camera() const { return camera_; }
  Camera3d& camera() { return camera_; }
  const std::array<Vec3, kMaxItems>& positions() const { return pos_; }
  // The projected positions (window-relative) and view depths of the last
  // draw, and the draw order (far first).
  ScreenPoint screen_position(std::size_t item) const { return screen_[item]; }
  float depth(std::size_t item) const { return depth_[item]; }
  const std::array<std::uint16_t, kMaxItems>& draw_order() const { return order_; }
  std::uint16_t hovered() const { return hovered_; }
  std::uint32_t motion() const { return motion_; }

 private:
  game::Session* core() const;
  // Lays the tree out when a game is there.
  void sync_with_game();
  // `ResWnd_update_hover` 0x428e8: the front-most visible item within 24
  // pixels of the pointer on both axes.
  void update_hover();
  // `ResWnd_compare_depth` 0x432c4 + the projection loop of the draw.
  void project_items();
  void draw_tree();
  void draw_info();
  void draw_title();
  // A button message: toggles the motion bits (`docs/11-screens.md`).
  void toggle_motion(int message, int button);

  Camera3d camera_;
  std::array<Vec3, kMaxItems> pos_{};             // +0x143
  std::array<ScreenPoint, kMaxItems> screen_{};   // +0x5f3, +0x6bb
  std::array<float, kMaxItems> depth_{};          // +0x783
  std::array<std::uint16_t, kMaxItems> order_{};  // +0x913
  std::uint16_t hovered_ = kNone;                 // +0x9db
  bool laid_out_ = false;
  // The module's globals: redraw the title (`0xf64a0`), the info column
  // (`0xf64a4`), the tree (`0xf64a8`); the motion flags (`0xf64ac`).
  bool redraw_title_ = false;
  bool redraw_info_ = false;
  bool redraw_tree_ = false;
  std::uint32_t motion_ = 0;
};

// Adds `ResWnd` (TYPE 9) to `factory`.
void add_research_windows(WindowFactory& factory);

}  // namespace oa
