// The ship gizmos used in the System Display battles (`docs/08-combat.md`
// "Using a gizmo", "AI in battle"): the original's runs of
// `tests/oracle/cov/gizbat` (the day-820 battle at Feculon edited by
// `gizbat_save.py`) replayed in the remake (`replay_rig.h`), both builds;
// every save (the battle's end and three days after) and screenshot
// compared.

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <vector>

#include "game_data.h"
#include "replay_rig.h"

namespace oa {

namespace {

// The runs `cov/gizbat/NAME` (PATCH) and `cov/gizbat/NAME_antag` (ANTAG) on
// the save `cov/gizbat/NAME.GAM`.
std::vector<test::ReplayRun> both_builds(const std::vector<std::string>& names) {
  std::vector<test::ReplayRun> runs;
  for (const std::string& name : names) {
    const std::string save = std::format("cov/gizbat/{}.GAM", name);
    runs.push_back({.run = "cov/gizbat/" + name, .resume = save, .shots = {}});
    runs.push_back({.run = std::format("cov/gizbat/{}_antag", name),
                    .variant = game::Variant::kAntagonizer,
                    .resume = save,
                    .shots = {}});
  }
  return runs;
}

}  // namespace

TEST_CASE("the original's weapons and other equipment in battle", "[content][oracle][gizbat]") {
  OA_REQUIRE_GAME_FILES(files);
  // weap: weapons 0..9 fired at the Capelon Mediums through their shields,
  // shields toggled on and off, a move with the Tonklin Motor and with the
  // Ion Banger, the generators' power, the Capelons' answers; three days.
  test::replay_oracle_runs(files, both_builds({"weap"}));
}

TEST_CASE("the original's special gizmos used by the player in battle", "[content][oracle][gizbat]") {
  OA_REQUIRE_GAME_FILES(files);
  // self: the untargeted 53 57 58 63 65 69 75 and the ship-targeted 33 34
  // 37 49 59 68. target: the ship-targeted 48 50 54 66 64 47 (too close in
  // ANTAG) 35 61. displace: 40, 36, 56 39 46 38 72. far: 57 and a gun
  // beyond its range, 47 beyond the ANTAG minimum, shields 13 14 15, moves
  // with drives 18 and 20.
  test::replay_oracle_runs(files, both_builds({"self", "target", "displace", "far"}));
}

TEST_CASE("the original's tactical AIs using gizmos", "[content][oracle][gizbat]") {
  OA_REQUIRE_GAME_FILES(files);
  // ai_evade: unarmed Capelon Mediums threatened by the Frigates use their
  // evasive gizmos 69 63 50 46 45 38 37 35 33 36 (ANTAG: its own table)
  // on the Capelons' turns and in the following days' battles; ai_evade2:
  // the two sets swapped (ANTAG tries only its last threatened ship's).
  // ai_support:
  // the Accutron of an armed Medium out of range, the Replenisher of one
  // with its gun used up.
  test::replay_oracle_runs(files, both_builds({"ai_evade", "ai_evade2", "ai_support"}));
}

TEST_CASE("the original's battle effects played frame by frame by ANTAG", "[content][oracle][gizbat]") {
  OA_REQUIRE_GAME_FILES(files);
  // The `m5fx` runs (`star_system_test.cpp`) replayed with `ANTAG.EXE`.
  std::vector<test::ReplayRun> runs;
  for (const int g : {52, 55, 32, 56, 74, 58, 46, 60, 59, 68, 45, 39, 40, 36, 72, 38}) {
    runs.push_back({.run = std::format("cov/gizbat/fx_{:02d}_antag", g),
                    .variant = game::Variant::kAntagonizer,
                    .resume = std::format("m5fx/fx_{:02d}.GAM", g),
                    .shots = {}});
  }
  test::replay_oracle_runs(files, runs);
}

}  // namespace oa
