#include "ui/shape_button.h"

#include "ui/winmgr.h"

namespace oa {

void RaceColorButton::draw(int mode) {
  const bool hot = (wm_.hot_window() == id_ && mode != kDrawNotHot) || mode == kDrawHot;
  vfx_pane_wipe(wm_.screen(), pane_, hot ? kHotBackColor : kBackColor);
  if (const Shape* s = shape(frame_)) {
    const GameSession& game = wm_.services().game;
    wm_.draw_colored(pane_, *s, 0, 0, game.race_color(game.player_race()));
  }
}

bool CosWndPlaceholder::handle_event(int type, int a, int b) {
  constexpr int kNewGameScreen = 3;
  if (type == kEventActivate && wm_.current_screen() == kNewGameScreen) {
    active_ = true;
    shown_ = true;
    return false;
  }
  return GWindow::handle_event(type, a, b);
}

}  // namespace oa
