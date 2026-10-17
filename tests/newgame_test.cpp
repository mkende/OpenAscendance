// The new-game set-up (screen 3) and the species history (screen 12), on
// the original files with a game core recording the calls.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "game_data.h"
#include "screens/new_game_wnd.h"
#include "screens/start_game_wnd.h"
#include "ui_rig.h"

namespace oa {
namespace {

// The stand-in game core, recording the new-game calls.
class RecordingSession final : public StubGameSession {
 public:
  bool save_resume() override {
    calls.push_back("save_resume");
    return true;
  }
  void generate_galaxy(int stars, int races, int species) override {
    calls.push_back(std::format("generate {} {} {}", stars, races, species));
    StubGameSession::generate_galaxy(stars, races, species);
  }
  void new_galaxy(int races, int species) override {
    calls.push_back(std::format("new {} {}", races, species));
    StubGameSession::new_galaxy(races, species);
  }
  void assign_colors(int color) override {
    calls.push_back(std::format("colors {}", color));
    StubGameSession::assign_colors(color);
  }
  void begin_game(const NewGameSettings& s) override {
    begun = s;
    StubGameSession::begin_game(s);
  }

  std::vector<std::string> calls;
  std::optional<NewGameSettings> begun;
};

// Opens the new-game screen from the opening menu.
NewGameWnd& open_new_game(test::UiRig& rig) {
  rig.click(318, 295);  // FRESHGAME
  REQUIRE(rig.wm.current_screen() == 3);
  auto* w = dynamic_cast<NewGameWnd*>(rig.wm.find_window("NEWGAMEWND"));
  REQUIRE(w != nullptr);
  return *w;
}

TEST_CASE("the new-game screen saves resume.gam and sets up the default galaxy", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  RecordingSession game;
  test::UiRig rig(files, &game);
  const NewGameWnd& w = open_new_game(rig);
  CHECK(game.calls == std::vector<std::string>{"save_resume", "generate 50 5 0", "colors 0"});
  CHECK(w.settings().star_density == 2);
  CHECK(w.settings().species_count == 5);
  CHECK(w.settings().atmosphere == 1);
  CHECK(w.settings().species == 0);
  // The selected colour 0 is framed in white: (381, 433)-(395, 457).
  CHECK(rig.gs.screen().at(381, 440) == kTextColor);
  CHECK(rig.gs.screen().at(395, 440) == kTextColor);
}

TEST_CASE("the setting buttons cycle and regenerate the galaxy", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  RecordingSession game;
  test::UiRig rig(files, &game);
  const NewGameWnd& w = open_new_game(rig);
  game.calls.clear();
  std::vector<int> densities;
  for (int i = 0; i < 5; ++i) {
    rig.click(60, 420);  // GALAXY_MORE
    densities.push_back(w.settings().star_density);
  }
  CHECK(densities == std::vector<int>{3, 4, 0, 1, 2});
  CHECK(game.calls.front() == "generate 75 5 0");
  CHECK(game.calls[2] == "generate 15 5 0");
  game.calls.clear();
  std::vector<int> counts;
  for (int i = 0; i < 5; ++i) {
    rig.click(190, 420);  // PLAYERS_MORE
    counts.push_back(w.settings().species_count);
  }
  CHECK(counts == std::vector<int>{6, 7, 3, 4, 5});
  CHECK(game.calls.front() == "new 6 0");
  game.calls.clear();
  std::vector<int> atmospheres;
  for (int i = 0; i < 3; ++i) {
    rig.click(310, 420);  // DIFFICULTYMORE
    atmospheres.push_back(w.settings().atmosphere);
  }
  CHECK(atmospheres == std::vector<int>{2, 0, 1});
  CHECK(game.calls.empty());
}

TEST_CASE("RACELIST picks the species, the colour bar the colour", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  RecordingSession game;
  test::UiRig rig(files, &game);
  const NewGameWnd& w = open_new_game(rig);
  game.calls.clear();
  rig.click(560, 200);  // the second row (rows of 115 pixels from y 46)
  CHECK(w.settings().species == 1);
  CHECK(game.calls == std::vector<std::string>{"new 5 1"});
  rig.click(381 + 3 * 16 + 5, 440);
  CHECK(w.settings().color == 3);
  CHECK(game.calls.back() == "colors 3");
}

TEST_CASE("Begin New Game starts the game, the species history leads to the star map", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  RecordingSession game;
  test::UiRig rig(files, &game);
  open_new_game(rig);
  rig.click(310, 420);  // atmosphere: hostile
  rig.click(560, 440);  // the "Begin New Game" box
  REQUIRE(game.begun);
  CHECK(game.begun->atmosphere == 2);
  CHECK(game.begun->star_density == 2);
  CHECK(rig.wm.current_screen() == 12);
  // The species' theme: tune 8 + species.
  rig.pass();
  CHECK(rig.sound.current_tune() == 8);
  rig.click(320, 200);
  rig.pass();
  CHECK(rig.wm.current_screen() == 1);
}

TEST_CASE("history.txt gives the species' title, introduction and text", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  const std::vector<std::uint8_t> data = files.load("history.txt");
  const SpeciesHistory minions = parse_species_history(data, 0, 1, "Minions");
  CHECK(minions.title == "Minions: Supreme Planet Invaders\n");
  CHECK(minions.intro.starts_with("     Your ancient programming has been reactivated.  You are the central"));
  CHECK(minions.intro.find("\n\n     The Minions have detected") != std::string::npos);
  CHECK(minions.text.starts_with("     The Minions are a species of artificial life"));
  // Species 1 has a neutral paragraph after its introduction.
  const SpeciesHistory neutral = parse_species_history(data, 1, 1, "Snovemdomas");
  const SpeciesHistory peaceful = parse_species_history(data, 1, 0, "Snovemdomas");
  CHECK(neutral.intro != peaceful.intro);
}

}  // namespace
}  // namespace oa
