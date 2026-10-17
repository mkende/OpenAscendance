// The days played through the user interface: the original's runs of the
// oracle corpus (`tests/oracle/corpus`, `docs/12-oracle.md`) replayed in the
// remake from their own window-event traces (`trace.bin`, the original's
// third argument) on the same start-up galaxy, ending in the original's
// saves and Alt+G screenshots: the new game, Next Turn, the game-event
// boxes closed with Esc, the opening menu's Return to Current Game, the
// save screen and the quit. OA_DATA install and OA_ORACLE (`work/oracle`).

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "assets/gif.h"
#include "assets/shp.h"
#include "audio/mixer.h"
#include "audio/sound_system.h"
#include "core/install.h"
#include "game/address.h"
#include "game/cosmos.h"
#include "game/new_game.h"
#include "game/save.h"
#include "game_data.h"
#include "gfx/gsystem.h"
#include "platform/clock.h"
#include "platform/input.h"
#include "replay_rig.h"
#include "ui/event_wnd.h"
#include "ui/game_core_session.h"
#include "ui/winmgr.h"
#include "ui_rig.h"

namespace oa {
namespace {

// A folder of its own, removed afterwards.
struct TempDir {
  TempDir() {
    std::random_device rd;
    path = std::filesystem::temp_directory_path() / std::format("oa_day_ui_{:08x}", rd());
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

// The program as `main` runs it (`app/corn.cpp`), headless on a virtual
// clock, as `PATCH.EXE` (`variant` Standard) or `ANTAG.EXE` (Antagonizer,
// the variant of its new games): the start-up galaxy of `cosmos.txt` (50
// stars, 7 races, `seed`),
// the windows, the opening menu, the pointer drawn into the shown frame
// (the Alt+G screenshots show it) and moved by the replay.
struct Program {
  // `install_saves`: the install's `01.SAV` and `RESUME.GAM` are seen (the
  // oracle's `--keep-saves`).
  Program(const GameFiles& f, game::Variant variant, std::int32_t seed, const std::filesystem::path& dir,
          bool install_saves)
      : files(f),
        gs([this] { clock.wait_vblank(); }),
        sound(f, &mixer),
        session(f, dir, install_saves),
        pointer_shapes(f.load("DATA\\MOUSE.SHP"), "DATA\\MOUSE.SHP"),
        wm((start(variant, seed),
            UiServices{.files = f,
                       .gsystem = gs,
                       .sound = sound,
                       .game = session,
                       .user_dir = dir,
                       .now_us = [this] { return clock.micros(); },
                       .ticks = [this] { return clock.ticks(); },
                       .idle = [this] { clock.idle(); },
                       .show_pointer = [this](bool shown) { pointer = shown; },
                       .set_pointer_frame = [this](int f) { pointer_frame = f; },
                       .poll_input = [this]() -> const InputState& { return next_input(); },
                       .present = [this] { present(); },
                       .shown_frame = [this]() -> const Image8& { return frame; },
                       .warp_pointer = [this](int x, int y) { input.warp(x, y); }}),
           test::full_factory()) {
    wm.setup_race_shapes();
    wm.start_pending_fade();
    gs.load_palette(files, "DATA\\game.pal", 0, 256);
    gs.set_dac_range(0, 256);
    wm.set_screen(0, false);
  }

  void start(game::Variant variant, std::int32_t seed) {
    session.set_new_game_variant(variant);
    const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = seed, .species = {}};
    session.start(0, &txt);
  }

  // The real input while a trace is replayed: none, but with
  // `auto_continue` (a tutorial run) a right click closes each tutorial help
  // box (the replay paused in a modal loop), as the oracle's
  // `autocontinue`, and Alt+X quits once the replay is over (`quitidle`).
  const InputState& next_input() {
    if (auto_continue && pending.empty() && !wm.replaying()) {
      const InputState& s = input.state();
      if (wm.in_modal()) {
        pending.push_back(InputEvent{.type = EventType::kMouseDown, .x = s.x, .y = s.y, .button = MouseButton::kRight});
        pending.push_back(InputEvent{.type = EventType::kMouseUp, .x = s.x, .y = s.y, .button = MouseButton::kRight});
        ++continues;
      } else if (!quit_sent) {
        pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_X, .mods = SDL_KMOD_LALT});
        pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = SDLK_X, .mods = 0});
        quit_sent = true;
      }
    }
    if (!pending.empty()) {
      input.feed(pending.front());
      pending.pop_front();
    }
    return input.poll();
  }

  void present() {
    frame = gs.screen();
    if (pointer) {
      const InputState& s = input.state();
      vfx_shape_draw(frame, GSystem::kScreenPane, pointer_shapes.at(static_cast<std::size_t>(pointer_frame)), s.x,
                     s.y);
    }
  }

  // Replays `trace` until the game quits (at most `max_passes` passes);
  // then `main`'s exit saves `RESUME.GAM`. Returns whether it quit.
  bool replay(const std::vector<std::uint8_t>& trace, int max_passes) {
    REQUIRE(wm.start_replay(trace, "trace.bin", false));
    for (int i = 0; i < max_passes && wm.running(); ++i) {
      wm.run_pass();
    }
    if (wm.running()) {
      return false;
    }
    wm.save_config();
    session.save_resume();
    return true;
  }

  const GameFiles& files;
  Clock clock{true};
  Mixer mixer;
  GSystem gs;
  SoundSystem sound;
  GameCoreSession session;
  ShapeTable pointer_shapes;
  Input input;
  std::deque<InputEvent> pending;
  Image8 frame;
  bool pointer = true;
  int pointer_frame = 0;
  bool auto_continue = false;
  int continues = 0;
  bool quit_sent = false;
  WinMgr wm;
};

// Counts the pixels of two screenshots that differ.
int differing_pixels(const std::filesystem::path& ours, const std::filesystem::path& theirs) {
  const Image8 a = decode_gif(read_file(ours), ours.string());
  const Image8 b = decode_gif(read_file(theirs), theirs.string());
  REQUIRE(a.width() == b.width());
  REQUIRE(a.height() == b.height());
  int n = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      n += a.at(x, y) != b.at(x, y) ? 1 : 0;
    }
  }
  return n;
}

// The first offset where two files differ, -1 for none.
std::ptrdiff_t first_difference(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    if (a[i] != b[i]) {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return a.size() == b.size() ? -1 : static_cast<std::ptrdiff_t>(std::min(a.size(), b.size()));
}

// Clears the stale stack bytes of a save header (`Game_save` 0x4f458): the
// per-race entries past the race count and the last 8 bytes.
std::vector<std::uint8_t> without_stale_header(std::vector<std::uint8_t> save) {
  const int races = save.at(4);
  for (const std::size_t table : {0x0cU, 0x28U, 0x44U}) {
    for (int r = races; r < 7; ++r) {
      std::fill_n(save.begin() + static_cast<std::ptrdiff_t>(table + 4 * static_cast<std::size_t>(r)), 4, 0);
    }
  }
  std::fill_n(save.begin() + 0x6c, 8, 0);
  return save;
}

// A run of the corpus (`tests/oracle/corpus/cases.txt`).
struct CorpusRun {
  std::string run;
  // The original's executable: `PATCH.EXE` or `ANTAG.EXE`.
  game::Variant variant = game::Variant::kStandard;
  std::int32_t seed = 1;
  bool keep_saves = false;
  // A save under OA_ORACLE installed as `RESUME.GAM` (the oracle's
  // `--resume`), none when empty.
  std::string resume{};
  // A tutorial run: its help boxes closed and the game quit at its end.
  bool tutorial = false;
  // The run quits on the new-game screen, whose galaxy turns at every pass:
  // the oracle's Alt+X comes after a wall-clock wait (`quitafter`), so the
  // star map's camera is not compared.
  bool turning_view = false;
};

// Clears the star map's camera in a save's window state (`CosWnd_serialize`
// +0x1e, 0x98 bytes).
std::vector<std::uint8_t> without_camera(std::vector<std::uint8_t> save) {
  const auto state = save.end() - static_cast<std::ptrdiff_t>(game::kWindowStateSize);
  std::fill(state + 0x1e, state + 0xb6, 0);
  return save;
}

// Replays the corpus run `c` and compares every file the original wrote.
// Returns false when the run is missing.
bool replay_run(const GameFiles& files, const CorpusRun& c, int max_passes) {
  const auto trace = oracle_path(c.run + "/trace.bin");
  const auto out = oracle_path(c.run + "/out");
  const auto resume = c.resume.empty() ? std::nullopt : oracle_path(c.resume);
  if (!trace || !out || (!c.resume.empty() && !resume)) {
    return false;
  }
  INFO(c.run);
  TempDir dir;
  if (resume) {
    std::filesystem::copy_file(*resume, dir.path / "RESUME.GAM");
  }
  // `ANTAG.EXE` plays the shipped (PATCH) saves under its own rules, and
  // its saves of that game keep their stale PATCH pointers: the remake gets
  // them with the variant's trailer, and the saves are compared without
  // their stale list slots.
  const bool foreign = c.keep_saves && c.variant != game::Variant::kStandard;
  if (foreign) {
    for (const char* name : {"01.SAV", "RESUME.GAM"}) {
      game::SaveImage image = game::parse_save(read_file(*files.install().find(name)));
      image.variant = c.variant;
      const std::vector<std::uint8_t> bytes = game::serialize_save(image);
      std::ofstream(dir.path / name, std::ios::binary)
          .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
  }
  Program program(files, c.variant, c.seed, dir.path, c.keep_saves && !foreign);
  program.auto_continue = c.tutorial;
  CHECK(program.replay(read_file(*trace), max_passes));
  if (const char* dump = std::getenv("OA_DUMP_DIR"); dump != nullptr && *dump != '\0') {
    // The remake's files, for `tools/savdiff.py` and `tools/compare_shot.py`.
    const std::filesystem::path to = std::filesystem::path(dump) / c.run;
    std::filesystem::create_directories(to);
    std::filesystem::copy(dir.path, to, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
  }
  for (const auto& entry : std::filesystem::directory_iterator(*out)) {
    const std::string name = entry.path().filename().string();
    const std::filesystem::path ours = dir.path / name;
    if (name.ends_with(".SAV") || name == "RESUME.GAM") {
      INFO(name);
      REQUIRE(std::filesystem::exists(ours));
      std::vector<std::uint8_t> mine = read_file(ours);
      std::vector<std::uint8_t> theirs = read_file(entry.path());
      if (c.variant != game::Variant::kStandard) {
        // The remake's variant trailer (`game/save.h`), absent from the original's file.
        REQUIRE(mine.size() == theirs.size() + 8);
        mine.resize(theirs.size());
      }
      if (foreign) {
        mine = test::without_stale_slots(mine, game::kPatchCosmosAddress);
        theirs = test::without_stale_slots(theirs, game::kPatchCosmosAddress);
      } else if (c.tutorial) {
        mine = test::without_stale_slots(mine);
        theirs = test::without_stale_slots(theirs);
        if (c.turning_view) {
          mine = without_camera(std::move(mine));
          theirs = without_camera(std::move(theirs));
        }
      } else {
        mine = without_stale_header(std::move(mine));
        theirs = without_stale_header(std::move(theirs));
      }
      CHECK(first_difference(mine, theirs) == -1);
    } else if (name.ends_with(".GIF")) {
      INFO(name);
      // The remake writes the screenshots in lower case.
      std::string lower = name;
      for (char& ch : lower) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      REQUIRE(std::filesystem::exists(dir.path / lower));
      CHECK(differing_pixels(dir.path / lower, entry.path()) == 0);
    }
  }
  return true;
}

// The shipped day-820 game (the install's `RESUME.GAM`) on the star map,
// under the window manager.
struct Game820 {
  explicit Game820(const GameFiles& files) : session(files, dir.path, true), rig(files, &session, dir.path) {
    REQUIRE(session.has_game());
    rig.wm.set_screen(1, true);
  }

  game::Session& core() { return *session.core(); }
  game::Cosmos& cosmos() { return core().game->cosmos; }
  EventWnd& box() { return dynamic_cast<EventWnd&>(*rig.wm.find_window("EVENTWIN")); }
  // The first ship of the player in a system.
  const game::Ship& player_ship_in_system() {
    for (std::int16_t i = 0; i < cosmos().ship_count; ++i) {
      const game::Ship& ship = cosmos().ships[static_cast<std::size_t>(i)];
      if (ship.owner == core().player_race && ship.location_kind == game::ShipLocation::kSystem) {
        return ship;
      }
    }
    FAIL("no ship of the player in a system");
    return cosmos().ships[0];
  }
  std::string species(int race) { return rig.wm.static_string(101 + cosmos().races.at(race).species); }

  TempDir dir;
  GameCoreSession session;
  test::UiRig rig;
};

std::string name_of(const auto& name) { return std::string(game::name_view(name)); }

TEST_CASE("game-event boxes: texts and pictures of the events", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Game820 g(files);
  EventWnd& box = g.box();
  const game::Session& core = g.core();
  const std::string player = g.species(core.player_race);
  const auto text = [&] { return box.content().elements[0].text; };
  const auto picture = [&](int element) {
    const HelpElement& e = box.content().elements[static_cast<std::size_t>(element)];
    return std::pair(e.shape_index, e.frame);
  };

  box.set_event({game::EventCode::kResearchDone, 5, 0});
  CHECK(text() == player + " Science Report: Our researchers have discovered " +
                      name_of(core.game->research.items[5].name) + ". ");
  CHECK(picture(1) == std::pair(31, 5));

  const game::Ship& ship = g.player_ship_in_system();
  const game::ShipIndex s = game::ship_index(g.cosmos(), ship);
  const game::Star& star = g.cosmos().stars[static_cast<std::size_t>(ship.location)];
  box.set_event({game::EventCode::kSystemDiscovered, s, 0});
  const auto plural = [](int n) { return n == 1 ? "" : "s"; };
  CHECK(text() == std::format("{}: We have discovered the {} System.  It contains {} planet{} and {} star lane{}.  "
                              "We're awaiting further orders. ",
                              name_of(ship.name), name_of(star.name), star.planet_count, plural(star.planet_count),
                              star.lane_count, plural(star.lane_count)));
  CHECK(picture(1) == std::pair(14 + core.player_race, static_cast<int>(ship.hull_size)));

  box.set_event({game::EventCode::kHostileShips, s, 2});
  CHECK(text() == std::format("{}: Entered {} system.  Hostile ships ({}) have been detected. ", name_of(ship.name),
                              name_of(star.name), g.species(2)));

  // The arriving race's ship sprite, of the player's ship's hull.
  box.set_event({game::EventCode::kHostileShipEntered, s, 3});
  CHECK(text() == std::format("A hostile ship belonging to the {} has just entered the {} system. ", g.species(3),
                              name_of(star.name)));
  CHECK(picture(1) == std::pair(14 + 3, static_cast<int>(ship.hull_size)));

  const game::Planet& planet = g.cosmos().planets[0];
  box.set_event({game::EventCode::kBuildingComplete, 0, 2});
  CHECK(text() == std::format("Construction of {} is complete on {}. \n\n{} has {} free population for additional "
                              "projects. ",
                              core.tables->planitems.items[2].name, name_of(planet.name), name_of(planet.name),
                              planet.population - planet.used_population));
  CHECK(picture(1) == std::pair(30, 2));
  CHECK(picture(2) == std::pair(29, planet.type * 5 + planet.size));

  box.set_event({game::EventCode::kRemainingMoves, ship.location, 7});
  CHECK(text().starts_with("There are remaining moves in the " + name_of(star.name) + " System"));

  box.set_event({game::EventCode::kRaceWantsToTalk, 1, 0});
  CHECK(text() == "The " + g.species(1) + " would like to speak with you. ");
  CHECK(picture(1) == std::pair(1, 0));

  // Two pages: its OK button turns the page first.
  box.set_event({game::EventCode::kBasicsKnown, 0, 0});
  CHECK(box.content().pages == 1);
  CHECK(text().starts_with(player + " Scientist Report"));
}

TEST_CASE("game events: one box per pass on the star map, Esc or a button, the screen it leads to",
          "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Game820 g(files);
  game::Session& core = g.core();
  const game::Ship& ship = g.player_ship_in_system();
  const game::ShipIndex s = game::ship_index(g.cosmos(), ship);
  core.post_event(game::EventCode::kResearchDone, 5);
  core.post_event(game::EventCode::kShipArrived, s);
  core.post_event(game::EventCode::kBuildingComplete, 0, 2);
  // Esc on the first box (no jump), the second one's "Go to System" on the
  // next pass (a box opens after the input of its pass); the third box
  // waits on the star map until the System Display is left, in the pass
  // of that Esc: its "Go to Planet".
  g.rig.pending.push_back(InputEvent{.type = EventType::kKeyUp});
  g.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_ESCAPE});
  g.rig.pending.push_back(InputEvent{.type = EventType::kKeyUp});
  g.rig.queue_click(170, 352);
  g.rig.pass();
  CHECK(g.box().event()->code == game::EventCode::kResearchDone);
  CHECK(g.rig.wm.current_screen() == 1);
  g.rig.pass();
  CHECK(g.box().event()->code == game::EventCode::kShipArrived);
  // The jump is the next pass's message.
  g.rig.pass();
  CHECK(g.rig.wm.current_screen() == 16);
  CHECK(g.cosmos().selected_star == ship.location);
  REQUIRE(g.session.has_game_events());
  g.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_ESCAPE});
  g.rig.queue_click(170, 352);
  g.rig.drain();
  CHECK_FALSE(g.session.has_game_events());
  CHECK(g.box().event()->code == game::EventCode::kBuildingComplete);
  g.rig.pass();
  CHECK(g.rig.wm.current_screen() == 17);
  CHECK(g.cosmos().selected_planet == 0);
}

TEST_CASE("Next Turn: the contested systems' prompt, Next Day or Go to System; the auto next turn stops at a box",
          "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Game820 g(files);
  game::Session& core = g.core();
  game::Cosmos& cosmos = g.cosmos();
  // War with the species of a ship sharing a system with the player's.
  const game::Ship& ship = g.player_ship_in_system();
  const game::Star& star = cosmos.stars[static_cast<std::size_t>(ship.location)];
  int enemy = -1;
  for (int r = 0; r < cosmos.race_count; ++r) {
    if (r != core.player_race && (star.ships_mask & (1 << r)) != 0) {
      enemy = r;
    }
  }
  REQUIRE(enemy != -1);
  cosmos.races[core.player_race].relation[static_cast<std::size_t>(enemy)] = game::kWar;
  cosmos.races[static_cast<std::size_t>(enemy)].relation[core.player_race] = game::kWar;
  const int day = cosmos.day;
  // Next Turn: "remaining moves" (the box) -> Next Day.
  const GWindow& next = *g.rig.wm.find_window("NEXTTURN");
  const int x = (next.pane().x0 + next.pane().x1) / 2;
  const int y = (next.pane().y0 + next.pane().y1) / 2;
  g.rig.move(x, y);
  g.rig.queue_click(x, y);
  for (int i = 0; i < 8; ++i) {
    g.rig.queue_click(330, 352);  // Next Day, on every contested system
  }
  g.rig.drain();
  CHECK(g.box().event()->code == game::EventCode::kRemainingMoves);
  CHECK(g.box().event()->b == 7);
  CHECK(cosmos.day == day + 1);
  // The next day: Go to System on the first prompt.
  g.rig.wm.find_window("NEXTTURN");
  g.rig.queue_click(x, y);
  g.rig.queue_click(170, 352);
  g.rig.drain();
  CHECK(g.box().event()->code == game::EventCode::kRemainingMoves);
  CHECK(g.rig.wm.current_screen() == 16);
  CHECK(cosmos.day == day + 1);
}

TEST_CASE("a modal window's screen lands in the buffer the Lush Growth Bomb writes into",
          "[day][screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  // `tests/oracle/cov/orch/lush_*.txt` on `lush_save.py`'s save: Pulveran
  // I's bomb completes on day 821 on its out-of-bounds square, byte 62245 of
  // the shared work buffer. Without a modal window that byte is `logo.gif`'s
  // (`lush_plain`: research item 26 granted); after the help box of Next
  // Turn it is a pixel of the star map under the box (`lush_help`: none).
  std::vector<test::ReplayRun> runs;
  for (const char* name : {"lush_help", "lush_plain"}) {
    runs.push_back({.run = std::format("cov/orch/{}_patch", name), .resume = "cov/orch/lush.GAM", .shots = {}});
    runs.push_back({.run = std::format("cov/orch/{}_antag", name),
                    .variant = game::Variant::kAntagonizer,
                    .resume = "cov/orch/lush.GAM",
                    .shots = {}});
  }
  test::replay_oracle_runs(files, runs);
}

TEST_CASE("a dropped order's pointer stays in the saves", "[day][screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  // `tests/oracle/cov/orch/stale_order.txt` on `m5fx/fx_32.GAM`: in the
  // day-820 battle at Feculon, Frigate 1 ordered to the lane end to Sophia,
  // then using its Lane Blocker on another lane end: the order's code is
  // cleared, Sophia's address stays (03.SAV after leaving the System
  // Display, 04.SAV the next day).
  test::replay_oracle_runs(files, {{.run = "cov/orch/stale_order_patch", .resume = "m5fx/fx_32.GAM", .shots = {}},
                                   {.run = "cov/orch/stale_order_antag",
                                    .variant = game::Variant::kAntagonizer,
                                    .resume = "m5fx/fx_32.GAM",
                                    .shots = {}}});
}

TEST_CASE("clicks outside the planet screen's square dialog are swallowed", "[screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  // `tests/oracle/cov/orch/square_dialog.txt`: the dialog of a square of
  // Zoroaster III stays up through a left and a right click outside it.
  test::replay_oracle_runs(files, {{.run = "cov/orch/square_dialog_patch", .keep_saves = true, .shots = {}},
                                   {.run = "cov/orch/square_dialog_antag",
                                    .variant = game::Variant::kAntagonizer,
                                    .keep_saves = true,
                                    .shots = {}}});
}

TEST_CASE("the end of the game: screen 9, the game events dropped", "[screens][content]") {
  OA_REQUIRE_GAME_FILES(files);
  Game820 g(files);
  game::Session& core = g.core();
  game::Cosmos& cosmos = g.cosmos();
  for (int r = 0; r < cosmos.race_count; ++r) {
    if (r != core.player_race) {
      cosmos.races[static_cast<std::size_t>(r)].extinct = -1;
    }
  }
  core.post_event(game::EventCode::kAbilityCharged);
  g.rig.wm.post_message(kMessageNextTurn, 0, 0);
  g.rig.pass();
  CHECK(cosmos.ending == 2);
  CHECK_FALSE(g.session.has_game_events());
  CHECK(g.rig.wm.current_screen() == 9);
}

TEST_CASE("an ANTAG.EXE save loaded with the Standard option continues as an Antagonizer game",
          "[screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  const auto day0 = oracle_path("corpus/idle_neutral_antag_s1/out/02.SAV");
  const auto day2 = oracle_path("corpus/idle_neutral_antag_s1/out/04.SAV");
  if (!day0 || !day2) {
    SKIP("OA_ORACLE is not set to work/oracle with the corpus");
  }
  TempDir dir;
  GameCoreSession session(files, dir.path, false);
  REQUIRE(session.new_game_variant() == game::Variant::kStandard);
  std::filesystem::copy_file(*day0, dir.path / "03.SAV");
  REQUIRE(session.load(3));
  CHECK(session.variant() == game::Variant::kAntagonizer);
  // Two idle days under the Antagonizer's rules and AI: its day-2 save, and
  // the remake's trailer after it.
  for (int day = 0; day < 2; ++day) {
    game::cosmos_next_turn(*session.core());
    session.core()->events.take();
  }
  REQUIRE(session.save(5));
  std::vector<std::uint8_t> ours = read_file(dir.path / "05.SAV");
  const std::vector<std::uint8_t> theirs = read_file(*day2);
  REQUIRE(ours.size() == theirs.size() + 8);
  CHECK(game::parse_save(ours).variant == game::Variant::kAntagonizer);
  ours.resize(theirs.size());
  // Without windows the window state is the loaded one, and `Cosmos_load`
  // keeps the process's clusters (none here, the original's new game's).
  const auto game_part = [](const std::vector<std::uint8_t>& save) {
    game::SaveImage image = game::parse_save(save);
    std::fill(image.clusters.begin(), image.clusters.end(), game::Cluster{});
    std::fill(image.window_state.begin(), image.window_state.end(), 0);
    return without_stale_header(game::serialize_save(image));
  };
  CHECK(first_difference(game_part(ours), game_part(theirs)) == -1);
  // A PATCH.EXE save stays Standard.
  std::filesystem::copy_file(*oracle_path("corpus/idle_neutral_patch_s1/out/02.SAV"), dir.path / "06.SAV");
  REQUIRE(session.load(6));
  CHECK(session.variant() == game::Variant::kStandard);
}

TEST_CASE("tutorials replayed to their end leave the original's game", "[screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  int found = 0;
  const char* only = std::getenv("OA_DAY_UI_RUN");
  for (const std::string name : {"tutintro", "beginnew", "galactic", "battle", "planet", "ship", "species"}) {
    if (only != nullptr && name.find(only) == std::string::npos) {
      continue;
    }
    const CorpusRun run{.run = "tutorials/" + name, .tutorial = true, .turning_view = name == "species"};
    found += replay_run(files, run, 400000) ? 1 : 0;
  }
  if (found == 0) {
    SKIP("OA_ORACLE is not set to work/oracle with the tutorial runs");
  }
}

TEST_CASE("days played through the user interface end in the original's saves", "[day][screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  constexpr game::Variant kAntag = game::Variant::kAntagonizer;
  int found = 0;
  const char* only = std::getenv("OA_DAY_UI_RUN");
  for (const CorpusRun& c : std::vector<CorpusRun>{
           {.run = "corpus/day0_patch_s1"},
           {.run = "corpus/day0_patch_s2", .seed = 2},
           {.run = "corpus/day0_patch_s3", .seed = 3},
           {.run = "corpus/day0_patch_s12345", .seed = 12345},
           {.run = "corpus/idle_neutral_patch_s1"},
           {.run = "corpus/idle_hostile_patch_s1"},
           {.run = "corpus/idle_peaceful_patch_s1"},
           {.run = "corpus/idle_neutral_patch_s12345", .seed = 12345},
           {.run = "corpus/selfmanaged_neutral_patch_s1"},
           {.run = "corpus/selfmanaged_hostile_patch_s1"},
           {.run = "corpus/selfmanaged_peaceful_patch_s1"},
           {.run = "corpus/resume820_patch", .keep_saves = true},
           {.run = "corpus/resume820_daily_patch", .keep_saves = true},
           {.run = "corpus/resume820_endlessparty_patch", .keep_saves = true},
           {.run = "corpus/small15_7species_patch_s1"},
           {.run = "corpus/large100_4species_patch_s1"},
           {.run = "corpus/resume820_selfmanaged_patch", .keep_saves = true},
           {.run = "corpus/resume820_shipdesign_patch", .keep_saves = true},
           {.run = "corpus/actions_patch_s1"},
           {.run = "corpus/resume820_battle_patch", .keep_saves = true},
           // `tests/oracle/event_boxes_*.txt`: screenshots of the boxes.
           {.run = "events/boxes_820_patch", .keep_saves = true},
           {.run = "events/boxes_new_patch_s1"},
           // `tests/oracle/event13_820.txt`: a colony invaded (event 13).
           {.run = "events/event13_820", .resume = "exp/event13.GAM"},
           // `ANTAG.EXE`'s new games.
           {.run = "corpus/day0_antag_s1", .variant = kAntag},
           {.run = "corpus/day0_antag_s2", .variant = kAntag, .seed = 2},
           {.run = "corpus/day0_antag_s3", .variant = kAntag, .seed = 3},
           {.run = "corpus/day0_antag_s12345", .variant = kAntag, .seed = 12345},
           {.run = "corpus/idle_neutral_antag_s1", .variant = kAntag},
           {.run = "corpus/idle_hostile_antag_s1", .variant = kAntag},
           {.run = "corpus/idle_peaceful_antag_s1", .variant = kAntag},
           {.run = "corpus/idle_neutral_antag_s12345", .variant = kAntag, .seed = 12345},
           {.run = "corpus/selfmanaged_neutral_antag_s1", .variant = kAntag},
           {.run = "corpus/selfmanaged_hostile_antag_s1", .variant = kAntag},
           {.run = "corpus/selfmanaged_peaceful_antag_s1", .variant = kAntag},
           {.run = "corpus/small15_7species_antag_s1", .variant = kAntag},
           {.run = "corpus/large100_4species_antag_s1", .variant = kAntag},
           {.run = "corpus/actions_antag_s1", .variant = kAntag},
           // `ANTAG.EXE` on the shipped day-820 game.
           {.run = "corpus/resume820_antag", .variant = kAntag, .keep_saves = true},
           {.run = "corpus/resume820_daily_antag", .variant = kAntag, .keep_saves = true},
           {.run = "corpus/resume820_selfmanaged_antag", .variant = kAntag, .keep_saves = true},
           {.run = "corpus/resume820_battle_antag", .variant = kAntag, .keep_saves = true},
       }) {
    if (only != nullptr && c.run.find(only) == std::string::npos) {
      continue;
    }
    found += replay_run(files, c, 400000) ? 1 : 0;
  }
  if (found == 0) {
    SKIP("OA_ORACLE is not set to work/oracle with the corpus");
  }
}

}  // namespace
}  // namespace oa
