// The end-of-game screen (screen 9): the ending texts and the screen of the
// oracle's `exp/endgame_dest` run (the shipped day-820 game with every other
// species marked extinct, then Next Turn: ending 2, `end-dest.txt`), and the
// endings 2..5 reached by a day of play (`tests/oracle/cov/endev`).

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_keycode.h>

#include "assets/gif.h"
#include "core_session.h"
#include "game/cosmos.h"
#include "game/new_game.h"
#include "game_data.h"
#include "game_oracle.h"
#include "replay_rig.h"
#include "screen_shots.h"
#include "screens/end_game_wnd.h"
#include "ui_rig.h"

namespace oa::test {

namespace {

constexpr int kScreenStarMap = 1;
constexpr int kScreenEndOfGame = 9;

}  // namespace

TEST_CASE("The rank is the score's tens less one, clamped", "[endgame]") {
  CHECK(EndGameWnd::rank_of(0) == 0);
  CHECK(EndGameWnd::rank_of(11) == 0);
  CHECK(EndGameWnd::rank_of(20) == 1);
  CHECK(EndGameWnd::rank_of(99) == 8);
  CHECK(EndGameWnd::rank_of(100) == 9);
  CHECK(EndGameWnd::rank_of(250) == 9);
}

TEST_CASE("An ending file gives its description, congratulations and rank title", "[endgame]") {
  OA_REQUIRE_GAME_FILES(files);
  const EndingText t = parse_ending(files.load("end-dest.txt"), 2, "end-dest.txt");
  // The line ends are spaces; `^` stays (a blank glyph of the font).
  CHECK(t.description.starts_with("^^^^^Your ferocity has no limits.  You have destroyed"));
  CHECK(t.description.ends_with("rule the entire galaxy. "));
  CHECK(t.rank == "Galactic Villain. ");
  CHECK(t.congratulations.ends_with("you will never know. "));
  CHECK(parse_ending(files.load("end-xtnc.txt"), 0, "end-xtnc.txt").rank.size() > 1);
}

TEST_CASE("The game ending on a new day shows the end-of-game screen", "[endgame]") {
  OA_REQUIRE_GAME_FILES(files);
  CoreGameSession game(files, files.load("RESUME.GAM"));
  game::Session& s = game.session();
  // The oracle's run: the start-up galaxy of seed 1 (on fresh records: the
  // stale fields of the loaded game would change its draws), then the save
  // loaded.
  s.game = std::make_unique<game::Game>();
  const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = 1, .species = {}};
  game::program_start(s, 0, &txt, true);
  game::load_game(s, game::parse_save(files.load("RESUME.GAM")));
  game::Cosmos& cosmos = s.game->cosmos;
  for (int r = 1; r < cosmos.race_count; ++r) {
    cosmos.races[static_cast<std::size_t>(r)].extinct = -1;
  }
  REQUIRE(game::cosmos_next_contested_star(s) == game::kNone);
  game::cosmos_end_day(s);
  // The original's: day 821, ending 2, score 11 (its RESUME.GAM).
  REQUIRE(cosmos.day == 821);
  REQUIRE(cosmos.ending == 2);
  REQUIRE(cosmos.score == 11);
  UiRig rig(files, &game);
  rig.wm.setup_race_shapes();
  rig.wm.set_screen(kScreenStarMap, false);
  const std::uint32_t before = s.rng.state();
  rig.wm.set_screen(kScreenEndOfGame, true);
  rig.pass();
  // Two draws per ship and planet of the player.
  int pictures = 0;
  for (int i = 0; i < cosmos.ship_count; ++i) {
    pictures += cosmos.ships[static_cast<std::size_t>(i)].owner == s.player_race ? 1 : 0;
  }
  for (int i = 0; i < cosmos.planet_count; ++i) {
    pictures += cosmos.planets[static_cast<std::size_t>(i)].owner == s.player_race ? 1 : 0;
  }
  WatcomRandom expected(before);
  for (int i = 0; i < 2 * pictures; ++i) {
    expected.rand();
  }
  CHECK(s.rng.state() == expected.state());
  write_shot(rig, "endgame_dest");
  if (const auto gif = game::oracle_test::oracle_file("exp/endgame_dest/out/SCR000.GIF")) {
    // Identical but for the pointer (on the Next Turn button's spot).
    const Image8 original = decode_gif(*gif, "SCR000.GIF");
    int differing = 0;
    for (int y = 0; y < kScreenHeight; ++y) {
      for (int x = 0; x < kScreenWidth; ++x) {
        const bool pointer = x >= 505 && x < 530 && y >= 60 && y < 90;
        differing += !pointer && rig.wm.screen().at(x, y) != original.at(x, y) ? 1 : 0;
      }
    }
    CHECK(differing == 0);
  }
  // Esc goes back to the star map.
  rig.key(SDLK_ESCAPE);
  CHECK(rig.wm.current_screen() == kScreenStarMap);
}

TEST_CASE("the original's endings 2..5 reached by a day of play", "[content][oracle][endev]") {
  OA_REQUIRE_GAME_FILES(files);
  // `tests/oracle/cov/endev/cases.txt`: the edited day-820 games of
  // `end_save.py`, then Next Turn (`unit`, `ctrl`: after the Capelons accept
  // the player's alliance); the end-of-game screen, the star map, a save.
  // `dest_hostile`, `capt_peaceful`: the game loaded after a new game of that
  // atmosphere (the score's factor).
  std::vector<ReplayRun> runs;
  for (const char* name : {"dest", "capt", "unit", "ctrl", "dest_hostile", "capt_peaceful"}) {
    const std::string save = std::string(name).substr(0, 4);
    runs.push_back({.run = std::format("cov/endev/{}_patch", name), .resume = std::format("cov/endev/{}.GAM", save),
                    .shots = {}});
    runs.push_back({.run = std::format("cov/endev/{}_antag", name),
                    .variant = game::Variant::kAntagonizer,
                    .resume = std::format("cov/endev/{}.GAM", save),
                    .shots = {}});
  }
  replay_oracle_runs(files, runs);
}

}  // namespace oa::test
