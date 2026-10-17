// Open Ascendance: entry point. The command line is described in
// app/command_line.h.

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
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

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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

// Windows: the executable is a windowed program (no console window of its
// own); started from a console, its output and messages go to that console.
void attach_parent_console() {
#ifdef _WIN32
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    std::FILE* ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
  }
#endif
}

// Android: standard error goes nowhere; the messages go to the system log
// (logcat) through SDL's.
void route_log_to_sdl() {
#ifdef SDL_PLATFORM_ANDROID
  oa::set_log_sink([](oa::LogLevel level, std::string_view message) {
    SDL_LogPriority priority = SDL_LOG_PRIORITY_INFO;
    switch (level) {
      case oa::LogLevel::kDebug:
        priority = SDL_LOG_PRIORITY_DEBUG;
        break;
      case oa::LogLevel::kInfo:
        priority = SDL_LOG_PRIORITY_INFO;
        break;
      case oa::LogLevel::kWarning:
        priority = SDL_LOG_PRIORITY_WARN;
        break;
      case oa::LogLevel::kError:
        priority = SDL_LOG_PRIORITY_ERROR;
        break;
    }
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "%.*s", static_cast<int>(message.size()), message.data());
  });
#endif
}

}  // namespace

int main(int argc, char** argv) {
  attach_parent_console();
  route_log_to_sdl();
  SDL_SetAppMetadata(oa::edition::kName, oa::edition::kVersion, oa::edition::kAppId);
  try {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const oa::CommandLine options = oa::parse_command_line(args);
    const std::optional<oa::Install> install = desktop_install(options);
    if (!install) {
      return 0;
    }
    oa::GameOptions game{.scaling = options.scaling, .keys = options.keys, .screenshot = {}, .frames = options.frames, .user_dir = {}};
    if (options.screenshot) {
      game.screenshot = *options.screenshot;
    } else {
      game.user_dir = oa::user_dir();
    }
    return oa::run_game(game, *install);
  } catch (const std::exception& e) {
    oa::log_error("{}", e.what());
#if defined(_WIN32) || defined(SDL_PLATFORM_ANDROID)
    // Started from the Explorer or the launcher, there is no console to tell it.
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, oa::edition::kName, e.what(), nullptr);
#endif
    return 1;
  }
}
