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
#include <fstream>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "assets/gif.h"
#include "assets/shp.h"
#include "audio/mixer.h"
#include "audio/sound_system.h"
#include "core/install.h"
#include "core_session.h"
#include "game/new_game.h"
#include "game/save.h"
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

// The program replaying a trace, as `PATCH.EXE` (`variant` Standard) or
// `ANTAG.EXE` (Antagonizer, the variant of its new games): the start-up
// galaxy of `cosmos.txt` (50 stars, 7 races, `seed`), the windows, the
// opening menu, the pointer drawn into the shown frame (the Alt+G
// screenshots show it) and moved by the replay.
struct ReplayProgram {
  // `install_saves`: the install's `01.SAV` and `RESUME.GAM` are seen (the
  // oracle's `--keep-saves`).
  ReplayProgram(const GameFiles& f, std::int32_t seed, const std::filesystem::path& dir, bool install_saves,
                game::Variant variant = game::Variant::kStandard)
      : files(f),
        gs([this] { clock.wait_vblank(); }),
        sound(f, &mixer),
        session(f, dir, install_saves),
        pointer_shapes(f.load("DATA\\MOUSE.SHP"), "DATA\\MOUSE.SHP"),
        wm((start(seed, variant), UiServices{.files = f,
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

  void start(std::int32_t seed, game::Variant variant) {
    session.set_new_game_variant(variant);
    const game::CosmosTxt txt{.stars = 50, .races = 7, .seed = seed, .species = {}};
    session.start(0, &txt);
  }

  void present() {
    if (on_present) {
      on_present();
    }
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
  // Called at every shown frame, modal loops included (a test's probe).
  std::function<void()> on_present;
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

// The bytes of a save with its stale list slots cleared: the stars' lane
// and planet lists and the races' AI targets past their counts (the
// tutorials' games hold their author's process's pointers there, which the
// original writes back as they were), and the header's stale bytes.
// `foreign_cosmos`: the cosmos address of the stale pointers of another
// process (`game::parse_save`).
inline std::vector<std::uint8_t> without_stale_slots(const std::vector<std::uint8_t>& save,
                                                     std::optional<game::RawAddress> foreign_cosmos = std::nullopt) {
  game::SaveImage image = game::parse_save(save, foreign_cosmos);
  for (game::Star& star : image.stars) {
    for (std::size_t i = static_cast<std::size_t>(star.lane_count); i < star.lanes.size(); ++i) {
      star.lanes[i] = game::kNone;
    }
    for (std::size_t i = static_cast<std::size_t>(star.planet_count); i < star.planets.size(); ++i) {
      star.planets[i] = game::kNone;
    }
  }
  for (game::Race& race : image.races) {
    for (std::size_t i = static_cast<std::size_t>(race.target_count); i < race.targets.size(); ++i) {
      race.targets[i] = game::kNone;
    }
  }
  return without_stale_save_header(game::serialize_save(image));
}

// Writes `bytes` to `path`.
inline void write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream(path, std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

// Returns the save `bytes` of a `PATCH.EXE` game as the remake's save of the
// same game played under `variant` (its trailer), unchanged when it already
// is one of that variant's.
inline std::vector<std::uint8_t> as_variant_save(const std::vector<std::uint8_t>& bytes, game::Variant variant) {
  game::SaveImage image = game::parse_save(bytes);
  if (image.cosmos_address != game::kPatchCosmosAddress || variant == game::Variant::kStandard) {
    return bytes;
  }
  image.variant = variant;
  return game::serialize_save(image);
}

// An oracle run to replay.
struct ReplayRun {
  std::string run;  // under OA_ORACLE
  // The original's executable: `PATCH.EXE` or `ANTAG.EXE` (Antagonizer).
  // `ANTAG.EXE` plays the `PATCH.EXE` saves it loads (`keep_saves`,
  // `resume`) under its own rules but writes their stale list slots back:
  // the remake loads them with the variant's trailer and the saves are
  // compared without those slots.
  game::Variant variant = game::Variant::kStandard;
  std::int32_t seed = 1;
  bool keep_saves = false;
  // A save under OA_ORACLE installed as `RESUME.GAM` (the oracle's
  // `--resume`), none when empty.
  std::string resume{};
  // The screenshots compared (`SCRnnn.GIF`), all when empty.
  std::vector<std::string> shots;
  // The races' pending action arguments compared on their low word only:
  // the high word is a stale stack value of the original's user interface
  // frames (`Session::stale_dip_arg`, `docs/09-simulation.md`), for runs
  // where an AI race proposes something to the player.
  bool dip_arg_low_word = false;
};

// Returns `mine` with the high words of its races' pending action arguments
// taken from `theirs` (`ReplayRun::dip_arg_low_word`).
inline std::vector<std::uint8_t> with_their_dip_arg_high_words(const std::vector<std::uint8_t>& mine,
                                                               const std::vector<std::uint8_t>& theirs,
                                                               std::optional<game::RawAddress> foreign_cosmos) {
  game::SaveImage image = game::parse_save(mine, foreign_cosmos);
  const game::SaveImage original = game::parse_save(theirs, foreign_cosmos);
  for (std::size_t r = 0; r < std::min(image.races.size(), original.races.size()); ++r) {
    game::RawAddress& arg = image.races[r].pending_action_arg;
    arg = (original.races[r].pending_action_arg & 0xffff0000u) | (arg & 0xffffu);
  }
  return game::serialize_save(image);
}

// Replays `c` and compares the saves and the screenshots the original
// wrote; with OA_DUMP_DIR the remake's files are copied there. Returns
// false when the run is missing.
inline bool replay_oracle_run(const GameFiles& files, const ReplayRun& c, int max_passes = 400000) {
  const auto trace = oracle_run_path(c.run + "/trace.bin");
  const auto out = oracle_run_path(c.run + "/out");
  const auto resume = c.resume.empty() ? std::nullopt : oracle_run_path(c.resume);
  if (!trace || !out || (!c.resume.empty() && !resume)) {
    return false;
  }
  INFO(c.run);
  ReplayDir dir;
  const bool antag = c.variant != game::Variant::kStandard;
  bool foreign = antag && c.keep_saves;
  if (resume) {
    const std::vector<std::uint8_t> bytes = read_file(*resume);
    foreign = foreign || (antag && game::parse_save(bytes).cosmos_address == game::kPatchCosmosAddress);
    write_file(dir.path / "RESUME.GAM", as_variant_save(bytes, c.variant));
  }
  if (antag && c.keep_saves) {
    for (const char* name : {"01.SAV", "RESUME.GAM"}) {
      if (name != std::string_view("RESUME.GAM") || !resume) {
        write_file(dir.path / name, as_variant_save(read_file(*files.install().find(name)), c.variant));
      }
    }
  }
  ReplayProgram program(files, c.seed, dir.path, c.keep_saves && !antag, c.variant);
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
      std::vector<std::uint8_t> mine = read_file(ours);
      std::vector<std::uint8_t> theirs = read_file(entry.path());
      if (antag) {
        // The remake's variant trailer (`game/save.h`), absent from the
        // original's file.
        REQUIRE(mine.size() == theirs.size() + 8);
        mine.resize(theirs.size());
      }
      if (c.dip_arg_low_word) {
        const std::optional<game::RawAddress> cosmos =
            foreign ? std::optional<game::RawAddress>(game::kPatchCosmosAddress) : std::nullopt;
        mine = with_their_dip_arg_high_words(mine, theirs, cosmos);
      }
      if (foreign) {
        mine = without_stale_slots(mine, game::kPatchCosmosAddress);
        theirs = without_stale_slots(theirs, game::kPatchCosmosAddress);
      }
      CHECK(first_byte_difference(without_stale_save_header(mine), without_stale_save_header(theirs)) == -1);
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

// Replays the oracle runs `runs` (those whose name contains OA_REPLAY_RUN
// when it is set), skipping the test when none is there.
inline void replay_oracle_runs(const GameFiles& files, const std::vector<ReplayRun>& runs) {
  int found = 0;
  const char* only = std::getenv("OA_REPLAY_RUN");
  for (const ReplayRun& run : runs) {
    if (only != nullptr && run.run.find(only) == std::string::npos) {
      continue;
    }
    found += replay_oracle_run(files, run) ? 1 : 0;
  }
  if (found == 0) {
    SKIP("OA_ORACLE is not set to work/oracle with the runs");
  }
}

}  // namespace oa::test
