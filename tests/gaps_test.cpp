// The coverage gaps of the other oracle tests: the original's runs of
// `tests/oracle/cov/gaps` (the shipped day-820 game edited by
// `gaps_save.py`, both builds) replayed in the remake (`replay_rig.h`) and
// compared save by save and screenshot by screenshot, and the intro's
// script events as the original's debugger probe dumped them (OA_DATA
// install; OA_ORACLE, `docs/12-oracle.md`).

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include "assets/flic_script.h"
#include "game_data.h"
#include "replay_rig.h"

namespace oa {

namespace {

// The runs `gaps_NAME_patch` (`PATCH.EXE`) and `gaps_NAME_antag`
// (`ANTAG.EXE`) on the edited save `gaps_saves/SAVE.GAM`.
std::vector<test::ReplayRun> both_builds(const std::string& name, const std::string& save, bool dip_arg = false) {
  const std::string path = std::format("gaps_saves/{}.GAM", save);
  return {{.run = std::format("gaps_{}_patch", name), .resume = path, .shots = {}, .dip_arg_low_word = dip_arg},
          {.run = std::format("gaps_{}_antag", name),
           .variant = game::Variant::kAntagonizer,
           .resume = path,
           .shots = {},
           .dip_arg_low_word = dip_arg}};
}

}  // namespace

TEST_CASE("SUBTITLE.TXT: the events in the original's dispatch order", "[content][oracle][gaps]") {
  OA_REQUIRE_GAME_FILES(files);
  // `tests/oracle/cov/gaps/probe/intro_events.probe`: the intro's sorted
  // records (12 bytes from +0x204 of the FlicWnd) at the third call of
  // `Flic_run_script_events`; e.g. frame 988: the tune 7 started, then the
  // volume 127, then subtitle 15 (the unstable sort's order).
  const auto dump = test::oracle_run_path("gaps_intro_probe/probe/0002_2a1a8_1.bin");
  if (!dump) {
    SKIP("OA_ORACLE is not set to work/oracle with the run gaps_intro_probe");
  }
  const std::vector<std::uint8_t> bytes = read_file(*dump);
  const FlicScript s = parse_flic_script(files.load("SUBTITLE.TXT"));
  REQUIRE(bytes.size() >= 2 + 12 * s.events.size());
  const auto u16 = [&](std::size_t at) { return static_cast<std::uint16_t>(bytes[at] | bytes[at + 1] << 8); };
  const auto u32 = [&](std::size_t at) { return static_cast<std::uint32_t>(u16(at) | u16(at + 2) << 16); };
  for (std::size_t i = 0; i < s.events.size(); ++i) {
    INFO(i);
    const std::size_t at = 2 + 12 * i;
    CHECK(s.events[i].frame == u16(at));
    CHECK(s.events[i].type == u16(at + 2));
    CHECK(s.events[i].a1 == u32(at + 4));
    CHECK(s.events[i].a2 == u32(at + 8));
  }
  // The record past the last one is empty.
  CHECK(u32(2 + 12 * s.events.size()) == 0);
}

TEST_CASE("the original's low scanners", "[content][oracle][gaps]") {
  OA_REQUIRE_GAME_FILES(files);
  // scan2: the `gizgal` scan run with the Frigates' scanners 21, 22, 23
  // (6 and 7 levels against Cloakers counting twice), what the System
  // Display shows of the cloaked Capelon Mediums.
  test::replay_oracle_runs(files, both_builds("scan2", "scan2"));
}

TEST_CASE("the original's Disintegrator and the AI's Positron Bouncer", "[content][oracle][gaps]") {
  OA_REQUIRE_GAME_FILES(files);
  // dis: Frigate 2's Disintegrator on a Capelon Medium within range (ANTAG
  // too), three passes, three days. bounce: the unarmed Mediums threatened
  // by the Frigates, a Positron Bouncer their only evasive gizmo.
  std::vector<test::ReplayRun> runs = both_builds("dis", "dis");
  for (test::ReplayRun& run : both_builds("bounce", "bounce")) {
    runs.push_back(run);
  }
  test::replay_oracle_runs(files, runs);
}

TEST_CASE("the original's Short Range Orbital Whopper", "[content][oracle][gaps]") {
  OA_REQUIRE_GAME_FILES(files);
  // whop_player: Feculon I's Whopper fired by the player (twice in range,
  // once out of range, the third shot). whop_ai: the Capelons' Stella IV
  // (Whopper and Missile Bases) firing at the player's Frigates, no Capelon
  // ship there, in both builds' day battles.
  std::vector<test::ReplayRun> runs = both_builds("whop_player", "whop_player");
  for (test::ReplayRun& run : both_builds("whop_ai", "whop_ai")) {
    runs.push_back(run);
  }
  test::replay_oracle_runs(files, runs);
}

TEST_CASE("the original's planet manager on over-populated planets", "[content][oracle][gaps]") {
  OA_REQUIRE_GAME_FILES(files);
  // overpop: two Capelon planets and the player's Self Managed Feculon I
  // above their maximum population, without a project; five days.
  test::replay_oracle_runs(files, both_builds("overpop", "overpop"));
}

TEST_CASE("the original's diplomacy: two talks on one day and response 11", "[content][oracle][gaps]") {
  OA_REQUIRE_GAME_FILES(files);
  // talk2: the Capelons' and the Orfa's alliance proposals on the same day,
  // the first answered from its box, the second box skipped and the Orfa's
  // proposal (still pending) answered from the species selection. resp11:
  // the allied Capelons asked to ally with the Orfa (accepted, the Orfa
  // ally) and with the Arbryls, whom they like more (refused).
  std::vector<test::ReplayRun> runs = both_builds("talk2", "talk2");
  for (test::ReplayRun& run : both_builds("resp11", "resp11")) {
    runs.push_back(run);
  }
  test::replay_oracle_runs(files, runs);
}

TEST_CASE("the original's colonisation and lane move in the shipped game", "[content][oracle][gaps]") {
  OA_REQUIRE_GAME_FILES(files);
  // colonise (the install's day-820 game, nothing edited): Explorator 1
  // leaves Ajax III, orbits Ajax II, creates a colony there, leaves again
  // and takes the lane to Chippendale; eight days.
  test::replay_oracle_runs(
      files, {{.run = "gaps_colonise_patch", .keep_saves = true, .shots = {}},
              {.run = "gaps_colonise_antag", .variant = game::Variant::kAntagonizer, .keep_saves = true, .shots = {}}});
}

}  // namespace oa
