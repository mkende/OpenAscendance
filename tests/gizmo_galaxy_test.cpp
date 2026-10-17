// The ship gizmos used on lanes and planets, the passive gizmos and the
// player's ships moving through the galaxy: the original's runs of
// `tests/oracle/cov/gizgal` (the shipped day-820 game edited by
// `gizgal_save.py`), both builds, replayed (`replay_rig.h`) and compared
// save by save and screenshot by screenshot (OA_DATA install; OA_ORACLE,
// `docs/12-oracle.md`).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "game_data.h"
#include "replay_rig.h"

namespace oa {

namespace {

// The runs of one case file line pair: `NAME_patch` (`PATCH.EXE`) and
// `NAME_antag` (`ANTAG.EXE`) on the edited save `NAME.GAM`.
std::vector<test::ReplayRun> both_builds(const std::string& name) {
  const std::string save = "cov/gizgal/" + name + ".GAM";
  return {
      {.run = "cov/gizgal/" + name + "_patch", .resume = save, .shots = {}},
      {.run = "cov/gizgal/" + name + "_antag", .variant = game::Variant::kAntagonizer, .resume = save, .shots = {}}};
}

}  // namespace

TEST_CASE("the original's lane gizmos and planet weapons", "[content][oracle][gizgal]") {
  OA_REQUIRE_GAME_FILES(files);
  // lane: Feculon I's Missile Base, Long Range Whopper and Tractor Beam at
  // the Capelon Mediums; the Lane Blocker, Mass Barrage Gun (damage 1) and
  // Fourier Missiles (damage 2) on the lane to Algol and a move into it;
  // the Lane Endoscope, Destabilizer, Blocker, a move into the blocked lane
  // and the Lane Magnetron on the lane to Paragon; four days.
  test::replay_oracle_runs(files, both_builds("lane"));
}

TEST_CASE("the original's ship gizmos at a planet", "[content][oracle][gizgal]") {
  OA_REQUIRE_GAME_FILES(files);
  // planet: weapons at Stella IV (a shield under construction, weakened
  // shields), at Stella I (its only orbital item a docked ship: double
  // damage) and at the protected Stella III; the Phase Bomb, the
  // Myrmidonic Carbonizer inside its ANTAG minimum range; the Capelons'
  // turns (planet weapons firing back in ANTAG).
  test::replay_oracle_runs(files, both_builds("planet"));
}

TEST_CASE("the original's scanners and cloakers", "[content][oracle][gizgal]") {
  OA_REQUIRE_GAME_FILES(files);
  // scan: what the System Display shows of Capelon ships with Cloakers to
  // Frigates with scanners and X Ray Megaglasses; the arrivals of the next
  // day (events 4, 8, 9).
  test::replay_oracle_runs(files, both_builds("scan"));
}

TEST_CASE("the original's ships moving through the galaxy", "[content][oracle][gizgal]") {
  OA_REQUIRE_GAME_FILES(files);
  // travel: an invasion, orbit orders (the Gyro-Inductor, event 16), a
  // colony created from event 16's planet screen, arrivals over four days
  // (events 3, 5, 6, 7, 9, 20, the contested-system prompts), a Star Lane
  // Hyperdrive on a red lane, the star map's ships in a lane.
  test::replay_oracle_runs(files, both_builds("travel"));
}

}  // namespace oa
