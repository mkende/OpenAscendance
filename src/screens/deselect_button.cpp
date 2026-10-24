#include "screens/deselect_button.h"

#include <array>
#include <cstdint>
#include <format>
#include <memory>
#include <string>

#include "core/error.h"
#include "screens/bat_mode_wnd.h"
#include "ui/remake_help.h"
#include "ui/winmgr.h"

namespace oa {

namespace {

constexpr int kScreenSystem = 16;
// The click sound of the panel's buttons (`soundfx.txt` 0, SOUNDCHANNEL 0).
constexpr int kClickSound = 0;
// The width of the stone recess around a slot of the panel.
constexpr int kRecess = 4;

// The icon, 31 by 31 in `game.pal`'s colours (`kIconColors`; '.' the
// background): the white selection marker crossed out in red, on its
// yellow stalk over the blue system grid (RECENTER's).
constexpr std::array<std::string_view, 31> kIcon = {
    "...............................",  //
    ".......ooooooooooooooooo.......",  //
    ".......oxxx.........xxxo.......",  //
    ".......oxaxxx.....xxxaxo.......",  //
    ".......oxxabxx...xxbaxxo.......",  //
    ".......o.xxxbxxxxxbxxx.o.......",  //
    ".......o...xxbxxabxx...o.......",  //
    ".......o....xxxaxxx....o.......",  //
    ".......o...xxbaxxbxx...o.......",  //
    ".......o.xxxbxxxxxbxxx.o.......",  //
    ".......oxxabxx...xxbaxxo.......",  //
    ".......oxaxxx.....xxxaxo.......",  //
    ".......oxxx.........xxxo.......",  //
    ".......ooooooooooooooooo.......",  //
    "...............c...............",  //
    "...........ddeeceedd...........",  //
    "..........efe..c..efe..........",  //
    ".....deeeeeeeeeceeeeeeeeed.....",  //
    "....ddd.dd..e..c..e..dd.ddd....",  //
    "..deeeeeeeeeeeeceeeeeeeeeeeed..",  //
    "..dd.dd.dd..e..c..e..dd.dd.dd..",  //
    ".deeeeeeeeeeeeeceeeeeeeeeeeeed.",  //
    "..dd.e..e..de..c..ed..e..e.dd..",  //
    "..e.dd.dd..dde.c.edd..dd.dd.e..",  //
    ".deeeeeeeeeeeeeceeeeeeeeeeeeed.",  //
    "....dd.dd..dd..e..dd..dd.dd....",  //
    "....d..e...e...e...e...e..d....",  //
    ".....deeeeeeeeeeeeeeeeeeed.....",  //
    ".......d...e...e...e...d.......",  //
    "...........d...d...d...........",  //
    "...............................",  //
};

// The palette index of an icon character.
constexpr std::uint8_t icon_color(char c) {
  switch (c) {
    case 'o':
      return 15;  // white
    case 'x':
      return 115;  // dark red
    case 'a':
      return 119;  // orange
    case 'b':
      return 118;  // red
    case 'c':
      return 144;  // yellow-green
    case 'd':
      return 97;  // dark blue
    case 'e':
      return 98;  // blue
    case 'f':
      return 96;  // deep blue
    default:
      return GWindow::kBackColor;
  }
}

GWindow& require_window(WinMgr& wm, std::string_view name) {
  GWindow* w = wm.find_window(name);
  if (w == nullptr) {
    throw DataError(std::format("windows.txt: no {} on the System Display", name));
  }
  return *w;
}

// Copies the `kRecess` pixels around `from` to around `to` (the same size).
void copy_recess(Image8& screen, const Pane& from, const Pane& to) {
  const int dx = to.x0 - from.x0;
  const int dy = to.y0 - from.y0;
  for (int y = from.y0 - kRecess; y <= from.y1 + kRecess; ++y) {
    for (int x = from.x0 - kRecess; x <= from.x1 + kRecess; ++x) {
      const bool inside = x >= from.x0 && x <= from.x1 && y >= from.y0 && y <= from.y1;
      if (!inside && screen.contains(x, y) && screen.contains(x + dx, y + dy)) {
        screen.set(x + dx, y + dy, screen.at(x, y));
      }
    }
  }
}

}  // namespace

DeselectButton::DeselectButton(WinMgr& wm, const Pane& pane) : GWindow(wm, std::string(kName), pane) {
  mouse_focus_ = true;
  sound_ = kClickSound;
  help_ = "oa_deselect";
}

bool DeselectButton::handle_event(int type, int a, int b) {
  switch (type) {
    case kEventLeftClick:
    case kEventRightClick:
      if (!contains(a, b)) {
        return false;
      }
      if (auto* view = dynamic_cast<BatModeWnd*>(wm_.find_window("BatModeWnd")); view != nullptr && !greyed()) {
        view->deselect();
      }
      return true;
    case kEventHelpClick:
      if (!contains(a, b)) {
        return false;
      }
      show_remake_help(wm_, help_);
      return true;
    default:
      return GWindow::handle_event(type, a, b);
  }
}

void DeselectButton::draw(int mode) {
  if (!active_) {
    return;
  }
  const bool hot = ((wm_.hot_window() == id_ && mode != kDrawNotHot) || mode == kDrawHot) && !greyed();
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, pane_, hot ? kHotBackColor : kBackColor);
  for (int y = 0; y < static_cast<int>(kIcon.size()) && pane_.y0 + y <= pane_.y1; ++y) {
    const std::string_view row = kIcon[static_cast<std::size_t>(y)];
    for (int x = 0; x < static_cast<int>(row.size()) && pane_.x0 + x <= pane_.x1; ++x) {
      if (const char c = row[static_cast<std::size_t>(x)]; c != '.') {
        screen.set(pane_.x0 + x, pane_.y0 + y, icon_color(c));
      }
    }
  }
  if (greyed()) {
    grey_out(kBackColor);
  }
}

void install_deselect_button(WinMgr& wm) {
  const Pane column = require_window(wm, "TOGGRID").pane();
  const Pane recenter = require_window(wm, "RECENTER").pane();
  const Pane pane{column.x0, recenter.y0, column.x1, recenter.y1};
  const int id = wm.register_window(std::make_unique<DeselectButton>(wm, pane));
  wm.set_enter_handler(kScreenSystem, [&wm, id, recenter, pane, on_screen = false](int screen) mutable {
    wm.draw_background(screen);
    const bool shown = !wm.replaying();
    if (shown != on_screen) {
      if (shown && !wm.add_to_screen(id, kScreenSystem)) {
        throw DataError("the System Display's window list is full");
      }
      if (!shown) {
        wm.remove_from_screen(id, kScreenSystem);
      }
      on_screen = shown;
    }
    if (shown) {
      copy_recess(wm.screen(), recenter, pane);
    }
  });
}

}  // namespace oa
