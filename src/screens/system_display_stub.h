// A stand-in for the System Display (screen 16, `BatControlWnd`,
// `windows.txt` TYPE 19) until the battle screen is written: entering the
// screen on `Cosmos::selected_star` (set by whoever posts message 1 to
// screen 16: the star map, the planet list, a game event's "Go to System")
// resolves the battle there as the Auto button does (`BattleControl`
// `open`, `start_auto`, `finish_all`, `close`) when the player and a
// species at war with the player are present; the player leaves the screen
// (Esc, an exit corner) as the real one.
//
// The battle screen replaces it by adding its own TYPE 19 class to the
// factory after `add_system_display_stub`.
#pragma once

#include "ui/gwindow.h"
#include "ui/winmgr.h"

namespace oa {

class SystemDisplayStub final : public GWindow {
 public:
  using GWindow::GWindow;

  // Event 1 (the screen entered): the battle resolved.
  bool handle_event(int type, int a, int b) override;
  void draw(int /*mode*/) override {}
  void draw_hilite(int /*mode*/) override {}
};

// Adds `SystemDisplayStub` (TYPE 19) to `factory`.
void add_system_display_stub(WindowFactory& factory);

}  // namespace oa
