// The remake's deselect button on the System Display (screen 16, not in
// the original): in the panel's free slot, left of RECENTER, an icon of the
// selection marker crossed out over the system grid. A click drops the
// view's selection (`BatModeWnd::deselect`); greyed while there is none.
#pragma once

#include <string_view>

#include "ui/gwindow.h"

namespace oa {

class DeselectButton final : public GWindow {
 public:
  // The window's name (`WinMgr::find_window`).
  static constexpr std::string_view kName = "OADESELECT";

  DeselectButton(WinMgr& wm, const Pane& pane);

  bool handle_event(int type, int a, int b) override;
  // As ABANDON's (`RaceColorButton::draw`): the pane wiped with 0x96 when
  // hot (0xf2 otherwise), then the icon.
  void draw(int mode) override;
  void draw_hilite(int /*mode*/) override { draw(kDrawHot); }
};

// Adds the deselect button to the System Display's screen: its slot is
// TOGGRID's column on RECENTER's row, its stone recess the one around
// RECENTER, drawn over the background when the screen is entered. Neither
// while a trace is replayed. Throws `DataError` when one of these windows
// is missing.
void install_deselect_button(WinMgr& wm);

}  // namespace oa
