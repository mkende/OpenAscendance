// Where the game files come from when `--data` does not give them: the
// folder chosen by the player at the install setup (app/install_setup.h),
// remembered in INSTALL.CFG in the player's SDL preference directory, and the
// start-up decision between it, `--data` and a new setup. Free of dialogs,
// for the tests. Adapted from SilentHeritage's app/install_source.h (host
// folders only for now).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/install.h"

namespace oa {

// The name of the configuration file in the preference directory.
inline constexpr std::string_view kInstallConfigName = "INSTALL.CFG";

// Returns the folder saved in `file`, nothing if there is no such file. The
// file is text:
//
//   OA-INSTALL 1
//   folder <path>
//
// Throws `DataError` when it cannot be read or is malformed.
std::optional<std::filesystem::path> load_install_config(const std::filesystem::path& file);

// Writes `folder` to `file`, creating its directory; throws
// `std::runtime_error` on failure.
void save_install_config(const std::filesystem::path& file, const std::filesystem::path& folder);

// The outcome of opening a candidate install: the install, or why it is not
// one.
struct InstallCheck {
  std::optional<Install> install;
  std::string problem;  // set when `install` is empty
};

// Opens host folder `dir` as an install: `COB.CFG` and the archives it lists
// must be there.
InstallCheck check_install_folder(const std::filesystem::path& dir);

// The install to start with, or the reasons to run the setup.
struct StartupInstall {
  std::optional<Install> install;  // nothing: run the setup
  // What was wrong with `--data` and the remembered folder.
  std::vector<std::string> problems;
};

// Decides where the game files come from: with `force_setup` the setup runs;
// otherwise `data_dir` (`--data`) when it is a valid install, then
// `remembered` when it is valid; else the setup runs with the problems found.
StartupInstall choose_startup_install(const std::optional<std::filesystem::path>& data_dir, bool force_setup,
                                      const std::optional<std::filesystem::path>& remembered);

}  // namespace oa
