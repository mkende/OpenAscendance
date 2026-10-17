// The window toolkit on the original files: windows.txt's classes, modal
// dialogs, the exit corners and the history, the hotkeys, the list box's
// auto-repeat, the toggle buttons, ascend.cfg, static.txt and the Alt+G GIF.

#include <SDL3/SDL_keycode.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <vector>

#include "assets/gif.h"
#include "assets/static_strings.h"
#include "game_data.h"
#include "ui/button_group.h"
#include "ui/digit_counter.h"
#include "ui/flicwnd.h"
#include "ui/help_wnd.h"
#include "ui/list_box.h"
#include "ui/shape_button.h"
#include "ui/text_box.h"
#include "ui/toggle_button.h"
#include "ui_rig.h"

namespace oa {
namespace {

using test::UiRig;

// An unused screen for the windows the tests make.
constexpr int kTestScreen = 5;

// Records the events its children send it.
class EventLog final : public GWindow {
 public:
  using GWindow::GWindow;
  struct Event {
    int type;
    int a;
    int b;
  };
  bool handle_event(int type, int a, int b) override {
    if (type >= kEventHelpButton) {
      events.push_back({type, a, b});
      return true;
    }
    return GWindow::handle_event(type, a, b);
  }
  void draw(int /*mode*/) override {}
  std::vector<Event> events;
};

TEST_CASE("windows.txt: the 124 windows in their classes and screens", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  UiRig rig(files);
  int count = 0;
  while (rig.wm.window(count + 1) != nullptr) {
    ++count;
  }
  CHECK(count == 124);
  const auto is = [&]<typename T>(std::string_view name) {
    INFO(name);
    return dynamic_cast<T*>(rig.wm.find_window(name)) != nullptr;
  };
  CHECK(is.operator()<FlicWnd>("INTROFLIC"));
  CHECK(is.operator()<ButtonGroup>("RACECONTROLS"));
  for (const std::string_view list : {"RACELIST", "LOADLIST", "GIZLIST", "STATPLANLST", "PLAYERDIP", "INTELLIST",
                                      "BatListWnd", "PLANLIST", "STATSHIPLST", "DBGWNDLIST"}) {
    CHECK(is.operator()<ListBox>(list));
  }
  CHECK(is.operator()<DigitCounter>("TURNCOUNT"));
  CHECK(is.operator()<HelpWnd>("HELPWINDOW"));
  for (const std::string_view toggle :
       {"TOGLANES", "TOGEMPIRE", "TOGSHIPS", "TOGCOLONIES", "NEXTTURNCONT", "TOGGRID", "TOGELEV"}) {
    CHECK(is.operator()<ToggleButton>(toggle));
  }
  CHECK(is.operator()<TextBox>("ALIENDIP"));
  CHECK(is.operator()<RaceColorButton>("ABANDON"));
  CHECK(is.operator()<CosWndPlaceholder>("COSMOSWnd"));

  const auto* group = dynamic_cast<ButtonGroup*>(rig.wm.find_window("RACECONTROLS"));
  REQUIRE(group->controls().size() == 7);
  CHECK(group->controls()[1].id == 2);
  CHECK(group->controls()[1].x0 == 501);
  CHECK(group->controls()[1].y0 == 449);
  CHECK(group->controls()[1].x1 == 517);
  CHECK(group->controls()[1].y1 == 465);
  CHECK(group->controls()[1].flags == ButtonGroup::kEnabled);
  CHECK(group->parent() == rig.wm.find_window("COSMOSWnd")->id());
  CHECK(dynamic_cast<ToggleButton*>(rig.wm.find_window("TOGLANES"))->on());
  CHECK_FALSE(dynamic_cast<ToggleButton*>(rig.wm.find_window("TOGEMPIRE"))->on());
}

TEST_CASE("static.txt: the strings by index", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const std::vector<std::string> s = parse_static_strings(files.load("static.txt"));
  REQUIRE(s.size() > 0x1e);
  CHECK(s[0] == "(depleted)");
  CHECK(s[5] == "\nOwned by %s");
  CHECK(s[0x10] == "<EMPTY>");
  CHECK(s[0x1e] == "OK");
}

TEST_CASE("the message box runs modally until its OK button, the screen restored", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  UiRig rig(files);
  // Off the buttons: the hot window loses its look when the dialog opens.
  rig.move(10, 10);
  const Image8 before = rig.gs.screen();
  // A click outside does nothing; the OK button (260..290) ends the loop.
  rig.queue_click(100, 400);
  rig.queue_click(275, 275);
  rig.wm.message_box("Hello");
  CHECK_FALSE(rig.wm.in_modal());
  // The button acts on the press: its release is left.
  CHECK(rig.pending.size() == 1);
  rig.drain();
  CHECK(rig.gs.screen().pixels()[0] == before.pixels()[0]);
  CHECK(std::ranges::equal(rig.gs.screen().pixels(), before.pixels()));
  // The menu works again (it was hidden during the loop).
  CHECK(rig.wm.find_window("FRESHGAME")->shown());
  rig.click(300, 450);
  CHECK_FALSE(rig.wm.running());
}

TEST_CASE("the input box: the first key replaces the text, Enter returns 3, Esc 0", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  UiRig rig(files);
  std::string name = "Old";
  rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_N});
  rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_E});
  rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_W});
  rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_BACKSPACE});
  rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_RETURN});
  CHECK(rig.wm.input_box("Name", name, 12) == 3);
  CHECK(name == "ne");
  rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_ESCAPE});
  CHECK(rig.wm.input_box("Name", name, 12) == 0);
  CHECK(rig.wm.current_screen() == 0);
}

TEST_CASE("exit corners: the pointer changes and a click goes back; none on the menu", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  UiRig rig(files);
  rig.move(639, 479);
  CHECK(rig.wm.exit_corner() == -1);
  rig.wm.set_screen(10, true);
  rig.move(639, 478);
  CHECK(rig.wm.exit_corner() == -1);
  rig.move(639, 479);
  CHECK(rig.wm.exit_corner() == 3);
  CHECK(rig.pointer_frame == kPointerExitCorner + 3);
  rig.move(0, 0);
  CHECK(rig.pointer_frame == kPointerExitCorner);
  rig.click(0, 0);
  CHECK(rig.wm.current_screen() == 0);
  CHECK(rig.pointer_frame == kPointerNormal);
}

TEST_CASE("the history: Esc and message 3 go back, Alt+S / Alt+L push", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  UiRig rig(files);
  rig.key(SDLK_S, SDL_KMOD_LALT);
  CHECK(rig.wm.current_screen() == 2);
  rig.key(SDLK_L, SDL_KMOD_LALT);
  CHECK(rig.wm.current_screen() == 4);
  CHECK(rig.wm.history() == std::vector<int>{0, 2});
  rig.key(SDLK_ESCAPE);
  CHECK(rig.wm.current_screen() == 2);
  rig.wm.post_message(kMessageBack, 0, 0);
  rig.pass();
  CHECK(rig.wm.current_screen() == 0);
  rig.key(SDLK_ESCAPE);
  CHECK(rig.wm.current_screen() == 0);
  // The ring keeps the last 16 screens.
  for (int i = 0; i < 20; ++i) {
    rig.wm.set_screen(i % 2 == 0 ? 10 : 18, true);
  }
  CHECK(rig.wm.history().size() == WinMgr::kHistory);
}

TEST_CASE("Alt+M and Alt+N mute, ascend.cfg keeps them", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "oa_widgets_cfg";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    UiRig rig(files, nullptr, dir);
    const bool music = rig.sound.music_muted();
    rig.key(SDLK_M, SDL_KMOD_LALT);
    rig.key(SDLK_N, SDL_KMOD_LALT);
    CHECK(rig.sound.music_muted() != music);
    rig.sound.set_music_muted(true);
    rig.sound.set_sfx_muted(false);
    rig.wm.set_rotation_speed(1.5F);
    rig.wm.save_config();
  }
  std::ifstream in(dir / "ascend.cfg", std::ios::binary);
  std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(data.size() == 12);
  {
    UiRig rig(files, nullptr, dir);
    CHECK(rig.sound.music_muted());
    CHECK_FALSE(rig.sound.sfx_muted());
    CHECK(rig.wm.rotation_speed() == 1.5F);
  }
  std::filesystem::remove_all(dir);
}

TEST_CASE("Alt+G writes the screen as scr000.gif", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "oa_widgets_gif";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  UiRig rig(files, nullptr, dir);
  rig.pass();
  rig.key(SDLK_G, SDL_KMOD_LALT);
  std::ifstream in(dir / "scr000.gif", std::ios::binary);
  const std::vector<std::uint8_t> gif((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const Image8 decoded = decode_gif(gif);
  CHECK(std::ranges::equal(decoded.pixels(), rig.gs.screen().pixels()));
  // The global table holds the DAC scaled to 8 bits.
  CHECK(gif[13 + 3 * 0xf3] == rig.gs.dac().dac[3 * 0xf3] * 255 / 63);
  std::filesystem::remove_all(dir);
}

TEST_CASE("a toggle button flips and tells its parent", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  UiRig rig(files);
  rig.wm.set_screen(1, true);
  auto* lanes = dynamic_cast<ToggleButton*>(rig.wm.find_window("TOGLANES"));
  REQUIRE(lanes != nullptr);
  // Children get the clicks from the broadcast (not the activation).
  REQUIRE(lanes->on());
  rig.click(490, 410);
  CHECK_FALSE(lanes->on());
  rig.click(490, 410);
  CHECK(lanes->on());
}

TEST_CASE("the list box: hovered row, pick message, arrows repeating faster while held", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  UiRig rig(files);
  auto parent = std::make_unique<EventLog>(rig.wm, "PARENT", Pane{100, 100, 300, 200});
  EventLog& log = *parent;
  const int parent_id = rig.wm.register_window(std::move(parent));
  WindowRecord record;
  record.type = 10;
  record.name = "TESTLIST";
  record.x0 = 100;
  record.y0 = 100;
  record.x1 = 300;
  record.y1 = 200;
  record.shape_frame = 7;
  auto list_window = std::make_unique<ListBox>(rig.wm, record, nullptr);
  ListBox& list = *list_window;
  list.set_parent(parent_id);
  const int list_id = rig.wm.register_window(std::move(list_window));
  log.add_child(&list);
  REQUIRE(rig.wm.add_to_screen(parent_id, kTestScreen));
  REQUIRE(rig.wm.add_to_screen(list_id, kTestScreen));
  for (int i = 0; i < 20; ++i) {
    list.add_item(std::format("Row {}", i));
  }
  CHECK(list.scroll_bar());
  rig.wm.set_screen(kTestScreen, true);
  CHECK(list.frame() == 0);

  // Rows are the font height + 3 = 14 pixels from y 101.
  rig.move(150, 101 + 14 * 2 + 5);
  CHECK(list.hover_row() == 2);
  rig.click(150, 101 + 14 * 2 + 5);
  REQUIRE(log.events.size() == 1);
  CHECK(log.events[0].type == kEventListPick);
  CHECK(log.events[0].a == 2);
  CHECK(log.events[0].b == kEventLeftClick);

  // Holding the lower arrow: a row at once, the next after 30 ticks (10
  // passes of 3 ticks), then faster; the release stops it.
  const int arrow_x = 295;
  const int arrow_y = 190;
  rig.move(arrow_x, arrow_y);
  CHECK(list.arrow_state() == 3);
  rig.pass(InputEvent{.type = EventType::kMouseDown, .x = arrow_x, .y = arrow_y, .button = MouseButton::kLeft});
  CHECK(list.top() == 1);
  std::vector<int> tops;
  for (int i = 0; i < 30; ++i) {
    rig.pass();
    tops.push_back(list.top());
  }
  // Periods 30, 19, 11, 6, 3, 1, then 0 (every pass).
  CHECK(tops[8] == 1);
  CHECK(tops[9] == 2);
  CHECK(tops[16] == 3);
  CHECK(tops[19] == 4);
  CHECK(tops[22] == 6);
  CHECK(tops[25] == 9);
  rig.pass(InputEvent{.type = EventType::kMouseUp, .x = arrow_x, .y = arrow_y, .button = MouseButton::kLeft});
  const int stopped = list.top();
  rig.pass();
  rig.pass();
  CHECK(list.top() == stopped);
  // The top row stops when the last rows fill the window.
  CHECK(list.top() <= list.count() - (200 - 100) / list.line_height());
}

}  // namespace
}  // namespace oa
