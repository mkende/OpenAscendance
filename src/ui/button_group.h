// The button group (`gwindow.cpp`, class `ButtonGroup`, `windows.txt`
// TYPE 3, ctor 0x2d0e4, vtable 0x85c70): a row of small toggle controls in
// one shape window, the star map's RACECONTROLS (one per race slot, id
// `1 << race`).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ui/gwindow.h"

namespace oa {

class ButtonGroup final : public GWindow {
 public:
  // A control's flags (`+0xa`).
  enum Flags : int { kEnabled = 1, kHover = 2, kOn = 4 };

  // A control (14 bytes in the original: id, x0, x1, y0, y1, flags,
  // frame), in screen coordinates.
  struct Control {
    int id = 0;
    int x0 = 0;
    int x1 = 0;
    int y0 = 0;
    int y1 = 0;
    int flags = 0;
    int frame = 0;
  };

  // The races of the start-up galaxy (`Cosmos_generate(50, 7)` before the
  // windows load), whose controls the loader enables.
  static constexpr int kStartupRaces = 7;

  // Reads NUMCONTROLS and the RACEnCONTROL records (`id x0 y0 x1 y1 flags
  // frame`) of `record`.
  ButtonGroup(WinMgr& wm, const WindowRecord& record, const ShapeTable* shapes);

  const std::vector<Control>& controls() const { return controls_; }
  int hover() const { return hover_; }

  void move(int dx, int dy) override;
  // `ButtonGroup_handle_event` (0x2d588): 1 sets the frames from the races'
  // colours, 4/5 toggle the control hit and send SENDMESSAGE (control id) to
  // the parent, 7/6 start and stop the mouse-move notifications, 8 tracks
  // the hovered control, 9 sets the flags of control `a` (-1: all) to `b`.
  bool handle_event(int type, int a, int b) override;
  // `ButtonGroup_draw` (0x2d7a4): the background frame, then each control's
  // frame offset by its state (disabled +4, enabled +0, hovered +1, on +2,
  // on and hovered +3).
  void draw(int mode) override;
  void draw_hilite(int mode) override { draw(mode); }
  // `ButtonGroup_serialize` (0x2d8d4): the controls' 14-byte records (id,
  // x0, x1, y0, y1, flags, frame as i16).
  bool save_state(std::span<std::uint8_t> out) const override;
  void load_state(std::span<const std::uint8_t> in) override;
  // `ButtonGroup_update_race_frames` (0x2d490) at "Begin New Game".
  void new_game() override { update_race_frames(); }

  // The size of a control's record in the saves.
  static constexpr std::size_t kControlRecordSize = 14;

 private:
  // `ButtonGroup_update_race_frames` (0x2d490): each race's control in its
  // colour (frames `colour * 5 + 20` ..).
  void update_race_frames();
  // `ButtonGroup_set_control` (0x2d1e8): the flags of the control `id`.
  void set_flags(int id, int flags);
  // `ButtonGroup_hit_test` (0x2d304): the control under (x, y), -1 for none.
  int hit_test(int x, int y) const;
  // `ButtonGroup_set_hover` (0x2d3c0).
  void set_hover(int index);
  // `ButtonGroup_toggle_control` (0x2d4d8): flips an enabled control.
  bool toggle(int index);

  std::vector<Control> controls_;
  int hover_ = -1;
};

}  // namespace oa
