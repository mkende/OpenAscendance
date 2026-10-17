// The window manager on the opening menu and the intro window, on the
// original files with a virtual clock.

#include <catch2/catch_test_macros.hpp>

#include "game_data.h"
#include "ui/flicwnd.h"
#include "ui_rig.h"

namespace oa {
namespace {

// The rig of the window manager tests (ui_rig.h).
using Rig = test::UiRig;

TEST_CASE("the opening menu: text buttons on 0xf2, hot on 0x96", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Rig rig(files);
  CHECK(rig.wm.current_screen() == 0);
  const Image8& screen = rig.gs.screen();
  // Inside the INTRO button (159, 200)-(477, 230), off its text.
  CHECK(screen.at(165, 205) == GWindow::kBackColor);
  // "Play Introduction" in white, centred on y 8 of the pane.
  bool white = false;
  for (int x = 159; x <= 477; ++x) {
    white = white || screen.at(x, 200 + 8 + 5) == 0xf3;
  }
  CHECK(white);
  // Hovering FRESHGAME.
  rig.pass(InputEvent{.type = EventType::kMouseMove, .x = 300, .y = 290});
  CHECK(rig.wm.hot_window() == rig.wm.find_window("FRESHGAME")->id());
  CHECK(screen.at(165, 285) == GWindow::kHotBackColor);
  rig.pass(InputEvent{.type = EventType::kMouseMove, .x = 300, .y = 100});
  CHECK(rig.wm.hot_window() == -1);
  CHECK(screen.at(165, 285) == GWindow::kBackColor);
}

TEST_CASE("the main loop pass lasts at least 3 ticks", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Rig rig(files);
  rig.pass();
  const std::uint32_t t = rig.clock.ticks();
  rig.pass();
  CHECK(rig.clock.ticks() - t == WinMgr::kPassTicks);
}

TEST_CASE("the music picker starts theme 0 on the menu", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Rig rig(files);
  rig.pass();
  CHECK(rig.sound.tune_playing());
  CHECK(rig.sound.current_tune() == 0);
}

TEST_CASE("Exit to DOS and Alt+X quit", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  {
    Rig rig(files);
    rig.click(300, 450);
    CHECK_FALSE(rig.wm.running());
  }
  {
    Rig rig(files);
    rig.pass(InputEvent{.type = EventType::kKeyDown, .key = 'x', .mods = 0x0100 /* SDL_KMOD_LALT */});
    CHECK_FALSE(rig.wm.running());
  }
}

TEST_CASE("Play Introduction runs the intro with its subtitles, a click goes back", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Rig rig(files);
  rig.click(318, 215);
  REQUIRE(rig.wm.current_screen() == 6);
  CHECK_FALSE(rig.pointer);
  const auto* intro = dynamic_cast<FlicWnd*>(rig.wm.find_window("INTROFLIC"));
  REQUIRE(intro != nullptr);
  REQUIRE(intro->flic() != nullptr);
  // Until frame 59: the first subtitle shows from frame 23, its colours
  // fading in by 2 per frame up to subfont.pal (the first frame's palette
  // fade, steps 32..63 one per frame, also rewrites them until frame 32).
  while (intro->flic()->frame() < 59) {
    rig.pass();
  }
  CHECK(intro->flic()->frame_rate() == 12);
  CHECK(rig.gs.dac().dac[254 * 3] == rig.gs.shadow().dac[254 * 3]);
  CHECK(rig.gs.dac().dac[254 * 3] > 0);
  // The tune of frame 20, intro00.
  CHECK(rig.sound.current_tune() == 6);
  // The subtitle box's pixels use the font's colours.
  bool subtitle = false;
  for (int x = 0; x < 640; ++x) {
    subtitle = subtitle || rig.gs.screen().at(x, 379 + 20) >= 240;
  }
  CHECK(subtitle);
  rig.click(10, 10);
  rig.pass();
  CHECK(rig.wm.current_screen() == 0);
  CHECK(rig.pointer);
  CHECK_FALSE((rig.sound.tune_playing() && rig.sound.current_tune() == 6));
}

}  // namespace
}  // namespace oa
