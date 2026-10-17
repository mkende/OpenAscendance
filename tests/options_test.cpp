// The remake's options (`screens/options_wnd.h`, `ui/options.h`): the
// opening menu's Options button and layout, the options screen and
// `options.cfg`, an Antagonizer new game against the original's (OA_DATA
// install; the comparisons with the original's runs need OA_ORACLE). With
// OA_SHOTS set, the screens are written there (`screen_shots.h`).

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "assets/gif.h"
#include "core_session.h"
#include "game/save.h"
#include "game_data.h"
#include "screen_shots.h"
#include "screens/options_wnd.h"
#include "test_dirs.h"
#include "ui/game_core_session.h"
#include "ui/help_wnd.h"
#include "ui/options.h"
#include "ui_rig.h"

namespace oa {
namespace {

using test::TempDir;

// The original's menu buttons (`windows.txt`): 159..477, 31 rows from y
// 200 every 40.
std::vector<Pane> original_menu() {
  std::vector<Pane> panes;
  for (int i = 0; i < 7; ++i) {
    panes.push_back(Pane{159, 200 + 40 * i, 477, 230 + 40 * i});
  }
  return panes;
}

// The options screen's buttons: choice `c` of row `row` (two choices), and
// OK and Cancel (`options_wnd.cpp`: rows from y 72 every 60, the choices'
// columns 258..422 and 445..609, OK and Cancel at y 424).
constexpr int choice_x(int c) { return c == 0 ? 340 : 527; }
constexpr int row_y(int row) { return 72 + 60 * row + 14; }
constexpr int kOkX = 340;
constexpr int kCancelX = 527;
constexpr int kButtonsY = 437;
enum Row : int { kVariant, kDisplay, kScaling, kMusic, kSound };

class FakeDisplay final : public DisplayControl {
 public:
  bool fullscreen() const override { return fullscreen_; }
  void set_fullscreen(bool fullscreen) override { fullscreen_ = fullscreen; }
  Scaling scaling() const override { return scaling_; }
  void set_scaling(Scaling scaling) override { scaling_ = scaling; }

 private:
  bool fullscreen_ = false;
  Scaling scaling_ = Scaling::kSmooth;
};

// Installs the options on `rig` and enters the opening menu again (the rig
// starts on it with the original layout).
void install(test::UiRig& rig, OptionsContext context) {
  install_options(rig.wm, std::move(context));
  rig.wm.set_screen(kScreenOptions, false);
  rig.wm.set_screen(0, false);
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

TEST_CASE("the opening menu's buttons spread over the original's span", "[ui]") {
  const std::vector<Pane> original = original_menu();
  const std::vector<Pane> eight = spread_menu_buttons(original, 8);
  REQUIRE(eight.size() == 8);
  CHECK(eight.front().y0 == 200);
  CHECK(eight.back().y1 == 470);
  for (std::size_t i = 0; i < eight.size(); ++i) {
    CHECK(eight[i].x0 == 159);
    CHECK(eight[i].x1 == 477);
    // 271 rows: 8 buttons of 26 and 7 gaps of 9, the original's gap.
    CHECK(eight[i].y1 - eight[i].y0 + 1 == 26);
    if (i > 0) {
      CHECK(eight[i].y0 - eight[i - 1].y1 - 1 == 9);
    }
  }
  // Rows that do not divide go to the first buttons.
  const std::vector<Pane> six = spread_menu_buttons(original, 6);
  CHECK(six.front().y1 - six.front().y0 + 1 == (271 - 5 * 9) / 6 + 1);
  CHECK(six.back().y1 == 470);
  CHECK(spread_menu_buttons(original, 7) == original);
}

TEST_CASE("the opening menu with the Options button", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::UiRig rig(files);
  RemakeOptions options;
  install(rig, OptionsContext{.options = &options, .file = {}, .display = nullptr});

  const std::vector<Pane> eight = spread_menu_buttons(original_menu(), 8);
  std::size_t slot = 0;
  for (std::size_t i = 0; i < kMenuButtons.size(); ++i, ++slot) {
    if (i == kOptionsButtonIndex) {
      ++slot;
    }
    CHECK(rig.wm.find_window(kMenuButtons[i])->pane() == eight[slot]);
  }
  GWindow* button = rig.wm.find_window("Options");
  REQUIRE(button != nullptr);
  CHECK(button->pane() == eight[kOptionsButtonIndex]);
  CHECK(button->shown());

  // Everything but the panel's rows between the buttons' top and bottom is
  // the original's picture.
  const Image8 gif = decode_gif(files.load("DATA\\0OPENING.GIF"), "0OPENING.GIF");
  int differing = 0;
  for (int y = 0; y < gif.height(); ++y) {
    for (int x = 0; x < gif.width(); ++x) {
      const bool panel = x >= 152 && x <= 484 && y >= 200 && y <= 470;
      if (!panel && rig.wm.screen().at(x, y) != gif.at(x, y)) {
        ++differing;
      }
    }
  }
  CHECK(differing == 0);
  rig.move(318, 433);
  CHECK(rig.wm.hot_window() == button->id());
  test::write_shot(rig, "options_menu");

  // Options opens the options screen, Esc goes back.
  rig.click(318, 433);
  CHECK(rig.wm.current_screen() == kScreenOptions);
  rig.key(SDLK_ESCAPE);
  CHECK(rig.wm.current_screen() == 0);
  // The other buttons keep their messages: New Game.
  rig.click(318, 282);
  CHECK(rig.wm.current_screen() == 3);
}

TEST_CASE("a tutorial replays on the original opening menu", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  GameCoreSession session(files, {}, false);
  session.start(0, nullptr);
  test::UiRig rig(files, &session);
  RemakeOptions options;
  install(rig, OptionsContext{.options = &options, .file = {}, .display = nullptr});
  rig.wm.set_screen(15, true);
  REQUIRE(rig.wm.start_tutorial("beginnew"));
  rig.pass();
  REQUIRE(rig.wm.current_screen() == 0);
  const std::vector<Pane> original = original_menu();
  for (std::size_t i = 0; i < kMenuButtons.size(); ++i) {
    CHECK(rig.wm.find_window(kMenuButtons[i])->pane() == original[i]);
  }
  CHECK_FALSE(rig.wm.find_window("Options")->shown());
  const Image8 gif = decode_gif(files.load("DATA\\0OPENING.GIF"), "0OPENING.GIF");
  CHECK(rig.wm.screen().at(155, 235) == gif.at(155, 235));
}

TEST_CASE("options.cfg: the settings' text round-trips; bad lines are ignored", "[ui]") {
  const RemakeOptions defaults;
  CHECK(parse_options("") == defaults);
  const RemakeOptions set{.variant = game::Variant::kAntagonizer, .fullscreen = true, .scaling = Scaling::kSharp};
  CHECK(format_options(set) == "variant=antagonizer\nfullscreen=1\nscaling=sharp\n");
  CHECK(parse_options(format_options(set)) == set);
  CHECK(parse_options(format_options(defaults)) == defaults);
  CHECK(parse_options("# comment\r\n variant = antagonizer # Antagonizer\r\nwide=1\nfullscreen=yes\nscaling\n") ==
        RemakeOptions{.variant = game::Variant::kAntagonizer, .fullscreen = false, .scaling = Scaling::kSmooth});

  TempDir dir("oa_options");
  const std::filesystem::path file = dir.path / kOptionsFile;
  CHECK(load_options(file) == defaults);
  REQUIRE(save_options(file, set));
  CHECK(load_options(file) == set);
}

TEST_CASE("the options screen: OK applies and saves the settings, Cancel and Esc leave them", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  TempDir dir("oa_options");
  test::UiRig rig(files, nullptr, dir.path);
  RemakeOptions options;
  FakeDisplay display;
  const std::filesystem::path file = dir.path / kOptionsFile;
  install(rig, OptionsContext{.options = &options, .file = file, .display = &display});
  rig.sound.set_music_muted(false);
  rig.sound.set_sfx_muted(true);

  rig.click(318, 433);
  REQUIRE(rig.wm.current_screen() == kScreenOptions);
  // The current settings are selected: Sound Effects off.
  rig.move(choice_x(1), row_y(kVariant));
  test::write_shot(rig, "options_screen");
  for (const Row row : {kVariant, kDisplay, kScaling, kMusic}) {
    rig.click(choice_x(1), row_y(row));
  }
  rig.click(choice_x(0), row_y(kSound));
  rig.click(kCancelX, kButtonsY);
  CHECK(rig.wm.current_screen() == 0);
  CHECK(options == RemakeOptions{});
  CHECK_FALSE(rig.sound.music_muted());
  CHECK(rig.sound.sfx_muted());
  CHECK_FALSE(std::filesystem::exists(file));

  // Opened again: the picks were forgotten. Esc cancels too.
  rig.click(318, 433);
  rig.click(choice_x(1), row_y(kVariant));
  rig.key(SDLK_ESCAPE);
  CHECK(rig.wm.current_screen() == 0);
  CHECK(rig.game.new_game_variant() == game::Variant::kStandard);

  rig.click(318, 433);
  for (const Row row : {kVariant, kDisplay, kScaling, kMusic}) {
    rig.click(choice_x(1), row_y(row));
  }
  rig.click(choice_x(0), row_y(kSound));
  rig.click(kOkX, kButtonsY);
  CHECK(rig.wm.current_screen() == 0);
  const RemakeOptions expected{.variant = game::Variant::kAntagonizer, .fullscreen = true, .scaling = Scaling::kSharp};
  CHECK(options == expected);
  CHECK(load_options(file) == expected);
  CHECK(rig.game.new_game_variant() == game::Variant::kAntagonizer);
  CHECK(display.fullscreen());
  CHECK(display.scaling() == Scaling::kSharp);
  CHECK(rig.sound.music_muted());
  CHECK_FALSE(rig.sound.sfx_muted());
  // ascend.cfg: the original's 12 bytes, the mute flags as 0 / -1.
  const std::string cfg = read_text(dir.path / "ascend.cfg");
  REQUIRE(cfg.size() == 12);
  CHECK(cfg.substr(0, 8) == std::string("\0\0\0\0\xff\xff\xff\xff", 8));

  // Enter is OK; the screen shows the settings in force (the window's).
  display.set_fullscreen(false);
  rig.click(318, 433);
  rig.key(SDLK_RETURN);
  CHECK(rig.wm.current_screen() == 0);
  CHECK_FALSE(options.fullscreen);
  CHECK(options.variant == game::Variant::kAntagonizer);
}

TEST_CASE("the options screen's help: Shift and a click", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  test::UiRig rig(files);
  RemakeOptions options;
  install(rig, OptionsContext{.options = &options, .file = {}, .display = nullptr});
  const auto help_click = [&](int x, int y) {
    rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_LSHIFT, .mods = SDL_KMOD_LSHIFT});
    rig.pending.push_back(InputEvent{.type = EventType::kMouseMove, .x = x, .y = y});
    rig.queue_click(x, y);
    rig.pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = SDLK_LSHIFT, .mods = 0});
    // The help window's modal loop takes the rest: its OK closes it.
    rig.queue_click(320, 350);
    rig.drain();
  };
  auto& help = dynamic_cast<HelpWnd&>(*rig.wm.find_window("HELPWINDOW"));
  help_click(318, 433);
  CHECK(help.topic() == "oa_options");
  rig.click(318, 433);
  REQUIRE(rig.wm.current_screen() == kScreenOptions);
  help_click(choice_x(0), row_y(kScaling));
  CHECK(help.topic() == "oa_scaling");
  help_click(100, row_y(kVariant));
  CHECK(help.topic() == "oa_variant");
  help_click(100, 400);
  CHECK(help.topic() == "oa_screen");
  CHECK(rig.wm.current_screen() == kScreenOptions);
}

// Clears the stale stack bytes of a save header: the per-race entries past
// the race count and the last 8 bytes.
void clear_stale_header(std::vector<std::uint8_t>& save) {
  const int races = save[4];
  for (const std::size_t table : {0x0cU, 0x28U, 0x44U}) {
    for (int r = races; r < 7; ++r) {
      std::fill_n(save.begin() + static_cast<std::ptrdiff_t>(table + 4 * static_cast<std::size_t>(r)), 4, 0);
    }
  }
  std::fill_n(save.begin() + 0x6c, 8, 0);
}

TEST_CASE("an Antagonizer new game from the options is the original Antagonizer's", "[screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  int found = 0;
  for (const int seed : {1, 2, 3, 12345}) {
    const std::string run = std::format("corpus/day0_antag_s{}/out/", seed);
    const auto new_game_shot = test::oracle_path(run + "SCR000.GIF");
    const auto save = test::oracle_path(run + "02.SAV");
    if (!new_game_shot || !save) {
      continue;
    }
    ++found;
    INFO(run);
    TempDir dir("oa_options");
    // `main` with options.cfg's variant: the start-up galaxy under its rules.
    GameCoreSession session(files, dir.path, false);
    session.set_new_game_variant(game::Variant::kAntagonizer);
    const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = seed, .species = {}};
    session.start(0, &txt);
    test::UiRig rig(files, &session, dir.path);
    RemakeOptions options{.variant = game::Variant::kAntagonizer};
    install(rig, OptionsContext{.options = &options, .file = {}, .display = nullptr});

    // The steps of the oracle's `day0_default.txt` (`star_map_test.cpp`),
    // New Game on its button of the remake's menu.
    const auto wait = [&](int passes) {
      for (int i = 0; i < passes; ++i) {
        rig.pass();
      }
    };
    wait(40);
    rig.move(318, 282);
    rig.click(318, 282);
    wait(60);
    REQUIRE(rig.wm.current_screen() == 3);
    // The original's screen (with its pointer) but the variant's tag under
    // the summary.
    const Image8 original = decode_gif(test::read_file(*new_game_shot), new_game_shot->string());
    int outside_tag = 0;
    int in_tag = 0;
    for (int y = 0; y < original.height(); ++y) {
      for (int x = 0; x < original.width(); ++x) {
        // The original's pointer, on its New Game button.
        const bool pointer = x >= 318 - 16 && x <= 318 + 16 && y >= 295 - 16 && y <= 295 + 16;
        if (pointer || original.at(x, y) == rig.wm.screen().at(x, y)) {
          continue;
        }
        (x >= 0x11 && x <= 0x144 && y >= 0x16b && y <= 0x174 ? in_tag : outside_tag) += 1;
      }
    }
    CHECK(outside_tag == 0);
    CHECK(in_tag > 0);
    if (seed == 1) {
      test::write_shot(rig, "options_new_game_antagonizer");
    }
    rig.pass();
    wait(2);
    rig.move(594, 452);
    rig.click(594, 452);  // Begin New Game
    wait(60);
    rig.move(320, 240);
    rig.click(320, 240);  // the species history
    wait(60);
    REQUIRE(rig.wm.current_screen() == 1);
    REQUIRE(session.save(2));
    std::vector<std::uint8_t> ours = test::read_file(dir.path / "02.SAV");
    std::vector<std::uint8_t> theirs = test::read_file(*save);
    // The remake's variant trailer (`game/save.h`) after the original's bytes.
    REQUIRE(ours.size() == theirs.size() + 8);
    CHECK(game::parse_save(ours).variant == game::Variant::kAntagonizer);
    ours.resize(theirs.size());
    clear_stale_header(ours);
    clear_stale_header(theirs);
    CHECK(ours == theirs);

    // Loaded back by a Standard session, the game is the Antagonizer's.
    GameCoreSession loader(files, dir.path, false);
    REQUIRE(loader.load(2));
    CHECK(loader.variant() == game::Variant::kAntagonizer);
  }
  if (found == 0) {
    SKIP("OA_ORACLE is not set to work/oracle with the day-0 corpus");
  }
}

}  // namespace
}  // namespace oa
