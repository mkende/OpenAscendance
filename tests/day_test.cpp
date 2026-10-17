// The day (`Cosmos_end_day` and everything it calls) against the original's
// runs of the oracle corpus (`tests/oracle/corpus/MANIFEST.md`): from the
// run's first save (day 0 of a new game, or the loaded day-820 game), days
// are simulated with the session's generator state and every later save of
// the run is compared byte for byte (the window state and the header's stale
// bytes excepted). Skipped without OA_DATA and OA_ORACLE. With OA_DUMP_DIR
// set, the remake's save of the first diverging day is written there under
// the run's name, for `tools/savdiff.py`.

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "game/battle_control.h"
#include "game/cosmos.h"
#include "game/new_game.h"
#include "game/planet.h"
#include "game/save.h"
#include "game/session.h"
#include "game_oracle.h"

namespace oa::game {
namespace {

using oracle_test::first_difference;
using oracle_test::oracle_dir;
using oracle_test::oracle_file;

// How a corpus run starts.
enum class Start : std::uint8_t {
  kNewGame,  // `cosmos.txt` seed, new-game window, `clicks`, Begin
  kResume,   // `cosmos.txt` seed 1 start-up galaxy, then `RESUME.GAM` loaded
};

struct DayCase {
  std::string run;  // under OA_ORACLE
  Variant variant;
  Start start;
  std::int32_t seed = 1;
  std::function<void(Session&, NewGameSetup&)> clicks = [](Session&, NewGameSetup&) {};
  // The player's actions on the day of save `slot`, before it.
  std::function<void(Session&, int slot)> actions = [](Session&, int) {};
};

// `actions_*_s1` on day 60: the build list's Laboratory placed on the
// clicked square of the player's planet (47 in the PATCH galaxy, 7 in the
// ANTAG one).
std::function<void(Session&, int)> build_laboratory_on_day_60(std::uint16_t square) {
  return [square](Session& session, int slot) {
    if (slot != 8) {
      return;
    }
    Cosmos& cosmos = session.game->cosmos;
    for (std::int16_t i = 0; i < cosmos.planet_count; ++i) {
      Planet& planet = cosmos.planets[static_cast<std::size_t>(i)];
      if (planet.owner == session.player_race) {
        REQUIRE(planet_set_square_item(session, planet, square, 2, SquareMode::kProject));
        return;
      }
    }
  };
}

// The battle of `resume820_battle_*`: Next Turn, "Go to System" on the first
// contested star, Auto until the turns stop, back to the star map (03.SAV,
// still day 820).
void auto_battle_at_first_contested_star(Session& session, int slot) {
  if (slot != 3) {
    return;
  }
  const StarIndex star = cosmos_next_contested_star(session);
  REQUIRE(star != kNone);
  BattleControl control;
  control.open(session, session.game->cosmos.stars[static_cast<std::size_t>(star)]);
  REQUIRE(control.start_auto(session));
  control.finish_all(session);
  control.close(session);
}

// Ends one day as the next-turn command of the oracle's scripts does (every
// contested-star prompt answered "move to the next day anyway"); the event
// boxes are closed.
void next_turn(Session& session) {
  cosmos_next_turn(session);
  session.events.take();
}

// Returns the bytes of the session's save with the original's window state
// and stale header bytes, without the variant trailer.
std::vector<std::uint8_t> save_like(Session& session, const SaveImage& original) {
  session.window_state = original.window_state;
  session.stale_header = original.header;
  std::vector<std::uint8_t> bytes = serialize_save(save_game(session));
  if (session.game->variant != Variant::kStandard) {
    bytes.resize(bytes.size() - 8);
  }
  return bytes;
}

// Runs one corpus case; returns false when its first save is missing.
bool run_case(const GameTables& tables, const DayCase& c) {
  const auto first = oracle_file(c.run + "/out/02.SAV");
  if (!first) {
    return false;
  }
  // The ANTAG saves of the day-820 game (a PATCH save loaded by ANTAG) keep
  // the PATCH file's stale pointers (past the list counts): they are parsed
  // against the PATCH cosmos too and compared as the remake writes them.
  const bool foreign = c.start == Start::kResume && c.variant == Variant::kAntagonizer;
  const std::optional<RawAddress> foreign_cosmos =
      foreign ? std::optional<RawAddress>(kPatchCosmosAddress) : std::nullopt;
  Session session(tables);
  session.game->variant = c.variant;
  const CosmosTxt txt{.stars = 50, .races = 7, .seed = c.seed, .species = {}};
  if (c.start == Start::kNewGame) {
    program_start(session, 0, &txt, false);
    NewGameSetup setup;
    setup.activate(session);
    c.clicks(session, setup);
    setup.begin(session);
  } else {
    program_start(session, 0, &txt, true);
  }
  // The first save carries the player's actions before it (projects,
  // self-managed planets and research, ship designs): none draws.
  const SaveImage start = parse_save(*first, foreign_cosmos);
  load_game(session, start);
  session.game->variant = c.variant;
  // The original process's own runtime address (it varies between runs).
  session.cosmos_address = start.cosmos_address;
  for (int slot = 3; slot <= 10; ++slot) {
    const std::string name = std::format("{:02d}.SAV", slot);
    auto file = oracle_file(c.run + "/out/" + name);
    if (!file) {
      break;
    }
    const SaveImage original = parse_save(*file, foreign_cosmos);
    if (foreign) {
      file = serialize_save(original);
    }
    while (session.game->cosmos.day < original.day) {
      next_turn(session);
    }
    c.actions(session, slot);
    INFO(c.run << " " << name << " day " << original.day);
    const std::vector<std::uint8_t> ours = save_like(session, original);
    const std::ptrdiff_t diff = first_difference(ours, *file);
    CHECK(diff == -1);
    if (diff != -1) {
      if (const char* dump = std::getenv("OA_DUMP_DIR"); dump != nullptr && *dump != '\0') {
        const std::filesystem::path dir = std::filesystem::path(dump) / c.run;
        std::filesystem::create_directories(dir);
        std::ofstream(dir / name, std::ios::binary)
            .write(reinterpret_cast<const char*>(ours.data()), static_cast<std::streamsize>(ours.size()));
        // The original as compared (differs from its file for `foreign`).
        std::ofstream(dir / (name + ".orig"), std::ios::binary)
            .write(reinterpret_cast<const char*>(file->data()), static_cast<std::streamsize>(file->size()));
      }
      break;  // later days follow from a wrong state
    }
  }
  return true;
}

// The cases of `tests/oracle/corpus/cases.txt` with more than one save, for
// `variant`.
std::vector<DayCase> corpus_cases(Variant variant) {
  const std::string exe = variant == Variant::kStandard ? "patch" : "antag";
  const auto hostile = [](Session&, NewGameSetup& g) { g.next_atmosphere(); };
  const auto peaceful = [](Session&, NewGameSetup& g) {
    g.next_atmosphere();
    g.next_atmosphere();
  };
  // 15 stars, 7 races.
  const auto small = [](Session& s, NewGameSetup& g) {
    for (int i = 0; i < 3; ++i) {
      g.next_density(s);
    }
    g.next_race_count(s);
    g.next_race_count(s);
  };
  // 100 stars, 4 races, Snovemdomas, colour 3.
  const auto large = [](Session& s, NewGameSetup& g) {
    g.next_density(s);
    g.next_density(s);
    for (int i = 0; i < 4; ++i) {
      g.next_race_count(s);
    }
    g.pick_species(s, 1);
    g.pick_color(s, 3);
  };
  std::vector<DayCase> cases;
  const auto add = [&](const std::string& run, Start start, std::int32_t seed,
                       std::function<void(Session&, NewGameSetup&)> clicks = {}) {
    DayCase c{.run = "corpus/" + run, .variant = variant, .start = start, .seed = seed};
    if (clicks) {
      c.clicks = std::move(clicks);
    }
    cases.push_back(std::move(c));
  };
  add(std::format("idle_neutral_{}_s1", exe), Start::kNewGame, 1);
  add(std::format("idle_neutral_{}_s12345", exe), Start::kNewGame, 12345);
  add(std::format("idle_hostile_{}_s1", exe), Start::kNewGame, 1, hostile);
  add(std::format("idle_peaceful_{}_s1", exe), Start::kNewGame, 1, peaceful);
  add(std::format("selfmanaged_neutral_{}_s1", exe), Start::kNewGame, 1);
  add(std::format("selfmanaged_hostile_{}_s1", exe), Start::kNewGame, 1, hostile);
  add(std::format("selfmanaged_peaceful_{}_s1", exe), Start::kNewGame, 1, peaceful);
  add(std::format("small15_7species_{}_s1", exe), Start::kNewGame, 1, small);
  add(std::format("large100_4species_{}_s1", exe), Start::kNewGame, 1, large);
  add(std::format("actions_{}_s1", exe), Start::kNewGame, 1);
  cases.back().actions = build_laboratory_on_day_60(variant == Variant::kStandard ? 47 : 7);
  add(std::format("resume820_{}", exe), Start::kResume, 1);
  add(std::format("resume820_daily_{}", exe), Start::kResume, 1);
  add(std::format("resume820_selfmanaged_{}", exe), Start::kResume, 1);
  add(std::format("resume820_battle_{}", exe), Start::kResume, 1);
  cases.back().actions = auto_battle_at_first_contested_star;
  if (variant == Variant::kStandard) {
    add("resume820_endlessparty_patch", Start::kResume, 1);
    add("resume820_shipdesign_patch", Start::kResume, 1);
  }
  return cases;
}

void run_corpus(Variant variant) {
  OA_REQUIRE_TABLES(t);
  if (!oracle_dir()) {
    SKIP("OA_ORACLE is not set to work/oracle");
  }
  for (const DayCase& c : corpus_cases(variant)) {
    DYNAMIC_SECTION(c.run) {
      if (!run_case(*t, c)) {
        SKIP(c.run << " is not in OA_ORACLE");
      }
    }
  }
}

TEST_CASE("Standard days are the original's (PATCH corpus)", "[game][content][oracle][day]") {
  run_corpus(Variant::kStandard);
}

TEST_CASE("Antagonizer days are the original's (ANTAG corpus)", "[game][content][oracle][day][antag]") {
  run_corpus(Variant::kAntagonizer);
}

}  // namespace
}  // namespace oa::game
