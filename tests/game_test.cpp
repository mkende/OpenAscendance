// The headless game core: tables, the save format, the galaxy generation
// against the original's saves (OA_DATA install, OA_ORACLE corpus).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/error.h"
#include "game/float_math.h"
#include "game/galaxy.h"
#include "game/new_game.h"
#include "game/save.h"
#include "game/session.h"
#include "game/tables.h"
#include "game_data.h"

namespace oa::game {
namespace {

// Returns the tables of the OA_DATA install, nothing when it is unset.
const GameTables* install_tables() {
  static const std::optional<GameTables> tables = []() -> std::optional<GameTables> {
    const auto install = test::game_install();
    if (!install) {
      return std::nullopt;
    }
    return load_game_tables(GameFiles(*install));
  }();
  return tables ? &*tables : nullptr;
}

#define OA_REQUIRE_TABLES(tables)                        \
  const GameTables* tables = install_tables();           \
  if (tables == nullptr) {                               \
    SKIP("OA_DATA is not set to an Ascendancy install"); \
  }

// Returns the file `relative` of the OA_ORACLE directory (`work/oracle`),
// nothing when OA_ORACLE is unset or the file is missing.
std::optional<std::vector<std::uint8_t>> oracle_file(const std::string& relative) {
  const char* dir = std::getenv("OA_ORACLE");
  if (dir == nullptr || *dir == '\0') {
    return std::nullopt;
  }
  const std::filesystem::path path = std::filesystem::path(dir) / relative;
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  return read_file(path);
}

// Returns the first offset where `a` and `b` differ, or -1.
std::ptrdiff_t first_difference(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    if (a[i] != b[i]) {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return a.size() == b.size() ? -1 : static_cast<std::ptrdiff_t>(std::min(a.size(), b.size()));
}

TEST_CASE("snap_to_grid rounds to the nearest multiple, halves away from zero", "[game]") {
  Vec3 v = {47.9f, -16.0f, 15.9f};
  snap_to_grid(v, 32);
  CHECK(v == Vec3{32.0f, -32.0f, 0.0f});
}

TEST_CASE("append_roman_numeral strcats the orbit numeral", "[game]") {
  std::array<char, 30> name{};
  std::memcpy(name.data(), "Vega I\0stale", 12);
  name[5] = '\0';  // "Vega "
  append_roman_numeral(name, 4);
  CHECK(name_view(name) == "Vega IV");
  CHECK(name[8] == 't');  // the tail keeps its bytes
}

TEST_CASE("the install's tables", "[game][content]") {
  OA_REQUIRE_TABLES(t);
  CHECK(t->planitems.pop_growth == 50);
  std::array<int, 5> surface{};
  for (std::size_t s = 0; s < 5; ++s) {
    for (const std::uint8_t h : t->planitems.column_height[s]) {
      surface[s] += h;
    }
  }
  CHECK(surface == std::array<int, 5>{7, 11, 23, 45, 73});
  CHECK(t->planitems.surface_count == 22);
  CHECK(t->planitems.orbital_count == 9);
  CHECK((t->planitems.items[kItemColonyBase].flags & kItemAnySquare) != 0);
  CHECK(t->gizmos[0x2b].name == "Star Lane Drive");
  CHECK(t->research.count == 66);
  CHECK(t->research.items[11].cost == 280);
  CHECK(t->research.items[65].cost == 33000);
  CHECK(name_view(t->research.items[0].name) == "Orbital Structures");
  CHECK(t->star_names[0] == "Vega");
  CHECK(t->dip[5].declare_war == 300);
  CHECK(t->dip[5].bio_sin_x == -3.0f);
  CHECK(t->dip[5].neg_player_turns == 91);
}

TEST_CASE("the shipped saves round-trip byte for byte", "[game][content]") {
  const auto install = test::game_install();
  if (!install) {
    SKIP("OA_DATA is not set to an Ascendancy install");
  }
  for (const char* name : {"01.SAV", "RESUME.GAM"}) {
    INFO(name);
    const std::vector<std::uint8_t> file = install->read(name);
    const SaveImage image = parse_save(file);
    CHECK(image.cosmos_address == kPatchCosmosAddress);
    CHECK(image.day == 820);
    CHECK(image.stars.size() == 50);
    CHECK(image.planets.size() == 156);
    CHECK(image.ships.size() == 22);
    CHECK(image.variant == Variant::kStandard);
    CHECK(first_difference(serialize_save(image), file) == -1);
  }
}

TEST_CASE("loading then saving 01.SAV zeroes the AI target counts and keeps the session's clusters",
          "[game][content]") {
  OA_REQUIRE_TABLES(t);
  const auto install = test::game_install();
  const std::vector<std::uint8_t> file = install->read("01.SAV");
  const SaveImage image = parse_save(file);
  Session session(*t);
  load_game(session, image);
  // Race_serialize zeroes the target count on load, and Cosmos_load leaves
  // the in-memory clusters (here the fresh session's) in place.
  SaveImage expected = image;
  for (Race& race : expected.races) {
    race.target_count = 0;
  }
  expected.clusters.assign(expected.clusters.size(), Cluster{});
  CHECK(first_difference(serialize_save(save_game(session)), serialize_save(expected)) == -1);
  CHECK(image.races[1].target_count == 3);
}

TEST_CASE("the variant is a trailer after the window state, absent for Standard", "[game][content]") {
  const auto install = test::game_install();
  if (!install) {
    SKIP("OA_DATA is not set to an Ascendancy install");
  }
  const std::vector<std::uint8_t> file = install->read("01.SAV");
  SaveImage image = parse_save(file);
  image.variant = Variant::kAntagonizer;
  std::vector<std::uint8_t> bytes = serialize_save(image);
  REQUIRE(bytes.size() == file.size() + 8);
  CHECK(std::equal(file.begin(), file.end(), bytes.begin()));
  CHECK(parse_save(bytes).variant == Variant::kAntagonizer);
  bytes.back() = 9;
  CHECK_THROWS_AS(parse_save(bytes), DataError);
}

// A new game of the oracle corpus: `cosmos.txt` with 50 stars, 7 races and
// the seed, the new-game window opened, `clicks` on its controls, Begin.
struct OracleNewGame {
  std::string save;  // under OA_ORACLE
  Variant variant;   // PATCH or ANTAG run
  std::int32_t seed;
  std::function<void(Session&, NewGameSetup&)> clicks;
};

TEST_CASE("new games are the original's day-0 saves", "[game][content][oracle]") {
  OA_REQUIRE_TABLES(t);
  const auto none = [](Session&, NewGameSetup&) {};
  // 15 stars, 7 races: the home-star spread loop at its tightest.
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
  std::vector<OracleNewGame> cases;
  for (const auto& [exe, variant] :
       {std::pair{"patch", Variant::kStandard}, std::pair{"antag", Variant::kAntagonizer}}) {
    for (const std::int32_t seed : {1, 2, 3, 12345}) {
      cases.push_back({std::format("corpus/day0_{}_s{}/out/02.SAV", exe, seed), variant, seed, none});
    }
    cases.push_back({std::format("corpus/small15_7species_{}_s1/out/02.SAV", exe), variant, 1, small});
    cases.push_back({std::format("corpus/large100_3species_{}_s1/out/02.SAV", exe), variant, 1, large});
  }
  std::size_t found = 0;
  for (const OracleNewGame& c : cases) {
    const auto file = oracle_file(c.save);
    if (!file) {
      continue;
    }
    ++found;
    INFO(c.save);
    const SaveImage original = parse_save(*file);
    Session session(*t);
    session.game->variant = c.variant;
    const CosmosTxt txt{.stars = 50, .races = 7, .seed = c.seed, .species = {}};
    program_start(session, 0, &txt, false);
    NewGameSetup setup;
    setup.activate(session);
    c.clicks(session, setup);
    setup.begin(session);
    // Not generated here: the user interface's window state and the stale
    // stack bytes of the header (per-race entries past the race count, the
    // last 8 bytes).
    session.window_state = original.window_state;
    session.stale_header = original.header;
    std::vector<std::uint8_t> bytes = serialize_save(save_game(session));
    if (c.variant != Variant::kStandard) {
      // The original's file has no variant trailer.
      REQUIRE(bytes.size() == file->size() + 8);
      bytes.resize(file->size());
    }
    CHECK(first_difference(bytes, *file) == -1);
  }
  if (found == 0) {
    SKIP("OA_ORACLE is not set to work/oracle with the new-game corpus");
  }
}

}  // namespace
}  // namespace oa::game
