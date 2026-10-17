// The generic window (`gwindow.cpp`, class `GWindow`, ctor 0x2bf50, init
// 0x2c004, vtable 0x85ca0): buttons and static panes from `windows.txt`, the
// base of every window class (`docs/07-ui-framework.md`, "Window classes").
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/shp.h"
#include "assets/windows_txt.h"
#include "gfx/vfx.h"

namespace oa {

class WinMgr;

// Window event types (`docs/07-ui-framework.md`, "Event types").
enum WindowEvent : int {
  kEventActivate = 1,
  kEventDeactivate = 2,
  kEventKey = 3,
  kEventLeftClick = 4,
  kEventRightClick = 5,
  kEventMouseLeave = 6,
  kEventMouseEnter = 7,
  kEventMouseMove = 8,
  kEventTick = 10,
  kEventHelpClick = 0xc,
  kEventShow = 0xe,
  kEventHide = 0xf,
  kEventOk = 201,
  kEventCancel = 202,
};

// The `draw` modes: 0 normal, 2 not hot, 3 hot.
enum DrawMode : int { kDrawNormal = 0, kDrawNotHot = 2, kDrawHot = 3 };

// The value of SENDMESSAGE / SENDPARAM1 / SENDPARAM2 replaced by the event
// type (4 left, 5 right).
inline constexpr int kParamMouseButton = 999;

class GWindow {
 public:
  // The colours of a text window's background: normal and hot (0xf2 and
  // 0x96, also the WMGR_MAPFOCUS remap).
  static constexpr std::uint8_t kBackColor = 0xf2;
  static constexpr std::uint8_t kHotBackColor = 0x96;

  // `GWindow_init` (0x2c004) from `record`: the shape table is the one of the
  // record's first screen (`shapes`, may be null for none).
  GWindow(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);
  virtual ~GWindow() = default;
  GWindow(const GWindow&) = delete;
  GWindow& operator=(const GWindow&) = delete;

  // vtable slot 2, `GWindow_handle_event` (0x2eb40): returns true when the
  // event is consumed (-1 in the original).
  virtual bool handle_event(int type, int a, int b);
  // vtable slot 3, `GWindow_draw` (0x2c444).
  virtual void draw(int mode);
  // vtable slot 4, `GWindow_draw_hilite` (0x2c648): the hot look.
  virtual void draw_hilite(int mode);

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }
  const std::string& name() const { return name_; }
  const Pane& pane() const { return pane_; }
  bool shown() const { return shown_; }
  bool active() const { return active_; }
  bool mouse_focus() const { return mouse_focus_; }
  int parent() const { return parent_; }
  void set_parent(int id) { parent_ = id; }
  const std::vector<GWindow*>& children() const { return children_; }
  void add_child(GWindow* child) { children_.push_back(child); }
  // Whether (x, y) is inside the window's rectangle.
  bool contains(int x, int y) const { return x >= pane_.x0 && x <= pane_.x1 && y >= pane_.y0 && y <= pane_.y1; }

 protected:
  // Sends `type` to the children (`GWindow_broadcast_event`).
  void broadcast_to_children(int type, int a, int b);
  // `GWindow_draw_items` (0x2c814).
  void draw_items(bool hot);
  // `GWindow_draw_children` (0x2c938).
  void draw_children();
  // The text path of `draw` / `draw_hilite`, on `background`.
  void draw_text(std::uint8_t background);

  WinMgr& wm_;
  Pane pane_;
  const ShapeTable* shapes_;
  int frame_;
  std::string name_;
  bool shown_ = false;
  bool active_ = false;
  bool mouse_focus_;
  int id_ = 0;
  int message_;
  int param1_;
  int param2_;
  std::string help_;
  int sound_;
  int text_x_ = -1;
  int text_y_ = -1;
  bool border_ = false;
  int parent_ = 0;  // 0: none
  std::vector<GWindow*> children_;
  std::vector<WindowItem> items_;
};

}  // namespace oa
