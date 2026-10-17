// Tests: the help files and the help window (help mode, Shift + click).

#include <SDL3/SDL_keycode.h>

#include <catch2/catch_test_macros.hpp>

#include "assets/help_txt.h"
#include "core/error.h"
#include "game_data.h"
#include "ui/help_wnd.h"
#include "ui_rig.h"

namespace oa {
namespace {

TEST_CASE("help.txt: a topic's texts, pages and button", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const std::vector<std::uint8_t> help = files.load("help.txt");
  const HelpTopic t = parse_help_topic(help, "TOGCOL", "help.txt", 167, 102);
  REQUIRE(t.element_count == 4);
  CHECK(t.pages == 1);
  const HelpElement& title = t.elements[0];
  CHECK(title.kind == HelpElement::Kind::kText);
  CHECK(title.text == "Star Information Toggle ");
  CHECK(title.flags == kFontCenterH);
  CHECK(title.y == 15);
  CHECK(t.elements[1].x == 5);
  CHECK(t.elements[1].y == 50);
  CHECK(t.elements[1].page_end);
  CHECK(t.elements[1].text.find("species. \n\nFour dots") != std::string::npos);
  CHECK_FALSE(t.elements[2].page_end);
  CHECK(t.buttons[0].shown);
  CHECK(t.buttons[0].title == "OK");
  CHECK(t.buttons[0].message == 0);
  CHECK_FALSE(t.buttons[1].shown);
  CHECK_THROWS_AS(parse_help_topic(help, "no-such-topic", "help.txt", 0, 0), DataError);
}

TEST_CASE("help mode: Shift + click opens the button's topic, a right click closes it", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::UiRig rig(files);
  const Image8 before = rig.gs.screen();
  rig.pass(InputEvent{.type = EventType::kKeyDown, .key = SDLK_LSHIFT, .mods = SDL_KMOD_LSHIFT});
  CHECK(rig.wm.help_mode());
  CHECK(rig.pointer_frame == kPointerHelp);
  rig.move(300, 295);  // FRESHGAME

  // Snapshots of the modal passes; a modal loop without input left ends.
  auto* help = dynamic_cast<HelpWnd*>(rig.wm.find_window("HELPWINDOW"));
  REQUIRE(help != nullptr);
  int modal_passes = 0;
  bool stuck = false;
  std::string topic;
  Image8 shown;
  const std::function<void()> present = rig.wm.services().present;
  rig.wm.services().present = [&] {
    present();
    if (rig.wm.in_modal()) {
      ++modal_passes;
      topic = help->topic();
      shown = rig.gs.screen();
      if (rig.pending.empty()) {
        stuck = true;
        rig.wm.end_modal(-1);
      }
    }
  };
  rig.pending.push_back(InputEvent{.type = EventType::kMouseDown, .x = 300, .y = 295, .button = MouseButton::kLeft});
  rig.pending.push_back(InputEvent{.type = EventType::kMouseUp, .x = 300, .y = 295, .button = MouseButton::kLeft});
  rig.pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = SDLK_LSHIFT, .mods = 0});
  rig.queue_click(300, 295, MouseButton::kRight);
  rig.pending.push_back(InputEvent{.type = EventType::kMouseMove, .x = 301, .y = 295});
  rig.drain();
  CHECK_FALSE(stuck);
  CHECK(modal_passes >= 2);
  CHECK(topic == "newgame");
  CHECK_FALSE(rig.wm.in_modal());
  CHECK_FALSE(rig.wm.help_mode());
  CHECK(rig.wm.current_screen() == 0);
  // The window opened at x 80 on the opening menu, its OK button along the
  // bottom; the screen under it came back.
  CHECK(help->pane() == Pane{80, 120, 80 + HelpWnd::kWidth, 375});
  // (HELPBUTTON0, renamed: find_window would see HELPBUTTON1 first now.)
  REQUIRE_FALSE(help->children().empty());
  const GWindow* ok = help->children()[0];
  CHECK(ok->name() == "OK");
  CHECK(ok->pane() == Pane{88, 120 + 0xd9, 80 + 0x147, 120 + 0xf7});
  CHECK(shown.at(85, 125) != before.at(85, 125));
  CHECK(rig.gs.screen().at(85, 125) == before.at(85, 125));
}

}  // namespace
}  // namespace oa
