// The species selection (screen 7), negotiation (13) and intelligence (14)
// screens and the special abilities window on the shipped day-820 game,
// replaying the oracle's `tests/oracle/diplomacy_820.txt` (peace with the
// Capelons: trades, an alliance proposal, war and peace) and
// `diplomacy_820_contacts.txt` (a hand-edited save: three species met, an
// alliance, a pending alien proposal, the third parties), `ability_820.txt`
// (the ability charged and used): the original's screenshots and saves
// (OA_ORACLE), and the screens' states.

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "assets/gif.h"
#include "assets/static_strings.h"
#include "core_session.h"
#include "game/negotiation.h"
#include "game/raceneg.h"
#include "game/save.h"
#include "game_data.h"
#include "game_oracle.h"
#include "screen_shots.h"
#include "screens/cos_wnd.h"
#include "screens/intel_wnd.h"
#include "screens/neg_wnd.h"
#include "ui/game_core_session.h"
#include "ui/help_wnd.h"
#include "ui/list_box.h"
#include "ui/text_box.h"
#include "ui_rig.h"

namespace oa {
namespace {

constexpr int kScreenSpecies = 7;
constexpr int kScreenDiplomacy = 13;
constexpr int kScreenIntelligence = 14;

// A folder of its own, removed afterwards.
struct TempDir {
  TempDir() {
    std::random_device rd;
    path = std::filesystem::temp_directory_path() / std::format("oa_diplomacy_{:08x}", rd());
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

// The race records in a save file (after the cosmos' race count).
constexpr std::size_t kSaveRaces = 0xfa0a;
constexpr std::size_t kRaceSize = 0x1ee;
constexpr std::size_t kHeaderRelations = 0x44;

void put_u8(std::vector<std::uint8_t>& save, std::size_t at, std::uint8_t v) { save.at(at) = v; }
void put_i16(std::vector<std::uint8_t>& save, std::size_t at, std::int16_t v) {
  save.at(at) = static_cast<std::uint8_t>(v);
  save.at(at + 1) = static_cast<std::uint8_t>(static_cast<std::uint16_t>(v) >> 8);
}

// Sets `rel(a, b)` and, when given, `A(a->b)` in a save file (and the
// header's copy of the player's relations).
void set_relation(std::vector<std::uint8_t>& save, std::size_t a, std::size_t b, std::uint8_t relation,
                  std::optional<std::int16_t> attitude = std::nullopt) {
  const std::size_t race = kSaveRaces + a * kRaceSize;
  put_u8(save, race + 0x1c0 + b, relation);
  if (attitude) {
    put_i16(save, race + 0x1b2 + 2 * b, *attitude);
  }
  if (a == 0) {
    put_u8(save, kHeaderRelations + 4 * b, relation);
  }
}

// The shipped `01.SAV` edited as the oracle's `dip820_contacts.SAV`
// (`tests/oracle/diplomacy_820_contacts.txt`): the Chamachies (0) also meet
// the Minions (2) and the Arbryls (3) at peace; they are allied with the
// Capelons (1), who are at war with the Minions; the Arbryls wait to
// propose an alliance (pending action 3).
std::vector<std::uint8_t> contacts_save(const GameFiles& files) {
  std::vector<std::uint8_t> save = files.load("01.SAV");
  set_relation(save, 0, 1, game::kAlliance);
  set_relation(save, 0, 2, game::kPeace, 400);
  set_relation(save, 0, 3, game::kPeace, 400);
  set_relation(save, 1, 0, game::kAlliance, 800);
  set_relation(save, 1, 2, game::kWar);
  set_relation(save, 2, 1, game::kWar);
  set_relation(save, 2, 0, game::kPeace, 500);
  set_relation(save, 3, 0, game::kPeace, 700);
  put_u8(save, kSaveRaces + 3 * kRaceSize + 0x1e9, game::kDipProposeAlliance);
  return save;
}

// The day-820 game started as the oracle's runs (`cosmos.txt` seed 1, the
// start-up galaxy, `save` installed as RESUME.GAM and loaded by the opening
// menu), replayed with the scripts' steps, a pass each.
struct DiplomacyRig {
  DiplomacyRig(const GameFiles& files, const std::vector<std::uint8_t>& save)
      : session((install(save), files), dir.path, false), rig((start(), files), &session, dir.path) {
    // The oracle run's pointer: the original's hover.
    rig.wm.set_enlarged_targets(false);
  }

  void install(const std::vector<std::uint8_t>& save) const {
    std::ofstream(dir.path / "RESUME.GAM", std::ios::binary)
        .write(reinterpret_cast<const char*>(save.data()), static_cast<std::streamsize>(save.size()));
  }
  void start() {
    const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = 1, .species = {}};
    session.start(0, &txt);
  }

  void wait(int passes) {
    for (int i = 0; i < passes; ++i) {
      rig.pass();
    }
  }
  void click(int x, int y, MouseButton button = MouseButton::kLeft) {
    rig.move(x, y);
    rig.click(x, y, button);
  }
  // `click NAME`: the window's centre.
  void click(std::string_view name) {
    const GWindow* w = rig.wm.find_window(name);
    REQUIRE(w != nullptr);
    click((w->pane().x0 + w->pane().x1) / 2, (w->pane().y0 + w->pane().y1) / 2);
  }
  void key(std::uint32_t sdl_key) { rig.pass(InputEvent{.type = EventType::kKeyDown, .key = sdl_key}); }
  // The steps of a modal window, queued for its loop: the pointer stays
  // where the last step left it.
  void queue_wait(int passes) {
    for (int i = 0; i < passes; ++i) {
      rig.pending.push_back(InputEvent{.type = EventType::kMouseMove, .x = px, .y = py});
    }
  }
  void queue_click(int x, int y) {
    px = x;
    py = y;
    queue_wait(1);
    rig.queue_click(x, y);
  }
  // Alt+G (the remake's `scrNNN.gif`, the original's `SCRnnn.GIF`), then the
  // script's 2-pass wait.
  void queue_screenshot() {
    rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = 'g', .mods = SDL_KMOD_LALT});
    rig.pending.push_back(InputEvent{.type = EventType::kKeyUp, .key = 'g', .mods = 0});
    queue_wait(1);
  }
  // The remake's Alt+G screenshot `index`, also copied to OA_SHOTS as `name`.
  Image8 alt_g_shot(int index, std::string_view name) const {
    const std::filesystem::path gif = dir.path / std::format("scr{:03d}.gif", index);
    REQUIRE(std::filesystem::exists(gif));
    if (const char* shots = std::getenv("OA_SHOTS"); shots != nullptr && *shots != '\0') {
      std::filesystem::copy_file(gif, std::filesystem::path(shots) / (std::string(name) + ".gif"),
                                 std::filesystem::copy_options::overwrite_existing);
    }
    return decode_gif(test::read_file(gif), gif.string());
  }
  // The script's `save 2`: Alt+S, then the save list's key 1 (02.SAV).
  void save2() {
    rig.key('s', SDL_KMOD_LALT);
    wait(40);
    rig.key('1');
    wait(40);
  }

  // `screenshot`: the frame Alt+G saves is the screen before its pass.
  Image8 screenshot(std::string_view name) {
    test::write_shot(rig, name);
    Image8 shot = rig.wm.screen();
    wait(3);
    return shot;
  }

  // The steps of `_resume820.txt` up to the star map.
  void to_star_map() {
    wait(40);
    click("RESUME");
    wait(60);
  }

  game::Session& core() { return *session.core(); }
  game::Race& race(int r) { return core().game->cosmos.races.at(static_cast<std::size_t>(r)); }
  NegWnd& negotiation() { return dynamic_cast<NegWnd&>(*rig.wm.find_window("NEGSCREEN")); }
  ListBox& options() { return dynamic_cast<ListBox&>(*rig.wm.find_window("PLAYERDIP")); }
  TextBox& alien() { return dynamic_cast<TextBox&>(*rig.wm.find_window("ALIENDIP")); }
  // The player's options as their codes.
  std::vector<int> option_codes() {
    std::vector<int> codes;
    for (int i = 0; i < options().count(); ++i) {
      codes.push_back(options().item(i).data);
    }
    return codes;
  }

  TempDir dir;
  GameCoreSession session;
  test::UiRig rig;
  // The pointer of the queued steps.
  int px = 0;
  int py = 0;
};

// Counts the pixels of `shot` differing from the original's screenshot
// `gif` outside the pointer at (px, py).
int differing_pixels(const Image8& shot, const std::filesystem::path& gif, int px, int py) {
  const Image8 original = decode_gif(test::read_file(gif), gif.string());
  int n = 0;
  for (int y = 0; y < original.height(); ++y) {
    for (int x = 0; x < original.width(); ++x) {
      const bool pointer = x >= px - 16 && x <= px + 16 && y >= py - 16 && y <= py + 16;
      if (!pointer && original.at(x, y) != shot.at(x, y)) {
        ++n;
      }
    }
  }
  return n;
}

// Compares `shot` with the original's `SCRnnn.GIF` of `run` when OA_ORACLE
// holds it.
void check_shot(const Image8& shot, const std::string& run, int index, int px, int py) {
  if (const auto gif = oracle_path(std::format("{}/out/SCR{:03d}.GIF", run, index))) {
    INFO(run << " SCR" << index);
    CHECK(differing_pixels(shot, *gif, px, py) == 0);
  }
}

// Returns the save file at `path` with the stale stack bytes of its header
// cleared: the per-race entries past the race count and the last 8 bytes.
std::vector<std::uint8_t> save_without_stale_header(const std::filesystem::path& path) {
  std::vector<std::uint8_t> save = test::read_file(path);
  const int races = save.at(4);
  for (const std::size_t table : {0x0cU, 0x28U, 0x44U}) {
    for (int r = races; r < 7; ++r) {
      std::fill_n(save.begin() + static_cast<std::ptrdiff_t>(table + 4 * static_cast<std::size_t>(r)), 4, 0);
    }
  }
  std::fill_n(save.begin() + 0x6c, 8, 0);
  return save;
}

// Compares the remake's `02.SAV` with the original's of `run` (OA_ORACLE).
void check_save(const std::filesystem::path& dir, const std::string& run) {
  const std::filesystem::path saved = dir / "02.SAV";
  REQUIRE(std::filesystem::exists(saved));
  if (const auto original = oracle_path(run + "/out/02.SAV")) {
    CHECK(game::oracle_test::first_difference(save_without_stale_header(saved), save_without_stale_header(*original)) ==
          -1);
  }
}

// The PLAYERDIP row `row`'s centre.
constexpr int kOptionX = 450;
int option_y(int row) { return 340 + 29 * row; }

}  // namespace

TEST_CASE("diplomacy on the day-820 game: trades, an alliance proposal, war and peace", "[diplomacy][content]") {
  OA_REQUIRE_GAME_FILES(files);
  DiplomacyRig d(files, files.load("01.SAV"));
  const std::string run = "diplomacy_820";
  using game::DipAction;
  d.to_star_map();
  d.click("RACES");
  d.wait(40);
  d.click("RACE_2");
  d.wait(60);
  REQUIRE(d.rig.wm.current_screen() == kScreenDiplomacy);
  CHECK(d.rig.wm.target_race() == 1);
  // At peace, more than 4 days after the last trades: every trade.
  CHECK(d.option_codes() == std::vector<int>{14, 2, 3, 4, 5, 6});
  CHECK(d.negotiation().negotiation()->third_parties().empty());
  check_shot(d.screenshot("dip820_00_open"), run, 0, 553, 61);
  const auto choose = [&](int row, int index, std::string_view name) {
    d.click(kOptionX, option_y(row));
    d.wait(10);
    check_shot(d.screenshot(std::format("dip820_{:02d}_{}", index, name)), run, index, kOptionX, option_y(row));
  };
  choose(3, 1, "lanes");
  CHECK(d.core().game->last_trade_days[0] == 820);
  CHECK(d.option_codes() == std::vector<int>{14, 2, 3, 5, 6});
  choose(4, 2, "research");
  CHECK(d.option_codes() == std::vector<int>{14, 2, 3, 5});
  choose(3, 3, "stars");
  CHECK(d.option_codes() == std::vector<int>{14, 2, 3});
  choose(2, 4, "alliance");
  // A star of the mini map: the selected system's box names it.
  d.click(266, 66);
  d.wait(10);
  check_shot(d.screenshot("dip820_05_star"), run, 5, 266, 66);
  choose(1, 6, "war");
  CHECK(d.race(0).relation[1] == game::kWar);
  CHECK(d.option_codes() == std::vector<int>{14, 13});
  choose(1, 7, "peace");
  d.click(kOptionX, option_y(0));  // Goodbye
  d.wait(40);
  CHECK(d.rig.wm.current_screen() == kScreenSpecies);
  check_shot(d.screenshot("dip820_08_species"), run, 8, kOptionX, option_y(0));
  d.click("Intelligence");
  d.wait(40);
  CHECK(d.rig.wm.current_screen() == kScreenIntelligence);
  auto& intel = dynamic_cast<IntelWnd&>(*d.rig.wm.find_window("INTELSCREEN"));
  REQUIRE(intel.row_count() == 2);
  check_shot(d.screenshot("dip820_09_intel"), run, 9, 553, 318);
  d.key(SDLK_ESCAPE);
  d.wait(30);
  d.key(SDLK_ESCAPE);
  d.wait(40);
  // The ability window runs modally: its steps are queued, and its picture
  // is the remake's own Alt+G screenshot.
  const GWindow* button = d.rig.wm.find_window("SPECIALAB");
  REQUIRE(button != nullptr);
  const int x = (button->pane().x0 + button->pane().x1) / 2;
  const int y = (button->pane().y0 + button->pane().y1) / 2;
  d.queue_click(x, y);
  d.queue_wait(20);
  d.queue_screenshot();
  d.rig.pending.push_back(InputEvent{.type = EventType::kKeyDown, .key = SDLK_ESCAPE});
  d.rig.drain();
  check_shot(d.alt_g_shot(0, "dip820_10_ability"), run, 10, x, y);
  d.wait(20);
  // The save after the dialogue equals the original's.
  d.save2();
  check_save(d.dir.path, run);
}

TEST_CASE("diplomacy with third parties: an alien proposal, alliance options, third-party actions",
          "[diplomacy][content]") {
  OA_REQUIRE_GAME_FILES(files);
  DiplomacyRig d(files, contacts_save(files));
  const std::string run = "diplomacy_820_contacts";
  int index = 0;
  const auto shot = [&](std::string_view name, int px, int py) {
    check_shot(d.screenshot(std::format("dip820c_{:02d}_{}", index, name)), run, index, px, py);
    ++index;
  };
  const auto choose = [&](int row, std::string_view name) {
    d.click(kOptionX, option_y(row));
    d.wait(10);
    shot(name, kOptionX, option_y(row));
  };
  d.to_star_map();
  d.click("RACES");
  d.wait(40);
  shot("species", 553, 301);
  // The Arbryls' pending alliance proposal: the player answers.
  d.click("RACE_4");
  d.wait(60);
  const game::Negotiation* n = d.negotiation().negotiation();
  REQUIRE(n != nullptr);
  CHECK(n->replying());
  CHECK(d.option_codes() == std::vector<int>{4, 5});
  CHECK(n->third_parties() == std::vector<game::RaceIndex>{1, 2});
  shot("proposal", 553, 139);
  choose(0, "accepted");
  CHECK_FALSE(n->replying());
  CHECK(d.race(3).pending_action == game::kDipNone);
  CHECK(d.race(0).relation[3] == game::kAlliance);
  CHECK(d.race(3).relation[0] == game::kAlliance);
  // Allied: break, the trades, help, then by the Arbryls' relation to the
  // Capelons (peace): war on them, an alliance with them.
  CHECK(d.option_codes() == std::vector<int>{14, 8, 4, 5, 6, 12, 9, 11});
  d.click(297, 358);
  d.wait(10);
  CHECK(n->selected_third_party() == 1);
  shot("third", 297, 358);
  d.click(kOptionX, option_y(0));  // Goodbye
  d.wait(40);
  // The Capelons, allied, at war with the Minions (the first third party).
  d.click("RACE_2");
  d.wait(60);
  n = d.negotiation().negotiation();
  CHECK(n->third_parties() == std::vector<game::RaceIndex>{2, 3});
  CHECK(d.option_codes() == std::vector<int>{14, 8, 4, 5, 6, 12, 7});
  shot("capelons", 553, 61);
  choose(4, "research");
  choose(2, "lanes");
  CHECK(d.option_codes() == std::vector<int>{14, 8, 5, 12, 7});
  choose(3, "help");
  choose(3, "peace_minions");
  d.click(297, 358);
  d.wait(10);
  CHECK(d.option_codes() == std::vector<int>{14, 8, 5, 9, 11});
  shot("arbryls", 297, 358);
  choose(4, "alliance_arbryls");
  choose(3, "war_arbryls");
  d.click(kOptionX, option_y(0));  // Goodbye
  d.wait(40);
  shot("species_after", kOptionX, option_y(0));
  d.click("Intelligence");
  d.wait(40);
  auto& intel = dynamic_cast<IntelWnd&>(*d.rig.wm.find_window("INTELSCREEN"));
  CHECK(intel.row_count() == 4);
  shot("intel", 553, 318);
  d.key(SDLK_ESCAPE);
  d.wait(30);
  d.key(SDLK_ESCAPE);
  d.wait(40);
  d.rig.key('s', SDL_KMOD_LALT);
  d.wait(40);
  d.rig.key('1');
  d.wait(40);
  check_save(d.dir.path, run);
}

TEST_CASE("the special abilities window: ready, used, its result", "[diplomacy][content]") {
  OA_REQUIRE_GAME_FILES(files);
  // The shipped game with the Chamachies' ability charged (89 days).
  std::vector<std::uint8_t> save = files.load("01.SAV");
  save.at(kSaveRaces + 0x19e) = 89;
  DiplomacyRig d(files, save);
  const std::string run = "ability_820";
  d.to_star_map();
  const auto project = d.core().game->research.project.at(0);
  REQUIRE(project >= 0);
  const GWindow* button = d.rig.wm.find_window("SPECIALAB");
  REQUIRE(button != nullptr);
  d.queue_click((button->pane().x0 + button->pane().x1) / 2, (button->pane().y0 + button->pane().y1) / 2);
  d.queue_wait(20);
  d.queue_screenshot();
  d.queue_click(167, 352);  // Use Ability
  d.queue_wait(20);
  d.queue_screenshot();
  d.queue_click(247, 352);  // OK
  d.rig.drain();
  CHECK_FALSE(d.rig.wm.in_modal());
  check_shot(d.alt_g_shot(0, "ability_00_ready"), run, 0, 553, 262);
  check_shot(d.alt_g_shot(1, "ability_01_used"), run, 1, 167, 352);
  CHECK(d.race(0).ability_charge == 0);
  d.wait(20);
  check_shot(d.screenshot("ability_02_map"), run, 2, 247, 352);
  d.save2();
  check_save(d.dir.path, run);
}

TEST_CASE("species selection: a met species' portrait opens the negotiation, the player's does not",
          "[diplomacy][content]") {
  OA_REQUIRE_GAME_FILES(files);
  DiplomacyRig d(files, files.load("01.SAV"));
  d.to_star_map();
  d.click("RACES");
  d.wait(40);
  REQUIRE(d.rig.wm.current_screen() == kScreenSpecies);
  const auto& map = dynamic_cast<const CosWnd&>(*d.rig.wm.find_window("COSMOSWnd"));
  // The portraits' centres are window-relative, the click test compares
  // them with the pointer's screen position (CosWnd_handle_event).
  const ScreenPoint own = map.circle()[0];
  d.click(own.x, own.y);
  d.wait(10);
  CHECK(d.rig.wm.current_screen() == kScreenSpecies);
  const ScreenPoint capelons = map.circle()[1];
  d.click(capelons.x + 45, capelons.y - 45);
  d.wait(10);
  CHECK(d.rig.wm.current_screen() == kScreenDiplomacy);
  CHECK(d.rig.wm.target_race() == 1);
  // Esc goes back to the species selection.
  d.key(SDLK_ESCAPE);
  d.wait(30);
  CHECK(d.rig.wm.current_screen() == kScreenSpecies);
}

TEST_CASE("species selection: the remake greys an extinct species' button", "[diplomacy]") {
  OA_REQUIRE_GAME_FILES(files);
  DiplomacyRig d(files, files.load("01.SAV"));
  d.to_star_map();
  d.race(2).extinct = -1;
  d.click("RACES");
  d.wait(40);
  REQUIRE(d.rig.wm.current_screen() == kScreenSpecies);
  const GWindow& extinct = *d.rig.wm.find_window("RACE_3");
  CHECK(extinct.greyed());
  CHECK_FALSE(d.rig.wm.find_window("RACE_1")->greyed());
  CHECK_FALSE(d.rig.wm.find_window("RACE_2")->greyed());
  // Hovered, it does not light up; a click does nothing; its help works.
  const Pane& p = extinct.pane();
  d.rig.move(p.x0 + 2, p.y0 + 2);
  test::write_shot(d.rig, "species_extinct");
  CHECK(d.rig.wm.screen().at(p.x0 + 2, p.y0 + 2) != GWindow::kHotBackColor);
  d.click("RACE_3");
  d.wait(10);
  CHECK(d.rig.wm.current_screen() == kScreenSpecies);
  d.rig.queue_help_click(p.x0 + 2, p.y0 + 2);
  d.rig.queue_click(320, 350);  // the help window's OK
  d.rig.drain();
  CHECK(dynamic_cast<HelpWnd&>(*d.rig.wm.find_window("HELPWINDOW")).topic() == "racesel");
}

}  // namespace oa
