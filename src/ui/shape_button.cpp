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

}  // namespace oa
