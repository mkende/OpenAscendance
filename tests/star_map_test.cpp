// The star map (`CosWnd`, screens 1, 3, 7) and the game session over the
// game core: the original's pixels of the new-game screen and the day-0
// star map, its projection, controls and draw order, the window state of the
// saves, the next-day buttons (OA_DATA install; the comparisons with the
// original's runs need OA_ORACLE, `docs/12-oracle.md`).

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "assets/gif.h"
#include "assets/winevent.h"
#include "core/error.h"
#include "game/cosmos.h"
#include "game/new_game.h"
#include "game/save.h"
#include "game_data.h"
#include "screens/cos_wnd.h"
#include "ui/game_core_session.h"
#include "ui/toggle_button.h"
#include "ui_rig.h"

namespace oa {
namespace {

// A folder of its own, removed afterwards.
struct TempDir {
  TempDir() {
    std::random_device rd;
    path = std::filesystem::temp_directory_path() / std::format("oa_star_map_{:08x}", rd());
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  std::filesystem::path path;
};

// Returns the file `relative` of OA_ORACLE (`work/oracle`), nothing when
// unset or missing.
std::optional<std::filesystem::path> oracle_path(const std::string& relative) {
  const char* dir = std::getenv("OA_ORACLE");
  if (dir == nullptr || *dir == '\0') {
    return std::nullopt;
  }
  const std::filesystem::path path = std::filesystem::path(dir) / relative;
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  return path;
}

// A session started as the oracle's runs (`cosmos.txt`: 50 stars, 7 races,
// `seed`), saving into `dir`, under the window manager.
struct StarMapRig {
  StarMapRig(const GameFiles& files, std::int32_t seed, const std::filesystem::path& dir)
      : session(files, dir, false), rig((start(seed), files), &session, dir) {}

  // The session's start-up galaxy (before the windows, as `main`).
  void start(std::int32_t seed) {
    const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = seed, .species = {}};
    session.start(0, &txt);
  }

  // The steps of the oracle's scripts (`tools/oracle.py`), a pass each.
  void wait(int passes) {
    for (int i = 0; i < passes; ++i) {
      rig.pass();
    }
  }
  void click(int x, int y, MouseButton button = MouseButton::kLeft) {
    rig.move(x, y);
    rig.click(x, y, button);
  }

  CosWnd& map() { return dynamic_cast<CosWnd&>(*rig.wm.find_window("COSMOSWnd")); }

  // The new game of `day0_default.txt` up to the new-game screen: its
  // screenshot is the frame of the next pass (Alt+G reads the screen,
  // drawn at the end of the pass before).
  void to_new_game_screen() {
    wait(40);
    click(318, 295);  // FRESHGAME
    wait(60);
  }
  // Then Begin, the species history and the star map.
  void to_star_map() {
    rig.pass();  // the Alt+G pass of the script
    wait(2);
    click(594, 452);  // Begin New Game
    wait(60);
    click(320, 240);  // the species history
    wait(60);
  }

  GameCoreSession session;
  test::UiRig rig;
};

// Counts the pixels of `screen` differing from the original's screenshot
// `gif`, outside the pointer at (px, py) (the original's screenshots show
// it).
int differing_pixels(const Image8& screen, const std::filesystem::path& gif, int px, int py) {
  const Image8 original = decode_gif(read_file(gif), gif.string());
  int n = 0;
  for (int y = 0; y < original.height(); ++y) {
    for (int x = 0; x < original.width(); ++x) {
      const bool pointer = x >= px - 16 && x <= px + 16 && y >= py - 16 && y <= py + 16;
      if (!pointer && original.at(x, y) != screen.at(x, y)) {
        ++n;
      }
    }
  }
  return n;
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

TEST_CASE("new games: the original's new-game screen, day-0 star map and save", "[screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  int found = 0;
  for (const int seed : {1, 2, 3, 12345}) {
    const std::string run = std::format("corpus/day0_patch_s{}/out/", seed);
    const auto new_game_shot = oracle_path(run + "SCR000.GIF");
    const auto star_map_shot = oracle_path(run + "SCR001.GIF");
    const auto save = oracle_path(run + "02.SAV");
    if (!new_game_shot || !star_map_shot || !save) {
      continue;
    }
    ++found;
    INFO(run);
    TempDir dir;
    StarMapRig r(files, seed, dir.path);
    r.to_new_game_screen();
    CHECK(differing_pixels(r.rig.wm.screen(), *new_game_shot, 318, 295) == 0);
    r.to_star_map();
    REQUIRE(r.rig.wm.current_screen() == 1);
    CHECK(differing_pixels(r.rig.wm.screen(), *star_map_shot, 320, 240) == 0);
    REQUIRE(r.session.save(2));
    std::vector<std::uint8_t> ours = read_file(dir.path / "02.SAV");
    std::vector<std::uint8_t> theirs = read_file(*save);
    REQUIRE(ours.size() == theirs.size());
    clear_stale_header(ours);
    clear_stale_header(theirs);
    CHECK(ours == theirs);
  }
  if (found == 0) {
    SKIP("OA_ORACLE is not set to work/oracle with the day-0 corpus");
  }
}

TEST_CASE("the windows' state of the saves round-trips through the windows", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  std::vector<std::filesystem::path> saves;
  for (const char* shipped : {"01.SAV", "RESUME.GAM"}) {
    if (const auto path = files.install().find(shipped)) {
      saves.push_back(*path);
    }
  }
  if (const auto corpus = oracle_path("corpus")) {
    for (const auto& run : std::filesystem::directory_iterator(*corpus)) {
      for (const char* name : {"out/02.SAV", "out/RESUME.GAM"}) {
        if (std::filesystem::exists(run.path() / name)) {
          saves.push_back(run.path() / name);
        }
      }
    }
  }
  REQUIRE_FALSE(saves.empty());
  TempDir dir;
  StarMapRig r(files, 1, dir.path);
  for (const std::filesystem::path& path : saves) {
    INFO(path.string());
    const std::vector<std::uint8_t> original = read_file(path);
    try {
      game::parse_save(original);
    } catch (const DataError& e) {
      WARN("not a save the game core reads: " << e.what());
      continue;
    }
    std::filesystem::copy_file(path, dir.path / "03.SAV", std::filesystem::copy_options::overwrite_existing);
    REQUIRE(r.session.load(3));
    REQUIRE(r.session.save(4));
    const std::vector<std::uint8_t> saved = read_file(dir.path / "04.SAV");
    // The Antagonizer games get the variant trailer after the window state.
    const std::size_t end = std::min(original.size(), saved.size());
    REQUIRE(end >= game::kWindowStateSize);
    std::vector<std::uint8_t> a(original.end() - static_cast<std::ptrdiff_t>(game::kWindowStateSize), original.end());
    std::vector<std::uint8_t> b(saved.begin() + static_cast<std::ptrdiff_t>(original.size()) -
                                    static_cast<std::ptrdiff_t>(game::kWindowStateSize),
                                saved.begin() + static_cast<std::ptrdiff_t>(original.size()));
    // The star map's 4 stale stack bytes (`CosWnd::save_state`) come from
    // the process's clusters, not from the loaded file.
    std::fill_n(a.begin() + 0xba, 4, 0);
    std::fill_n(b.begin() + 0xba, 4, 0);
    CHECK(a == b);
  }
}

TEST_CASE("the star map's default camera and its species circle", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  TempDir dir;
  StarMapRig r(files, 1234, dir.path);
  r.to_new_game_screen();
  r.to_star_map();
  const CosWnd& map = r.map();
  // CosWnd_reset_camera: fov 35, the focal length 229 / tan(17.5 degrees)
  // (726.2952, the original saves' bits), 1500 units from the centre.
  CHECK(map.fov() == 35.0F);
  CHECK(std::bit_cast<std::uint32_t>(map.camera().focal) == 0x443592e5U);
  CHECK(map.camera().view == std::array<float, 12>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1500});
  // A star at the galaxy's centre plane projects at x f / (z + 1500).
  for (const std::int32_t k : map.draw_list()) {
    const CosWnd::Item& it = map.items()[static_cast<std::size_t>(k)];
    if (it.kind == CosWnd::ItemKind::kStar) {
      const game::Vec3& p = r.session.core()->game->cosmos.stars[static_cast<std::size_t>(it.index)].position;
      CHECK(it.depth == p[2] + 1500.0F);
      const double inv = 1.0 / static_cast<double>(it.depth);
      CHECK(it.x == 229 + game::chop(static_cast<double>(p[0]) * map.camera().focal * inv + 0.5));
      CHECK(it.y == 232 + game::chop(static_cast<double>(map.camera().focal) * p[1] * inv + 0.5));
      break;
    }
  }
  // The species circle (`0xc8452`): 170 pixels around (229, 232), 72
  // degrees apart for five races, from the top, counter-clockwise.
  CHECK(map.circle()[0].x == 229);
  CHECK(map.circle()[0].y == 62);
  CHECK(map.circle()[1].x == 67);
  CHECK(map.circle()[1].y == 179);
}

TEST_CASE("the star map's items are drawn farthest first", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  TempDir dir;
  StarMapRig r(files, 2, dir.path);
  r.to_new_game_screen();
  r.to_star_map();
  const CosWnd& map = r.map();
  // 50 stars, the player's home star with its ships' marker, the lanes it
  // knows.
  REQUIRE(map.draw_list().size() > 50);
  for (std::size_t i = 1; i < map.draw_list().size(); ++i) {
    const float before = map.items()[static_cast<std::size_t>(map.draw_list()[i - 1])].depth;
    const float after = map.items()[static_cast<std::size_t>(map.draw_list()[i])].depth;
    CHECK(before >= after);
  }
  // The ship marker after a star: right of it, one unit nearer.
  for (std::size_t k = 1; k < map.items().size(); ++k) {
    const CosWnd::Item& it = map.items()[k];
    if (it.kind == CosWnd::ItemKind::kShip && map.items()[k - 1].kind == CosWnd::ItemKind::kStar) {
      CHECK(it.depth == map.items()[k - 1].depth + 1.0F);
      CHECK(it.x > map.items()[k - 1].x);
    }
  }
}

TEST_CASE("the rotation and zoom buttons latch, stop and reverse; the zoom stops past its limits",
          "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  TempDir dir;
  StarMapRig r(files, 1234, dir.path);
  r.to_new_game_screen();
  r.to_star_map();
  CosWnd& map = r.map();
  const auto rclick = [&](std::string_view name) {
    const Pane& p = r.rig.wm.find_window(name)->pane();
    r.click((p.x0 + p.x1) / 2, (p.y0 + p.y1) / 2, MouseButton::kRight);
  };
  rclick("ROTLEFT");
  CHECK(map.actions() == CosWnd::kActionRotateY);
  rclick("ROTRIGHT");  // reversed
  CHECK(map.actions() == (CosWnd::kActionRotateY | CosWnd::kActionRight));
  rclick("ROTRIGHT");  // stopped
  CHECK(map.actions() == CosWnd::kActionRight);
  CHECK(map.camera().orientation != std::array<float, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1});

  rclick("ZOOMIN");
  r.wait(30);
  // 35, 34 .. 20, 19: below 20 the next step stops the zoom.
  CHECK(map.fov() == 19.0F);
  CHECK((map.actions() & CosWnd::kActionZoom) == 0);
  rclick("ZOOMOUT");
  r.wait(50);
  CHECK(map.fov() == 61.0F);
  CHECK((map.actions() & CosWnd::kActionZoom) == 0);

  rclick("CENTERHOLO");
  CHECK(map.fov() == 35.0F);
  CHECK(map.camera().orientation == std::array<float, 9>{1, 0, 0, 0, 1, 0, 0, 0, 1});
}

TEST_CASE("hovering a star names it; a click on an explored star opens its system", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  TempDir dir;
  StarMapRig r(files, 1234, dir.path);
  r.to_new_game_screen();
  r.to_star_map();
  CosWnd& map = r.map();
  const game::Session& s = *r.session.core();
  const game::StarIndex home = s.game->cosmos.races[s.player_race].home;
  const CosWnd::Item* item = nullptr;
  for (const CosWnd::Item& it : map.items()) {
    if (it.kind == CosWnd::ItemKind::kStar && it.index == home) {
      item = &it;
    }
  }
  REQUIRE(item != nullptr);
  const int x = map.pane().x0 + item->x;
  const int y = map.pane().y0 + item->y;
  r.rig.move(x + 2, y + 1);
  REQUIRE(map.hovered() != -1);
  CHECK(map.items()[static_cast<std::size_t>(map.hovered())].index == home);
  CHECK(r.rig.pointer_frame == 3);  // CURSORSHAPE + 1
  r.rig.click(x + 2, y + 1);
  r.rig.pass();
  CHECK(s.game->cosmos.selected_star == home);
  CHECK(r.rig.wm.current_screen() == 16);
}

// The index of the star item of `star` on `map`.
std::size_t star_item(const CosWnd& map, game::StarIndex star) {
  for (std::size_t k = 0; k < map.items().size(); ++k) {
    if (map.items()[k].kind == CosWnd::ItemKind::kStar && map.items()[k].index == star) {
      return k;
    }
  }
  FAIL("no item for star " << star);
  return 0;
}

// The star map of a new game (seed 1234) with its player's home star.
struct HomeRig {
  explicit HomeRig(const GameFiles& files) : r(files, 1234, dir.path) {
    r.to_new_game_screen();
    r.to_star_map();
    REQUIRE(r.rig.wm.current_screen() == 1);
  }

  game::Cosmos& cosmos() { return r.session.core()->game->cosmos; }
  game::StarIndex home() { return cosmos().races[r.session.core()->player_race].home; }
  // The map rebuilt and projected again (a new day's refresh).
  void refresh() { r.rig.wm.send_event(r.map().id(), kEventRefresh, 0, 0); }
  // Moves the pointer away from the map's items, then to the window-relative
  // point (x, y).
  void hover(int x, int y) {
    r.rig.move(600, 300);
    r.rig.move(r.map().pane().x0 + x, r.map().pane().y0 + y);
  }
  // Clicks at the window-relative point (x, y), then runs the posted
  // messages.
  void click(int x, int y) {
    r.rig.click(r.map().pane().x0 + x, r.map().pane().y0 + y);
    r.rig.pass();
  }

  TempDir dir;
  StarMapRig r;
};

TEST_CASE("the remake picks the only star within 5 pixels of the original's radius", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  HomeRig h(files);
  CosWnd& map = h.r.map();
  const std::size_t home = star_item(map, h.home());
  const CosWnd::Item star = map.items()[home];
  // 9 pixels left of the home star (81 > 40), no other star within 12.
  const int x = star.x - 9;
  const int y = star.y;
  for (const CosWnd::Item& it : map.items()) {
    if (it.kind == CosWnd::ItemKind::kStar && it.index != h.home()) {
      REQUIRE((it.x - x) * (it.x - x) + (it.y - y) * (it.y - y) > 144);
    }
  }
  h.r.rig.wm.set_enlarged_targets(false);
  h.hover(x, y);
  CHECK(map.hovered() == -1);
  h.r.rig.wm.set_enlarged_targets(true);
  h.hover(x, y);
  CHECK(map.hovered() == static_cast<int>(home));
  CHECK(h.r.rig.pointer_frame == 3);  // CURSORSHAPE + 1
  // Kept while the pointer moves within the enlarged radius.
  h.r.rig.move(map.pane().x0 + x - 1, map.pane().y0 + y + 1);
  CHECK(map.hovered() == static_cast<int>(home));
  // 12 pixels away (144 > (sqrt 40 + 5)^2): none.
  h.hover(star.x - 12, y);
  CHECK(map.hovered() == -1);
  CHECK(h.r.rig.pointer_frame == 2);
  h.hover(x, y);
  h.click(x, y);
  CHECK(h.cosmos().selected_star == h.home());
  CHECK(h.r.rig.wm.current_screen() == 16);
}

TEST_CASE("the remake picks no star when two are within its enlarged radius", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  HomeRig h(files);
  // Another star (without ships) put 20 units right of the home star:
  // about 10 pixels apart on the map.
  game::Cosmos& c = h.cosmos();
  game::StarIndex other = 0;
  while (other == h.home() || c.stars[static_cast<std::size_t>(other)].ships_mask != 0) {
    ++other;
  }
  const game::Vec3& at = c.stars[static_cast<std::size_t>(h.home())].position;
  c.stars[static_cast<std::size_t>(other)].position = {at[0] + 20.0F, at[1], at[2]};
  h.refresh();
  CosWnd& map = h.r.map();
  const CosWnd::Item a = map.items()[star_item(map, h.home())];
  const CosWnd::Item b = map.items()[star_item(map, other)];
  // Below the middle of the two: within the enlarged radius of both.
  const int x = (a.x + b.x) / 2;
  const int y = a.y + 7;
  for (const CosWnd::Item& it : {a, b}) {
    const int d = (it.x - x) * (it.x - x) + (it.y - y) * (it.y - y);
    REQUIRE(d >= 40);
    REQUIRE(d <= 128);
  }
  h.hover(x, y);
  CHECK(map.hovered() == -1);
  h.click(x, y);
  CHECK(h.r.rig.wm.current_screen() == 1);
}

TEST_CASE("a click on the ships at a star opens it; one on a ship in transit does nothing", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  HomeRig h(files);
  // A ship of the player in its home system (none on day 0).
  game::Session& session = *h.r.session.core();
  game::Ship* ship = game::cosmos_alloc_ship(session, session.player_race);
  REQUIRE(ship != nullptr);
  ship->location_kind = game::ShipLocation::kSystem;
  game::ship_enter_star_system(session, h.cosmos().stars[static_cast<std::size_t>(h.home())], *ship);
  h.refresh();
  CosWnd& map = h.r.map();
  const std::size_t home = star_item(map, h.home());
  // The home star's ships marker, right after it; its hover point is 5
  // pixels right and 2 up of the item.
  REQUIRE(home + 1 < map.items().size());
  const CosWnd::Item marker = map.items()[home + 1];
  REQUIRE(marker.kind == CosWnd::ItemKind::kShip);
  const int x = marker.x + 5;
  const int y = marker.y - 2;

  SECTION("the original's: the ship hovered, its click does nothing") {
    h.r.rig.wm.set_enlarged_targets(false);
    h.hover(x, y);
    CHECK(map.hovered() == static_cast<int>(home + 1));
    h.click(x, y);
    CHECK(h.r.rig.wm.current_screen() == 1);
  }
  SECTION("never while a trace is replayed") {
    std::vector<std::uint8_t> trace(Trace::kHeaderSize, 0);
    const auto sx = static_cast<std::uint32_t>(map.pane().x0 + x);
    const auto sy = static_cast<std::uint32_t>(map.pane().y0 + y);
    test::append_trace_record(trace, 1, TraceEventType::kMouseMove, sx, sy);
    test::append_trace_record(trace, 2, TraceEventType::kMouseButton, 0, 1);
    test::append_trace_record(trace, 3, TraceEventType::kMouseButton, 0, 0);
    test::append_trace_record(trace, 30, TraceEventType::kMouseMove, sx, sy);
    REQUIRE(h.r.rig.wm.start_replay(trace, "test.bin", false));
    h.r.wait(8);
    REQUIRE(h.r.rig.wm.replaying());
    CHECK(map.hovered() == static_cast<int>(home + 1));
    CHECK(h.r.rig.wm.current_screen() == 1);
  }
  SECTION("the remake's: the star hovered (its label), the click opens it") {
    h.hover(x, y);
    CHECK(map.hovered() == static_cast<int>(home));
    CHECK(h.r.rig.pointer_frame == 3);
    h.click(x, y);
    CHECK(h.cosmos().selected_star == h.home());
    CHECK(h.r.rig.wm.current_screen() == 16);
  }
  SECTION("a ship in transit: picked only while still at its departure star") {
    game::Cosmos& c = h.cosmos();
    const game::Star& star = c.stars[static_cast<std::size_t>(h.home())];
    REQUIRE(star.lane_count > 0);
    const game::LaneIndex l = star.lanes[0];
    const game::Lane& lane = c.lanes[static_cast<std::size_t>(l)];
    // `ship_enter_lane`, halfway along.
    ship->location_kind = game::ShipLocation::kLane;
    ship->location = l;
    ship->position[1] = lane.star_a == h.home() ? 1.0F : 0.0F;
    ship->position[0] = game::f32(game::length(game::sub(c.stars[static_cast<std::size_t>(lane.star_a)].position,
                                                         c.stars[static_cast<std::size_t>(lane.star_b)].position)) *
                                  0.5);
    h.refresh();
    const auto transit = static_cast<int>(map.items().size()) - 1;
    REQUIRE(map.items()[static_cast<std::size_t>(transit)].kind == CosWnd::ItemKind::kShip);
    const CosWnd::Item moving = map.items()[static_cast<std::size_t>(transit)];
    h.hover(moving.x + 5, moving.y - 2);
    CHECK(map.hovered() == transit);
    h.click(moving.x + 5, moving.y - 2);
    CHECK(h.r.rig.wm.current_screen() == 1);

    ship->position[0] = 0.0F;
    h.refresh();
    const CosWnd::Item leaving = map.items()[static_cast<std::size_t>(transit)];
    h.hover(leaving.x + 5, leaving.y - 2);
    CHECK(map.hovered() == static_cast<int>(star_item(map, h.home())));
    h.click(leaving.x + 5, leaving.y - 2);
    CHECK(h.r.rig.wm.current_screen() == 16);
  }
}

TEST_CASE("Next Turn ends the day; the auto next turn repeats it until the map is left", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  TempDir dir;
  StarMapRig r(files, 3, dir.path);
  r.to_new_game_screen();
  r.to_star_map();
  REQUIRE(r.session.day() == 0);
  r.click(511, 67);  // NEXTTURN
  CHECK(r.session.day() == 1);
  r.click(594, 67);  // NEXTTURNCONT
  CHECK(r.rig.wm.auto_turn());
  r.wait(5);
  CHECK(r.session.day() >= 6);
  r.click(553, 145);  // Planets: the star map is left
  r.rig.pass();
  CHECK_FALSE(r.rig.wm.auto_turn());
  CHECK_FALSE(dynamic_cast<ToggleButton&>(*r.rig.wm.find_window("NEXTTURNCONT")).on());
}

}  // namespace
}  // namespace oa
