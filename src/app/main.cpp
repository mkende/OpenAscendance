// Open Ascendance: entry point. The command line is described in
// app/command_line.h.

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_messagebox.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "app/command_line.h"
#include "app/corn.h"
#include "app/install_setup.h"
#include "app/install_source.h"
#include "core/edition.h"
#include "core/error.h"
#include "core/log.h"

namespace {

// Returns the install of `--data`, else the one remembered by the install
// setup, else the setup's (with `--setup`, always the setup's); nothing when
// the player quits the setup. A headless run shows no dialog: its `--data`
// must be an install.
std::optional<oa::Install> desktop_install(const oa::CommandLine& options) {
  std::optional<std::filesystem::path> data_dir;
  if (options.data_dir) {
    data_dir = *options.data_dir;
  }
  if (options.screenshot) {
    if (!data_dir) {
      throw std::runtime_error("a screenshot run needs --data <install dir>");
    }
    oa::StartupInstall start = oa::choose_startup_install(data_dir, false, std::nullopt);
    if (!start.install) {
      throw std::runtime_error(start.problems.front());
    }
    return std::move(start.install);
  }
  const std::filesystem::path config = oa::user_dir() / oa::kInstallConfigName;
  std::optional<std::filesystem::path> remembered;
  try {
    remembered = oa::load_install_config(config);
  } catch (const oa::DataError& e) {
    oa::log_warning("{}", e.what());
  }
  oa::StartupInstall start = oa::choose_startup_install(data_dir, options.setup, remembered);
  if (start.install) {
    return std::move(start.install);
  }
  for (const std::string& p : start.problems) {
    oa::log_warning("{}", p);
  }
  return oa::run_install_setup(start.problems, config);
}

}  // namespace

int main(int argc, char** argv) {
  SDL_SetAppMetadata(oa::edition::kName, oa::edition::kVersion, oa::edition::kAppId);
  try {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const oa::CommandLine options = oa::parse_command_line(args);
    const std::optional<oa::Install> install = desktop_install(options);
    if (!install) {
      return 0;
    }
    oa::GameOptions game{.scaling = options.scaling, .keys = options.keys, .screenshot = {}, .frames = options.frames};
    if (options.screenshot) {
      game.screenshot = *options.screenshot;
    }
    return oa::run_game(game, *install);
  } catch (const std::exception& e) {
    oa::log_error("{}", e.what());
    return 1;
  }
}
