// The game-event codes (`docs/09-simulation.md` "Event codes",
// `game/events.h`): which oracle runs show which codes. The hidden
// `[.dev]` inventory replays runs in the remake pass by pass and lists the
// codes posted; the `[oracle]` cases replay the runs made to show the codes
// no other run showed (`tests/oracle/cov/endev/`).

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <cstdlib>
#include <deque>
#include <format>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "game_data.h"
#include "replay_rig.h"

namespace oa::test {

namespace {

// A run of the inventory: `ReplayRun`'s fields plus the tutorial handling
// (each Ctrl+P help box closed with a right click, Alt+X once the replay is
// over, as the oracle's `autocontinue` and `quitidle`).
struct InventoryRun {
  ReplayRun run;
  bool tutorial = false;
};

// Parses `spec`: runs separated by `;`, each `RUN[,antag][,keep][,seed=N]
// [,resume=SAVE][,tutorial]` (SAVE under OA_ORACLE).
std::vector<InventoryRun> parse_runs(const std::string& spec) {
  std::vector<InventoryRun> runs;
  std::stringstream list(spec);
  for (std::string entry; std::getline(list, entry, ';');) {
    std::stringstream fields(entry);
    InventoryRun r;
    std::getline(fields, r.run.run, ',');
    if (r.run.run.empty()) {
      continue;
    }
    for (std::string f; std::getline(fields, f, ',');) {
      if (f == "antag") {
        r.run.variant = game::Variant::kAntagonizer;
      } else if (f == "keep") {
        r.run.keep_saves = true;
      } else if (f == "tutorial") {
        r.tutorial = true;
      } else if (f.starts_with("seed=")) {
        r.run.seed = std::stoi(f.substr(5));
      } else if (f.starts_with("resume=")) {
        r.run.resume = f.substr(7);
      } else {
        FAIL("unknown run option " << f);
      }
    }
    runs.push_back(r);
  }
  return runs;
}

// The runs of the inventory without OA_EVENT_RUNS: the event-box runs, the
// corpus replayed by `day_ui_test.cpp` and the tutorials.
std::string default_runs() {
  std::string s =
      "events/boxes_820_patch,keep;events/boxes_new_patch_s1;events/event13_820,resume=exp/event13.GAM;"
      "corpus/resume820_patch,keep;corpus/resume820_daily_patch,keep;corpus/resume820_endlessparty_patch,keep;"
      "corpus/resume820_selfmanaged_patch,keep;corpus/resume820_shipdesign_patch,keep;"
      "corpus/resume820_battle_patch,keep;corpus/actions_patch_s1;corpus/idle_neutral_patch_s1;"
      "corpus/idle_hostile_patch_s1;corpus/idle_peaceful_patch_s1;corpus/idle_neutral_patch_s12345,seed=12345;"
      "corpus/selfmanaged_neutral_patch_s1;corpus/selfmanaged_hostile_patch_s1;corpus/selfmanaged_peaceful_patch_s1;"
      "corpus/small15_7species_patch_s1;corpus/large100_4species_patch_s1;"
      "corpus/idle_neutral_antag_s1,antag;corpus/idle_hostile_antag_s1,antag;corpus/idle_peaceful_antag_s1,antag;"
      "corpus/idle_neutral_antag_s12345,antag,seed=12345;corpus/selfmanaged_neutral_antag_s1,antag;"
      "corpus/selfmanaged_hostile_antag_s1,antag;corpus/selfmanaged_peaceful_antag_s1,antag;"
      "corpus/small15_7species_antag_s1,antag;corpus/large100_4species_antag_s1,antag;corpus/actions_antag_s1,antag;"
      "corpus/resume820_antag,antag,keep;corpus/resume820_daily_antag,antag,keep;"
      "corpus/resume820_selfmanaged_antag,antag,keep;corpus/resume820_battle_antag,antag,keep";
  for (const char* name : {"tutintro", "beginnew", "galactic", "battle", "planet", "ship", "species"}) {
    s += std::format(";tutorials/{},tutorial", name);
  }
  return s;
}

// Replays `r` and returns how many times each event code was posted
// (nothing when the run is missing).
std::map<int, int> posted_codes(const GameFiles& files, const InventoryRun& r) {
  std::map<int, int> codes;
  const ReplayRun& c = r.run;
  const auto trace = oracle_run_path(c.run + "/trace.bin");
  const auto resume = c.resume.empty() ? std::nullopt : oracle_run_path(c.resume);
  if (!trace || (!c.resume.empty() && !resume)) {
    return codes;
  }
  ReplayDir dir;
  const bool antag = c.variant != game::Variant::kStandard;
  if (resume) {
    write_file(dir.path / "RESUME.GAM", as_variant_save(read_file(*resume), c.variant));
  }
  if (antag && c.keep_saves) {
    for (const char* name : {"01.SAV", "RESUME.GAM"}) {
      if (name != std::string_view("RESUME.GAM") || !resume) {
        write_file(dir.path / name, as_variant_save(read_file(*files.install().find(name)), c.variant));
      }
    }
  }
  ReplayProgram program(files, c.seed, dir.path, c.keep_saves && !antag, c.variant);
  REQUIRE(program.wm.start_replay(read_file(*trace), "trace.bin", false));
  // The queue keeps the shown events until it empties; a box's modal loop
  // shows frames, so every event is seen there. The tutorials' help boxes
  // wait in a modal loop too: their input is fed at the frames. Alt+X once
  // no replay has run for 50 passes (a tutorial starts its own replay one
  // pass after the trace's end).
  std::size_t seen = 0;
  std::deque<InputEvent> pending;
  bool quit_sent = false;
  int idle = 0;
  program.on_present = [&] {
    if (const game::Session* core = program.session.core()) {
      const std::vector<game::GameEvent>& events = core->events.events();
      seen = events.size() < seen ? 0 : seen;
      for (; seen < events.size(); ++seen) {
        ++codes[static_cast<int>(events[seen].code)];
      }
    }
    if (r.tutorial && pending.empty() && !program.wm.replaying()) {
      const InputState& s = program.input.state();
      if (program.wm.in_modal()) {
        pending.push_back(InputEvent{.type = EventType::kMouseDown, .x = s.x, .y = s.y, .button = MouseButton::kRight});
        pending.push_back(InputEvent{.type = EventType::kMouseUp, .x = s.x, .y = s.y, .button = MouseButton::kRight});
      } else if (!quit_sent && idle > 50) {
        pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_X, .mods = SDL_KMOD_LALT});
        pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = SDLK_X, .mods = 0});
        quit_sent = true;
      }
    }
    if (!pending.empty()) {
      program.input.feed(pending.front());
      pending.pop_front();
    }
  };
  for (int pass = 0; pass < 400000 && program.wm.running(); ++pass) {
    program.wm.run_pass();
    idle = program.wm.replaying() ? 0 : idle + 1;
  }
  return codes;
}

}  // namespace

TEST_CASE("inventory of the game-event codes posted by oracle runs", "[.dev][events]") {
  OA_REQUIRE_GAME_FILES(files);
  // OA_EVENT_RUNS lists the runs (`parse_runs`), the default ones without.
  const char* env = std::getenv("OA_EVENT_RUNS");
  const std::vector<InventoryRun> runs = parse_runs(env != nullptr && *env != '\0' ? env : default_runs());
  std::map<int, std::vector<std::string>> by_code;
  for (const InventoryRun& r : runs) {
    const std::map<int, int> codes = posted_codes(files, r);
    std::string line = r.run.run + ":";
    for (const auto& [code, n] : codes) {
      line += std::format(" {}x{}", code, n);
      by_code[code].push_back(std::format("{} ({})", r.run.run, n));
    }
    std::cout << line << "\n";
  }
  for (int code = 0; code <= 25; ++code) {
    std::string line = std::format("event {:2}:", code);
    for (const std::string& run : by_code[code]) {
      line += " " + run;
    }
    std::cout << line << "\n";
  }
}

namespace {

// Replays the PATCH and ANTAG runs `cov/endev/<name>_{patch,antag}` resumed
// from `cov/endev/<save>`.
// `dip_arg`: `ReplayRun::dip_arg_low_word` (an AI race talks to the player).
void replay_both_builds(const GameFiles& files, const std::vector<std::pair<std::string, std::string>>& runs,
                        bool dip_arg = false) {
  std::vector<ReplayRun> list;
  for (const auto& [name, save] : runs) {
    list.push_back({.run = std::format("cov/endev/{}_patch", name),
                    .resume = "cov/endev/" + save,
                    .shots = {},
                    .dip_arg_low_word = dip_arg});
    list.push_back({.run = std::format("cov/endev/{}_antag", name),
                    .variant = game::Variant::kAntagonizer,
                    .resume = "cov/endev/" + save,
                    .shots = {},
                    .dip_arg_low_word = dip_arg});
  }
  replay_oracle_runs(files, list);
}

}  // namespace

TEST_CASE("the original's game-event boxes of the codes no other run showed", "[content][oracle][endev]") {
  OA_REQUIRE_GAME_FILES(files);
  // `tests/oracle/cov/endev/cases.txt` on `events_save.py`'s saves: ev_a
  // events 2, 4, 5, 6, 8, 9, 12, 13, 19; ev_b 7, 11; ev_c 0, 1, 10, 16, 18,
  // 20, 21, 22, 24; ev_jump the right-click answers of boxes 9, 2 and 8.
  // The Capelons' offer of peace (ev_a, ev_jump) stores a stale stack word.
  replay_both_builds(files, {{"ev_a", "ev_a.GAM"}, {"ev_b", "ev_b.GAM"}, {"ev_c", "ev_c.GAM"}, {"ev_jump", "ev_a.GAM"}},
                     true);
}

TEST_CASE("the original's event 25: a first Laboratory without research", "[content][oracle][endev]") {
  OA_REQUIRE_GAME_FILES(files);
  // `ev_d_{patch,antag}.txt`: new games of seed 1.
  replay_oracle_runs(files, {{.run = "cov/endev/ev_d_patch", .shots = {}},
                             {.run = "cov/endev/ev_d_antag", .variant = game::Variant::kAntagonizer, .shots = {}}});
}

TEST_CASE("the original's planet items built through the planet screen", "[content][oracle][endev]") {
  OA_REQUIRE_GAME_FILES(files);
  // `items.txt` on `items_save.py`'s save: Orbital Missile Base, Lush Growth
  // Bomb, Automation, Observation Installation, Endless Party, Scientist
  // Takeover and Alien Hospitality chosen, then 26 days.
  replay_both_builds(files, {{"items", "items.GAM"}});
}

}  // namespace oa::test
