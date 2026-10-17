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
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "assets/gif.h"
#include "assets/shp.h"
#include "audio/mixer.h"
#include "audio/sound_system.h"
#include "core/install.h"
#include "game/new_game.h"
#include "game/save.h"
#include "game_data.h"
#include "gfx/gsystem.h"
#include "platform/clock.h"
#include "platform/input.h"
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
// clock: the start-up galaxy of `cosmos.txt` (50 stars, 7 races, `seed`),
// the windows, the opening menu, the pointer drawn into the shown frame
// (the Alt+G screenshots show it) and moved by the replay.
struct Program {
  // `install_saves`: the install's `01.SAV` and `RESUME.GAM` are seen (the
  // oracle's `--keep-saves`).
  Program(const GameFiles& f, std::int32_t seed, const std::filesystem::path& dir, bool install_saves)
      : files(f),
        gs([this] { clock.wait_vblank(); }),
        sound(f, &mixer),
        session(f, dir, install_saves),
        pointer_shapes(f.load("DATA\\MOUSE.SHP"), "DATA\\MOUSE.SHP"),
        wm((start(seed),
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
    gs.load_palette(files, "DATA\\game.pal", 0, 256);
    gs.set_dac_range(0, 256);
    wm.set_screen(0, false);
  }

  void start(std::int32_t seed) {
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

// A run of the corpus (`tests/oracle/corpus/cases.txt`, PATCH).
struct CorpusRun {
  std::string run;
  std::int32_t seed = 1;
  bool keep_saves = false;
  // A tutorial run: its help boxes closed and the game quit at its end.
  bool tutorial = false;
};

// Replays the corpus run `c` and compares every file the original wrote.
// Returns false when the run is missing.
bool replay_run(const GameFiles& files, const CorpusRun& c, int max_passes) {
  const auto trace = oracle_path(c.run + "/trace.bin");
  const auto out = oracle_path(c.run + "/out");
  if (!trace || !out) {
    return false;
  }
  INFO(c.run);
  TempDir dir;
  Program program(files, c.seed, dir.path, c.keep_saves);
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
      CHECK(first_difference(without_stale_header(read_file(ours)), without_stale_header(read_file(entry.path()))) ==
            -1);
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

TEST_CASE("tutorials replayed to their end leave the original's game", "[screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
  int found = 0;
  for (const std::string name : {"beginnew", "galactic", "planet"}) {
    found += replay_run(files, {.run = "tutorials/" + name, .tutorial = true}, 400000) ? 1 : 0;
  }
  if (found == 0) {
    SKIP("OA_ORACLE is not set to work/oracle with the tutorial runs");
  }
}

TEST_CASE("days played through the user interface end in the original's saves", "[day][screens][content][oracle]") {
  OA_REQUIRE_GAME_FILES(files);
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
