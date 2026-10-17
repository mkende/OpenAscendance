// The original's oracle runs replayed in the remake (`docs/12-oracle.md`):
// the program as `main` runs it, headless on a virtual clock, fed the run's
// own window-event trace (`trace.bin`, the original's third argument) on the
// same start-up galaxy; its saves and Alt+G screenshots land in a folder of
// its own, to compare with the run's `out/` files. OA_DATA install and
// OA_ORACLE (`work/oracle`).
#pragma once

#include <catch2/catch_test_macros.hpp>

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
#include "core_session.h"
#include "game/new_game.h"
#include "gfx/gsystem.h"
#include "platform/clock.h"
#include "platform/input.h"
#include "ui/game_core_session.h"
#include "ui/winmgr.h"
#include "ui_rig.h"

namespace oa::test {

// A folder of its own, removed afterwards.
struct ReplayDir {
  ReplayDir() {
    std::random_device rd;
    path = std::filesystem::temp_directory_path() / std::format("oa_replay_{:08x}", rd());
    std::filesystem::create_directories(path);
  }
  ~ReplayDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
  ReplayDir(const ReplayDir&) = delete;
  ReplayDir& operator=(const ReplayDir&) = delete;

  std::filesystem::path path;
};

// Returns the file or folder `relative` of OA_ORACLE, nothing when unset or
// missing.
inline std::optional<std::filesystem::path> oracle_run_path(const std::string& relative) {
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

// The program replaying a trace: the start-up galaxy of `cosmos.txt` (50
// stars, 7 races, `seed`), the windows, the opening menu, the pointer drawn
// into the shown frame (the Alt+G screenshots show it) and moved by the
// replay.
struct ReplayProgram {
  // `install_saves`: the install's `01.SAV` and `RESUME.GAM` are seen (the
  // oracle's `--keep-saves`).
  ReplayProgram(const GameFiles& f, std::int32_t seed, const std::filesystem::path& dir, bool install_saves)
      : files(f),
        gs([this] { clock.wait_vblank(); }),
        sound(f, &mixer),
        session(f, dir, install_saves),
        pointer_shapes(f.load("DATA\\MOUSE.SHP"), "DATA\\MOUSE.SHP"),
        wm((start(seed), UiServices{.files = f,
                                    .gsystem = gs,
                                    .sound = sound,
                                    .game = session,
                                    .user_dir = dir,
                                    .now_us = [this] { return clock.micros(); },
                                    .ticks = [this] { return clock.ticks(); },
                                    .idle = [this] { clock.idle(); },
                                    .show_pointer = [this](bool shown) { pointer = shown; },
                                    .set_pointer_frame = [this](int fr) { pointer_frame = fr; },
                                    .poll_input = [this]() -> const InputState& { return input.poll(); },
                                    .present = [this] { present(); },
                                    .shown_frame = [this]() -> const Image8& { return frame; },
                                    .warp_pointer = [this](int x, int y) { input.warp(x, y); }}),
           full_factory()) {
    wm.setup_race_shapes();
    gs.load_palette(files, "DATA\\game.pal", 0, 256);
    gs.set_dac_range(0, 256);
    wm.set_screen(0, false);
  }

  void start(std::int32_t seed) {
    const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = seed, .species = {}};
    session.start(0, &txt);
  }

  void present() {
    frame = gs.screen();
    if (pointer) {
      const InputState& s = input.state();
      vfx_shape_draw(frame, GSystem::kScreenPane, pointer_shapes.at(static_cast<std::size_t>(pointer_frame)), s.x, s.y);
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
  Image8 frame;
  bool pointer = true;
  int pointer_frame = 0;
  WinMgr wm;
};

// Counts the pixels of two screenshots that differ.
inline int differing_gif_pixels(const std::filesystem::path& ours, const std::filesystem::path& theirs) {
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
inline std::ptrdiff_t first_byte_difference(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    if (a[i] != b[i]) {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return a.size() == b.size() ? -1 : static_cast<std::ptrdiff_t>(std::min(a.size(), b.size()));
}

// Clears the stale stack bytes of a save header (`Game_save` 0x4f458): the
// per-race entries past the race count and the last 8 bytes.
inline std::vector<std::uint8_t> without_stale_save_header(std::vector<std::uint8_t> save) {
  const int races = save.at(4);
  for (const std::size_t table : {0x0cU, 0x28U, 0x44U}) {
    for (int r = races; r < 7; ++r) {
      std::fill_n(save.begin() + static_cast<std::ptrdiff_t>(table + 4 * static_cast<std::size_t>(r)), 4, 0);
    }
  }
  std::fill_n(save.begin() + 0x6c, 8, 0);
  return save;
}

// An oracle run to replay.
struct ReplayRun {
  std::string run;  // under OA_ORACLE
  std::int32_t seed = 1;
  bool keep_saves = false;
  // The screenshots compared (`SCRnnn.GIF`), all when empty.
  std::vector<std::string> shots;
};

// Replays `c` and compares the saves and the screenshots the original
// wrote; with OA_DUMP_DIR the remake's files are copied there. Returns
// false when the run is missing.
inline bool replay_oracle_run(const GameFiles& files, const ReplayRun& c, int max_passes = 400000) {
  const auto trace = oracle_run_path(c.run + "/trace.bin");
  const auto out = oracle_run_path(c.run + "/out");
  if (!trace || !out) {
    return false;
  }
  INFO(c.run);
  ReplayDir dir;
  ReplayProgram program(files, c.seed, dir.path, c.keep_saves);
  CHECK(program.replay(read_file(*trace), max_passes));
  if (const char* dump = std::getenv("OA_DUMP_DIR"); dump != nullptr && *dump != '\0') {
    const std::filesystem::path to = std::filesystem::path(dump) / c.run;
    std::filesystem::create_directories(to);
    std::filesystem::copy(dir.path, to,
                          std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
  }
  for (const auto& entry : std::filesystem::directory_iterator(*out)) {
    const std::string name = entry.path().filename().string();
    const std::filesystem::path ours = dir.path / name;
    if (name.ends_with(".SAV") || name == "RESUME.GAM") {
      INFO(name);
      REQUIRE(std::filesystem::exists(ours));
      CHECK(first_byte_difference(without_stale_save_header(read_file(ours)),
                                  without_stale_save_header(read_file(entry.path()))) == -1);
    } else if (name.ends_with(".GIF") &&
               (c.shots.empty() || std::find(c.shots.begin(), c.shots.end(), name) != c.shots.end())) {
      INFO(name);
      std::string lower = name;
      for (char& ch : lower) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      REQUIRE(std::filesystem::exists(dir.path / lower));
      CHECK(differing_gif_pixels(dir.path / lower, entry.path()) == 0);
    }
  }
  return true;
}

}  // namespace oa::test
