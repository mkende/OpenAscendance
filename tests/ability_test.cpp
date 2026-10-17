// The species' special abilities (`docs/09-simulation.md` "Special
// abilities", `Race_use_special_ability` 0x3efc8): the original's runs of
// `tests/oracle/cov/abil` replayed (`replay_rig.h`) in both variants, their
// saves and screenshots compared (OA_DATA install, OA_ORACLE runs).

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <utility>
#include <vector>

#include "game_data.h"
#include "replay_rig.h"

namespace oa {

namespace {

// The runs of `name` (`cov/abil/<name>_patch`, `_antag`), on the edited
// save `resume` (none for a new game) or new games of `seed`.
std::vector<test::ReplayRun> both_variants(const std::string& name, const std::string& resume) {
  std::vector<test::ReplayRun> runs;
  for (const auto& [suffix, variant] :
       {std::pair{"patch", game::Variant::kStandard}, std::pair{"antag", game::Variant::kAntagonizer}}) {
    runs.push_back({.run = std::format("cov/abil/{}_{}", name, suffix),
                    .variant = variant,
                    .resume = resume.empty() ? std::string() : std::format("cov/abil/{}.GAM", resume),
                    .shots = {}});
  }
  return runs;
}

}  // namespace

TEST_CASE("the player's special abilities used through their window", "[content][oracle][ability]") {
  OA_REQUIRE_GAME_FILES(files);
  // `use_NN`: the day-820 game with the player given species NN, its
  // ability charged (or one day short: event 24 first), the situation its
  // effect needs; the window, Use Ability, the result, then days.
  // `use_11g`: Ungooma with free ship slots below a ship on a lane, which
  // the loop over `ship_count` slots skips.
  std::vector<test::ReplayRun> runs = both_variants("use_11g", "use_11g");
  for (const int species : {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20}) {
    const std::string name = std::format("use_{:02d}", species);
    for (test::ReplayRun& run : both_variants(name, name)) {
      runs.push_back(std::move(run));
    }
  }
  test::replay_oracle_runs(files, runs);
}

TEST_CASE("the player's passive abilities", "[content][oracle][ability]") {
  OA_REQUIRE_GAME_FILES(files);
  // `p_00` Minions invading with one module through a Surface Shield,
  // `p_01` Snovemdomas launching a ship (double hit points), `p_02` Orfa
  // building on a black square, `p_18` Chronomyst ships on lanes (one
  // without a drive).
  std::vector<test::ReplayRun> runs;
  for (const int species : {0, 1, 2, 18}) {
    const std::string name = std::format("p_{:02d}", species);
    for (test::ReplayRun& run : both_variants(name, name)) {
      runs.push_back(std::move(run));
    }
  }
  test::replay_oracle_runs(files, runs);
}

TEST_CASE("new games of the start-of-game abilities", "[content][oracle][ability]") {
  OA_REQUIRE_GAME_FILES(files);
  // Kambuchka, Hanshaks and Oculons picked on the new-game screen (seed 1).
  test::replay_oracle_runs(files, both_variants("newgame", ""));
}

TEST_CASE("AI races using their special abilities", "[content][oracle][ability]") {
  OA_REQUIRE_GAME_FILES(files);
  // `ai_a`: Mebes and Fludentri AI races, `ai_b`: a Shevar one, charged.
  std::vector<test::ReplayRun> runs = both_variants("ai_a", "ai_a");
  for (test::ReplayRun& run : both_variants("ai_b", "ai_b")) {
    runs.push_back(std::move(run));
  }
  test::replay_oracle_runs(files, runs);
}

}  // namespace oa
