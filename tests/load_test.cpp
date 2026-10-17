// The load/save and tutorial lists (screens 2, 4, 15), on the original
// files, with a game core faking a few saves.

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <cstring>

#include "game_data.h"
#include "screens/load_wnd.h"
#include "ui_rig.h"

namespace oa {
namespace {

// A game core whose slots 3 and 5 hold saves; it records the calls.
class FakeSaves : public StubGameSession {
 public:
  std::vector<SaveSlot> list_saves() const override {
    std::vector<SaveSlot> slots = StubGameSession::list_saves();
    for (const int s : {3, 5}) {
      SaveSlot& slot = slots[static_cast<std::size_t>(s - 1)];
      slot.header = SaveHeader{.day = 820,
                               .race_count = 4,
                               .player_race = 0,
                               .species = {2, 5, 9, 11, 0, 0, 0},
                               .colors = {1, 2, 3, 4, 0, 0, 0},
                               .relations = {0, 1, 2, 3, 0, 0, 0},
                               .fleet = 22,
                               .colonies = 12,
                               .discoveries = 40};
      slot.year = 1996;
      slot.month = 3;
      slot.day = 14;
      slot.hour = 15;
      slot.minute = 7;
    }
    return slots;
  }
  bool save(int slot) override {
    saved = slot;
    return true;
  }
  bool load(int slot) override {
    loaded = slot;
    return true;
  }

  int saved = 0;
  int loaded = 0;
};

constexpr std::uint16_t kAlt = SDL_KMOD_LALT;

// Alt + `key`, then Alt released (a held Alt makes clicks developer
// palette picks).
void alt_key(test::UiRig& rig, std::uint32_t key) {
  rig.key(key, kAlt);
  rig.pass(InputEvent{.type = EventType::kKeyUp, .key = SDLK_LALT, .mods = 0});
}

LoadWnd& load_window(test::UiRig& rig) {
  auto* w = dynamic_cast<LoadWnd*>(rig.wm.find_window("LOADSCREEN"));
  REQUIRE(w != nullptr);
  return *w;
}

ListBox& load_list(test::UiRig& rig) {
  auto* l = dynamic_cast<ListBox*>(rig.wm.find_window("LOADLIST"));
  REQUIRE(l != nullptr);
  return *l;
}

TEST_CASE("Alt+S opens the save list: 20 slots, a pick saves and goes back", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  FakeSaves game;
  test::UiRig rig(files, &game);
  alt_key(rig, 's');
  REQUIRE(rig.wm.current_screen() == 2);
  CHECK(load_window(rig).mode() == LoadWnd::Mode::kSave);
  CHECK(load_list(rig).count() == 20);
  CHECK(load_list(rig).line_height() == LoadWnd::kSaveRowHeight);
  // The title bar of the save list is wiped with 0x73.
  CHECK(rig.gs.screen().at(10, 10) == 0x73);
  // Row 2 (slot 2, empty): its text in colour 9 ("2." then "<EMPTY>").
  bool dim = false;
  for (int x = 47 + 5; x < 47 + 100; ++x) {
    for (int y = 47 + 85 + 5; y < 47 + 85 + 16; ++y) {
      dim = dim || rig.gs.screen().at(x, y) == 9;
    }
  }
  CHECK(dim);
  // Pick slot 2 (the second row, y 47 + 85..).
  rig.move(100, 47 + 85 + 20);
  CHECK(load_list(rig).hover_row() == 1);
  rig.click(100, 47 + 85 + 20);
  rig.pass();
  CHECK(game.saved == 2);
  CHECK(rig.wm.current_screen() == 0);
}

TEST_CASE("the load list: an empty slot does nothing, a used one loads and opens the star map", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  FakeSaves game;
  test::UiRig rig(files, &game);
  rig.click(318, 375);  // Load Game
  REQUIRE(rig.wm.current_screen() == 4);
  CHECK(load_window(rig).mode() == LoadWnd::Mode::kLoad);
  CHECK(rig.gs.screen().at(10, 10) == 0x10);
  // The key '1' picks the second row: slot 2, empty.
  rig.key('1');
  rig.pass();
  CHECK(game.loaded == 0);
  CHECK(rig.wm.current_screen() == 4);
  // '2' picks slot 3, a save.
  rig.key('2');
  rig.pass();
  CHECK(game.loaded == 3);
  CHECK(rig.wm.current_screen() == 1);
}

TEST_CASE("the save list's down arrow repeats while held, faster and faster", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  FakeSaves game;
  test::UiRig rig(files, &game);
  alt_key(rig, 's');
  ListBox& list = load_list(rig);
  REQUIRE(list.scroll_bar());
  // The lower half of the scroll bar.
  const int x = 625;
  const int y = 460;
  rig.move(x, y);
  CHECK(list.arrow_state() == 3);
  rig.pass(InputEvent{.type = EventType::kMouseDown, .x = x, .y = y, .button = MouseButton::kLeft});
  CHECK(list.top() == 1);
  // Nothing more until the 30-tick delay (a pass lasts 3 ticks).
  for (int i = 0; i < 8; ++i) {
    rig.pass();
  }
  CHECK(list.top() == 1);
  int passes = 0;
  while (list.top() < 4 && passes < 40) {
    rig.pass();
    ++passes;
  }
  CHECK(list.top() == 4);
  // The delays shrink (19, 11, 6 ticks): well under 30 ticks per row.
  CHECK(passes <= 16);
  // Released: the repeat stops.
  rig.pass(InputEvent{.type = EventType::kMouseUp, .x = x, .y = y, .button = MouseButton::kLeft});
  const int top = list.top();
  for (int i = 0; i < 20; ++i) {
    rig.pass();
  }
  CHECK(list.top() == top);
}

TEST_CASE("the tutorial list shows the 7 tutorials of tutindex.txt", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::UiRig rig(files);
  rig.click(318, 255);  // Tutorial
  REQUIRE(rig.wm.current_screen() == 15);
  const LoadWnd& w = load_window(rig);
  CHECK(w.mode() == LoadWnd::Mode::kTutorial);
  REQUIRE(w.tutorials().size() == 7);
  CHECK(w.tutorials()[1].name == "beginnew");
  CHECK(w.tutorials()[1].description == "Tutorial #2 : Beginning a New Game");
  CHECK(load_list(rig).count() == 7);
  CHECK(load_list(rig).line_height() == LoadWnd::kTutorialRowHeight);
}

// A game core whose slot 1 holds the shipped `01.SAV` (its header read from
// the install), dated 2026-10-16 23:54.
class ShippedSave : public StubGameSession {
 public:
  explicit ShippedSave(const GameFiles& files) {
    const std::vector<std::uint8_t> data = files.load("01.SAV");
    const auto i32 = [&](std::size_t i) {
      std::int32_t v = 0;
      std::memcpy(&v, data.data() + 4 * i, 4);
      return v;
    };
    SaveHeader h{.day = i32(0), .race_count = i32(1), .player_race = i32(2)};
    for (std::size_t r = 0; r < 7; ++r) {
      h.species[r] = i32(3 + r);
      h.colors[r] = i32(10 + r);
      h.relations[r] = i32(17 + r);
    }
    h.fleet = i32(24);
    h.colonies = i32(25);
    h.discoveries = i32(26);
    header_ = h;
  }
  std::vector<SaveSlot> list_saves() const override {
    std::vector<SaveSlot> slots = StubGameSession::list_saves();
    slots[0].header = header_;
    slots[0].year = 2026;
    slots[0].month = 10;
    slots[0].day = 16;
    slots[0].hour = 23;
    slots[0].minute = 54;
    return slots;
  }

 private:
  SaveHeader header_;
};

TEST_CASE("a used slot's row: flag, species, day, date, portrait, pips and contacts", "[ui][content]") {
  OA_REQUIRE_GAME_FILES(files);
  ShippedSave game(files);
  test::UiRig rig(files, &game);
  rig.click(318, 375);  // Load Game
  REQUIRE(rig.wm.current_screen() == 4);
  const Image8& screen = rig.gs.screen();
  // The texts in the player's colour ramp's brightest entry (colour 5:
  // 0x27), the fleet's 5 pips from x 8 + 200 at y 47 + 20.
  const int text = 5 * 4 + 0x13;
  int count = 0;
  for (int y = 47; y < 47 + 85; ++y) {
    for (int x = 8; x < 200; ++x) {
      count += screen.at(x, y) == text ? 1 : 0;
    }
  }
  CHECK(count > 100);
}

}  // namespace
}  // namespace oa
