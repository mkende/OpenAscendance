// The original's developer mode (`nougat.lf`, the hidden `--nougat`;
// `docs/05-subsystems.md`): the oracle's `dev_modes` run replayed in the
// developer mode, the window manager's developer keys (the debug window,
// the trade-all, the recorder, the overlays), the screens' cheat keys and
// overlay, and the ship-flag checks (OA_DATA install, OA_ORACLE runs).

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "assets/winevent.h"
#include "game/cosmos.h"
#include "game/planet.h"
#include "game/raceneg.h"
#include "game/save.h"
#include "game_data.h"
#include "gfx/font.h"
#include "replay_rig.h"
#include "screens/bat_mode_wnd.h"
#include "screens/cos_wnd.h"
#include "ui/dbg_wnd.h"
#include "ui/list_box.h"

namespace oa::test {
namespace {

constexpr int kScreenStarMap = 1;
constexpr int kScreenPlanet = 17;
constexpr int kScreenResearch = 20;
constexpr std::uint16_t kAlt = SDL_KMOD_LALT;

// The shipped day-820 game (`RESUME.GAM`) on the star map, the pointer at
// (320, 240), the saves and the developer files in a folder of their own.
struct DevRig {
  DevRig(const GameFiles& files, bool developer) : session(files, dir.path, true), rig(files, &session, dir.path) {
    REQUIRE(session.load_resume());
    session.core()->developer = developer;
    rig.move(320, 240);
    rig.wm.set_screen(kScreenStarMap, true);
    rig.pass();
  }

  game::Session& core() { return *session.core(); }
  game::Cosmos& cosmos() { return core().game->cosmos; }
  // Alt + `key`, then Alt released.
  void alt_key(std::uint32_t key) {
    rig.key(key, kAlt);
    rig.pass(InputEvent{.type = EventType::kKeyUp, .key = SDLK_LALT, .mods = 0});
  }
  // The next day (message 7, NEXTTURN).
  void next_turn() {
    rig.wm.post_message(kMessageNextTurn, 0, 0);
    rig.pass();
  }
  // The planet screen of the first planet `pick` accepts.
  game::Planet& open_planet(const std::function<bool(const game::Planet&)>& pick) {
    for (game::PlanetIndex i = 0; i < cosmos().planet_count; ++i) {
      if (pick(cosmos().planets[static_cast<std::size_t>(i)])) {
        cosmos().selected_planet = i;
        rig.wm.set_screen(kScreenPlanet, true);
        rig.pass();
        return cosmos().planets[static_cast<std::size_t>(i)];
      }
    }
    FAIL("no such planet");
    throw std::logic_error("unreachable");
  }

  ReplayDir dir;
  GameCoreSession session;
  UiRig rig;
};

// Whether `text` is drawn at (x, y) of `pane` in `font` on the screen:
// drawing it again there changes nothing.
bool drawn(UiRig& rig, GameFont& font, const Pane& pane, int x, int y, std::string_view text, int background) {
  Image8 copy = rig.wm.screen();
  const Pane saved = font.pane();
  font.pane() = pane;
  font.draw_wrapped(copy, x, y, text, 0, kDefault, background, 0);
  font.pane() = saved;
  for (int py = 0; py < copy.height(); ++py) {
    for (int px = 0; px < copy.width(); ++px) {
      if (copy.at(px, py) != rig.wm.screen().at(px, py)) {
        return false;
      }
    }
  }
  return true;
}

std::vector<std::uint8_t> save_bytes(const game::Session& s) { return game::serialize_save(game::save_game(s)); }

std::uint8_t player_bit(const game::Session& s) { return static_cast<std::uint8_t>(1U << s.player_race); }

}  // namespace

TEST_CASE("the oracle's developer-mode run: overlays, debug window, planet keys", "[content][oracle][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  // The day-820 game: Alt+F, Alt+D, the star map's C, Alt+W, the planet
  // keys O, I and T, one Next Turn (`docs/12-oracle.md`). The frame-rate
  // overlay's numbers are left out: the original's screenshots take it
  // more than three seconds (SCR002 shows 388 ticks for ten passes, their
  // remains in SCR003).
  replay_oracle_runs(
      files,
      {{.run = "dev_modes", .keep_saves = true, .shots = {}, .developer = true, .unchecked = Pane{25, 10, 150, 30}}});
}

TEST_CASE("the developer hotkeys are swallowed without the developer mode", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig r(files, false);
  r.rig.wm.set_rotation_speed(2.0F);
  for (const std::uint32_t key : {SDLK_E, SDLK_R, SDLK_P, SDLK_D, SDLK_F, SDLK_W}) {
    r.alt_key(key);
  }
  CHECK_FALSE(r.rig.wm.trade_all());
  CHECK_FALSE(r.rig.wm.recording());
  CHECK_FALSE(r.rig.wm.replaying());
  CHECK_FALSE(r.rig.wm.pointer_overlay());
  CHECK_FALSE(r.rig.wm.fps_overlay());
  CHECK_FALSE(r.rig.wm.in_modal());
  CHECK(r.rig.wm.rotation_speed() == 2.0F);
  CHECK_FALSE(std::filesystem::exists(r.dir.path / "WINEVENT.BIN"));
  // Nor does a day write the ship-flag check.
  r.next_turn();
  CHECK(r.session.day() == 821);
  CHECK_FALSE(std::filesystem::exists(r.dir.path / "numships.dbg"));
}

TEST_CASE("Alt+E: after each day the player receives stars and lanes from every race", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig traded(files, true);
  DevRig plain(files, true);
  traded.alt_key(SDLK_E);
  CHECK(traded.rig.wm.trade_all());
  traded.next_turn();
  plain.next_turn();
  REQUIRE(traded.session.day() == 821);
  REQUIRE(plain.session.day() == 821);
  // The same day, then the trades of `Race_trade_stars` and
  // `Race_trade_lanes` with every other race.
  const std::vector<std::uint8_t> before = save_bytes(plain.core());
  game::Session& s = plain.core();
  for (int r = 0; r < plain.cosmos().race_count; ++r) {
    if (r != s.player_race) {
      game::race_trade_stars(s, plain.cosmos().races[s.player_race], static_cast<game::RaceIndex>(r), -1);
      game::race_trade_lanes(s, plain.cosmos().races[s.player_race], static_cast<game::RaceIndex>(r), -1);
    }
  }
  CHECK(save_bytes(plain.core()) != before);
  CHECK(save_bytes(traded.core()) == save_bytes(plain.core()));
  // Alt+E again: off.
  traded.alt_key(SDLK_E);
  CHECK_FALSE(traded.rig.wm.trade_all());
}

TEST_CASE("Alt+D and Alt+F draw their overlays at each refresh, left on the screen when off", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig r(files, true);
  GameFont& font = r.rig.wm.large_font();
  // The pointer in the bottom half: its position at the top-left corner.
  CHECK_FALSE(drawn(r.rig, font, GSystem::kScreenPane, 0, 0, "(320,240)", kDefault));
  r.alt_key(SDLK_D);
  CHECK(r.rig.wm.pointer_overlay());
  CHECK(drawn(r.rig, font, GSystem::kScreenPane, 0, 0, "(320,240)", kDefault));
  // In the top half: at the bottom.
  r.rig.move(100, 50);
  CHECK(drawn(r.rig, font, GSystem::kScreenPane, 0, 460, "(100,50)", kDefault));
  r.alt_key(SDLK_D);
  CHECK_FALSE(r.rig.wm.pointer_overlay());
  r.rig.pass();
  CHECK(drawn(r.rig, font, GSystem::kScreenPane, 0, 460, "(100,50)", kDefault));

  // The frame rate: its first text until 10 passes, then the passes per
  // second and the ticks of 10 passes (3 each).
  const Pane fps{25, 10, 150, 30};
  r.alt_key(SDLK_F);
  CHECK(r.rig.wm.fps_overlay());
  CHECK(drawn(r.rig, font, fps, 0, 0, "FPS DUDE", kDefault));
  for (int i = 0; i < 20; ++i) {
    r.rig.pass();
  }
  CHECK(drawn(r.rig, font, fps, 0, 0, "FPS 33  30", kDefault));
  r.alt_key(SDLK_F);
  CHECK_FALSE(r.rig.wm.fps_overlay());
  r.rig.pass();
  CHECK(drawn(r.rig, font, fps, 0, 0, "FPS 33  30", kDefault));
}

TEST_CASE("Alt+R records the input into WINEVENT.BIN and its listing; Alt+P replays it", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig r(files, true);
  r.rig.wm.set_rotation_speed(2.0F);
  r.alt_key(SDLK_R);
  REQUIRE(r.rig.wm.recording());
  // `srand(0)` and the rotation speed at 1 first.
  CHECK(r.core().rng.state() == 0);
  CHECK(r.rig.wm.rotation_speed() == 1.0F);
  r.rig.move(100, 200);
  r.rig.key(SDLK_A);
  r.alt_key(SDLK_R);
  CHECK_FALSE(r.rig.wm.recording());

  const std::vector<std::uint8_t> bytes = read_file(r.dir.path / "WINEVENT.BIN");
  const Trace trace = parse_trace(bytes, "WINEVENT.BIN");
  const auto has = [&](TraceEventType type, std::uint32_t a, std::uint32_t b) {
    return std::ranges::any_of(trace.records,
                               [&](const TraceRecord& rec) { return rec.type == type && rec.a == a && rec.b == b; });
  };
  CHECK(has(TraceEventType::kMouseMove, 100, 200));
  CHECK(has(TraceEventType::kKey, 0x1e, 'a'));
  // Not the recorder's own Alt+R.
  CHECK_FALSE(std::ranges::any_of(
      trace.records, [](const TraceRecord& rec) { return rec.type == TraceEventType::kKey && rec.a == 0x13; }));
  CHECK(trace.records.size() == r.rig.wm.records().size());
  const std::vector<std::uint8_t> listing = read_file(r.dir.path / "WINEVENT.TXT");
  CHECK(std::string(listing.begin(), listing.end()).find(" EVT_MOUSE_POS    :  Moved to (100,200)\n") !=
        std::string::npos);

  // Alt+P: the trace replayed once.
  r.alt_key(SDLK_P);
  CHECK(r.rig.wm.replaying());
  for (std::uint32_t i = 0; i <= trace.records.back().time + 1; ++i) {
    r.rig.pass();
  }
  CHECK_FALSE(r.rig.wm.replaying());
}

TEST_CASE("Alt+W: the debug window lists the screen's windows, the palette and the tunes", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig r(files, true);
  auto& debug = dynamic_cast<DbgWnd&>(*r.rig.wm.find_window("WMGRDEBUG"));
  auto& list = dynamic_cast<ListBox&>(*r.rig.wm.find_window("DBGWNDLIST"));
  // Alt + click: the palette pick of the pixel under the pointer.
  const int color = r.rig.wm.screen().at(500, 100);
  r.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_LALT, .mods = kAlt});
  r.rig.queue_click(500, 100);
  r.rig.pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = SDLK_LALT, .mods = 0});
  r.rig.drain();
  CHECK(r.rig.wm.picked_color() == color);

  // Opened and closed (EXIT): the windows of the star map's screen, the
  // window and its children on top.
  const auto open_then = [&](std::initializer_list<std::pair<int, int>> clicks) {
    r.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_W, .mods = kAlt});
    r.rig.pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = SDLK_W, .mods = 0});
    for (const auto& [x, y] : clicks) {
      r.rig.queue_click(x, y);
    }
    r.rig.queue_click(283, 298);  // EXIT
    r.rig.drain();
    r.rig.pass(InputEvent{.type = EventType::kKeyUp, .key = SDLK_LALT, .mods = 0});
    CHECK_FALSE(r.rig.wm.in_modal());
  };
  open_then({});
  CHECK(debug.mode() == DbgWnd::Mode::kWindows);
  REQUIRE(list.count() > 8);
  CHECK(list.item(0).text == std::format("C HWND{:03d} DBGWNDLIST", list.id()));
  CHECK(list.item(7).text == std::format("  HWND{:03d} WMGRDEBUG", debug.id()));
  bool map = false;
  for (int i = 0; i < list.count(); ++i) {
    map = map || list.item(i).text.ends_with(" COSMOSWnd");
  }
  CHECK(map);

  // P: the 256 colours; N: the tunes.
  open_then({{150, 298}});
  CHECK(debug.mode() == DbgWnd::Mode::kPalette);
  CHECK(list.count() == 256);
  open_then({{190, 298}});
  CHECK(debug.mode() == DbgWnd::Mode::kTunes);
  CHECK(list.count() == r.rig.sound.tune_count());
}

TEST_CASE("the planet screen's developer keys", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  SECTION("O, T, N and Shift+M, whoever owns the planet") {
    DevRig r(files, true);
    game::Planet& p = r.open_planet([](const game::Planet& p) { return game::name_view(p.name) == "Zoroaster III"; });
    r.rig.key(SDLK_O);
    CHECK(p.population == p.max_population);
    const auto bases = [&] {
      return std::ranges::count_if(r.cosmos().squares, [](const game::Square& sq) { return sq.item == 5; });
    };
    const std::uint16_t population = p.population;
    const auto before = bases();
    r.rig.key(SDLK_T);
    CHECK(p.population == population + 2);
    CHECK(bases() == before + 1);
    const std::uint16_t base = p.base_population;
    r.rig.key(SDLK_M, SDL_KMOD_LSHIFT);
    CHECK(p.base_population == base + 1);
  }
  SECTION("I: the project's progress + 200 and a planet day") {
    DevRig r(files, true);
    DevRig by_hand(files, true);
    game::Planet& p = r.open_planet([](const game::Planet& p) { return game::name_view(p.name) == "Zoroaster III"; });
    game::Planet& q =
        by_hand.open_planet([](const game::Planet& p) { return game::name_view(p.name) == "Zoroaster III"; });
    r.rig.key(SDLK_I);
    q.progress = static_cast<std::uint16_t>(q.progress + 200);
    game::planet_daily_update(by_hand.core(), q);
    CHECK(p.progress == q.progress);
    CHECK(p.population == q.population);
    CHECK(p.project_item == q.project_item);
  }
  SECTION("R: an unowned planet gets only the owner byte") {
    DevRig r(files, true);
    game::Planet& p = r.open_planet([](const game::Planet& p) { return p.owner == game::kNoOwner; });
    const std::uint8_t star_owners = r.cosmos().stars[static_cast<std::size_t>(p.star)].owner_mask;
    r.rig.key(SDLK_R);
    CHECK(p.owner == r.core().player_race);
    CHECK(p.population == 0);
    CHECK(r.cosmos().stars[static_cast<std::size_t>(p.star)].owner_mask == star_owners);
  }
  SECTION("R: another race's planet is taken") {
    DevRig r(files, true);
    const std::uint8_t player = r.core().player_race;
    game::Planet& p =
        r.open_planet([&](const game::Planet& p) { return p.owner != game::kNoOwner && p.owner != player; });
    r.rig.key(SDLK_R);
    CHECK(p.owner == player);
    CHECK((r.cosmos().stars[static_cast<std::size_t>(p.star)].owner_mask & player_bit(r.core())) != 0);
  }
  SECTION("nothing without the developer mode") {
    DevRig r(files, false);
    game::Planet& p = r.open_planet([](const game::Planet& p) { return game::name_view(p.name) == "Zoroaster III"; });
    const std::uint16_t population = p.population;
    const std::uint16_t base = p.base_population;
    const std::uint16_t progress = p.progress;
    const std::uint8_t player = r.core().player_race;
    for (const std::uint32_t key : {SDLK_T, SDLK_I, SDLK_N}) {
      r.rig.key(key);
    }
    r.rig.key(SDLK_M, SDL_KMOD_LSHIFT);
    CHECK(p.population == population);
    CHECK(p.base_population == base);
    CHECK(p.progress == progress);
    game::Planet& foreign =
        r.open_planet([&](const game::Planet& p) { return p.owner != game::kNoOwner && p.owner != player; });
    const std::uint8_t owner = foreign.owner;
    r.rig.key(SDLK_R);
    CHECK(foreign.owner == owner);
  }
}

TEST_CASE("the star map's developer keys and statistics overlay", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig r(files, true);
  const game::Session& s = r.core();
  const std::uint8_t bit = player_bit(s);
  r.rig.key(SDLK_S);
  CHECK(std::all_of(r.cosmos().stars.begin(), r.cosmos().stars.begin() + r.cosmos().star_count,
                    [&](const game::Star& star) { return (star.explored_mask & bit) != 0; }));
  r.rig.key(SDLK_L);
  CHECK(std::all_of(r.cosmos().lanes.begin(), r.cosmos().lanes.begin() + r.cosmos().lane_count,
                    [&](const game::Lane& lane) { return (lane.known_mask & bit) != 0; }));
  r.rig.key(SDLK_R);
  for (int i = 0; i < r.cosmos().race_count; ++i) {
    if (i != s.player_race) {
      const game::Race& race = r.cosmos().races[static_cast<std::size_t>(i)];
      // At peace, both attitudes the other race's starting one.
      CHECK(r.cosmos().races[s.player_race].relation[static_cast<std::size_t>(i)] == game::kPeace);
      CHECK(race.relation[s.player_race] == game::kPeace);
      CHECK(r.cosmos().races[s.player_race].attitude[static_cast<std::size_t>(i)] == race.start_attitude);
      CHECK(race.attitude[s.player_race] == race.start_attitude);
    }
  }
  // C: the statistics; its stars controlled is always 0.
  GameFont& small = r.rig.wm.small_font();
  CHECK_FALSE(drawn(r.rig, small, GSystem::kScreenPane, 10, 396, "STARS CONTROLLED 0", 0xff));
  r.rig.key(SDLK_C);
  CHECK(drawn(r.rig, small, GSystem::kScreenPane, 10, 396, "STARS CONTROLLED 0", 0xff));
  // 2: play as race slot 1; 8: as every race; 1: back to slot 0.
  r.rig.key(SDLK_2);
  CHECK(s.player_race == 1);
  CHECK(s.player_mask == 2);
  r.rig.key(SDLK_8);
  CHECK(s.player_race == 0);
  CHECK(s.player_mask == 0xff);
  r.rig.key(SDLK_1);
  CHECK(s.player_mask == 1);
}

TEST_CASE("the star map's keys change nothing without the developer mode", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig r(files, false);
  const std::vector<std::uint8_t> before = save_bytes(r.core());
  for (const std::uint32_t key : {SDLK_S, SDLK_L, SDLK_R, SDLK_2, SDLK_8}) {
    r.rig.key(key);
  }
  CHECK(save_bytes(r.core()) == before);
}

TEST_CASE("the System Display's developer caption of the hovered ship", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  constexpr int kScreenStarSystem = 16;
  for (const bool developer : {false, true}) {
    INFO(developer);
    // The day-820 game on Feculon, where the player's ships are.
    CoreGameSession session(files, files.load("RESUME.GAM"));
    UiRig rig(files, &session);
    game::Session& s = session.session();
    game::Cosmos& c = s.game->cosmos;
    s.developer = developer;
    rig.wm.restore_window_state();
    const auto star = std::find_if(c.stars.begin(), c.stars.begin() + c.star_count,
                                   [](const game::Star& star) { return game::name_view(star.name) == "Feculon"; });
    REQUIRE(star != c.stars.begin() + c.star_count);
    c.selected_star = static_cast<game::StarIndex>(star - c.stars.begin());
    rig.move(400, 460);
    rig.wm.set_screen(kScreenStarSystem, true);
    rig.pass();
    const auto ship = std::find_if(c.ships.begin(), c.ships.end(), [&](const game::Ship& ship) {
      return ship.owner == s.player_race && ship.location_kind == game::ShipLocation::kSystem &&
             ship.location == c.selected_star;
    });
    REQUIRE(ship != c.ships.end());
    auto& view = dynamic_cast<BatModeWnd&>(*rig.wm.find_window("BatModeWnd"));
    const int i = view.find_item(BatModeWnd::ItemKind::kShip, static_cast<int>(ship - c.ships.begin()));
    REQUIRE(i >= 0);
    const BatModeWnd::Item& item = view.items()[static_cast<std::size_t>(i)];
    rig.move(item.x + view.pane().x0, item.y + view.pane().y0);
    REQUIRE(view.hovered() == i);
    GameFont& small = rig.wm.small_font();
    CHECK(drawn(rig, small, GSystem::kScreenPane, 250, 20,
                std::format("POWER {} MOVES {} INTEGRITY {}", ship->power, ship->moves, ship->hit_points),
                kDefault) == developer);
  }
}

TEST_CASE("the research screen's developer keys", "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  DevRig r(files, true);
  r.rig.wm.set_screen(kScreenResearch, true);
  r.rig.pass();
  game::Research& research = r.core().game->research;
  const std::uint8_t bit = player_bit(r.core());
  const std::uint8_t player = r.core().player_race;
  const auto known = [&] {
    return std::count_if(research.items.begin(), research.items.begin() + research.count,
                         [&](const game::ResearchItem& item) { return (item.known_mask & bit) != 0; });
  };
  // F forgets every item, C learns the project, D learns every item; each
  // clears the project and its progress.
  r.rig.key(SDLK_F);
  CHECK(known() == 0);
  research.project[player] = 3;
  research.progress[player] = 10;
  r.rig.key(SDLK_C);
  CHECK(known() == 1);
  CHECK((research.items[3].known_mask & bit) != 0);
  CHECK(research.project[player] == -1);
  CHECK(research.progress[player] == 0);
  r.rig.key(SDLK_D);
  CHECK(known() == research.count);
}

TEST_CASE("the developer mode's ship-flag checks: numships.dbg and the star map's cos%05d.bug",
          "[content][developer]") {
  OA_REQUIRE_GAME_FILES(files);
  SECTION("the lines of the check") {
    DevRig r(files, false);
    game::Cosmos& c = r.cosmos();
    CHECK(game::cosmos_ship_flag_report(c, 821).empty());
    // A flag without a ship: a star without ships, race 2's bit set.
    game::Star& empty = *std::find_if(c.stars.begin(), c.stars.begin() + c.star_count,
                                      [](const game::Star& star) { return star.ships_mask == 0; });
    empty.ships_mask = 4;
    CHECK(game::cosmos_ship_flag_report(c, 821) ==
          std::format("T0821: {} flags (4) show R2 ship, but none found.\n", game::name_view(empty.name)));
    empty.ships_mask = 0;
    // A ship (not being built) without its flag.
    const game::Ship& ship = *std::find_if(c.ships.begin(), c.ships.end(), [](const game::Ship& ship) {
      return ship.owner != -1 && ship.location_kind == game::ShipLocation::kSystem;
    });
    game::Star& here = c.stars[static_cast<std::size_t>(ship.location)];
    here.ships_mask = static_cast<std::uint8_t>(here.ships_mask & ~(1U << ship.owner));
    CHECK(game::cosmos_ship_flag_report(c, 821).find(
              std::format("T0821: R{} ship exists at {}, flags ({}) indicate otherwise.", ship.owner,
                          game::name_view(here.name), here.ships_mask)) != std::string::npos);
  }
  SECTION("each day appends the check to numships.dbg") {
    DevRig r(files, true);
    r.next_turn();
    REQUIRE(r.session.day() == 821);
    REQUIRE(std::filesystem::exists(r.dir.path / "numships.dbg"));
    CHECK(read_file(r.dir.path / "numships.dbg").empty());
  }
  SECTION("the star map saves the game as cos%05d.bug and fails on a flag without a ship") {
    DevRig r(files, true);
    game::Cosmos& c = r.cosmos();
    std::find_if(c.stars.begin(), c.stars.begin() + c.star_count, [](const game::Star& star) {
      return star.ships_mask == 0;
    })->ships_mask = 1;
    auto& map = dynamic_cast<CosWnd&>(*r.rig.wm.find_window("COSMOSWnd"));
    CHECK_THROWS_AS(map.build_items(), std::logic_error);
    CHECK(std::filesystem::exists(r.dir.path / "cos00820.bug"));
    // Not without the developer mode.
    r.core().developer = false;
    CHECK_NOTHROW(map.build_items());
  }
}

TEST_CASE("window-event traces: written back as read, and their listing", "[assets][developer]") {
  Trace trace;
  trace.header[0] = 0x3c;
  trace.records = {
      {.time = 1, .window = 0, .type = TraceEventType::kMouseMove, .msg = 0, .a = 100, .b = 200, .c = 0},
      {.time = 2, .window = 0, .type = TraceEventType::kMouseButton, .msg = 1, .a = 0, .b = 0xffffffff, .c = 0},
      {.time = 3, .window = 0, .type = TraceEventType::kKey, .msg = 0, .a = 0x1e, .b = 'a', .c = 0},
      {.time = 4, .window = 7, .type = TraceEventType::kRequestTimer, .msg = 59, .a = 1, .b = 2, .c = 0},
      {.time = 5, .window = 7, .type = TraceEventType::kCancelNotify, .msg = 8, .a = 0, .b = 0, .c = 0},
  };
  const std::vector<std::uint8_t> bytes = serialize_trace(trace);
  REQUIRE(bytes.size() == Trace::kHeaderSize + 5 * Trace::kRecordSize);
  const Trace back = parse_trace(bytes, "trace");
  CHECK(back.header == trace.header);
  REQUIRE(back.records.size() == 5);
  CHECK(back.records[3].window == 7);
  CHECK(back.records[3].msg == 59);
  CHECK(back.records[1].b == 0xffffffff);
  CHECK(trace_text(trace) ==
        " TIME[00001]  EVT_MOUSE_POS    :  Moved to (100,200)\n"
        " TIME[00002]  EVT_MOUSE_BUTTON :  Left Button DOWN\n"
        " TIME[00003]  EVT_KEYBOARD     :  'a'  (0030)\n"
        " TIME[00004]  EVT_REQ_CONT   :  HWND[0007] MSG[0059] P1[0001] P2[0002]\n"
        " TIME[00005]  EVT_CAN_CHANGE :  HWND[0007] MSG[0008] P1[0000] P2[0000] F[0000]\n");
}

}  // namespace oa::test
