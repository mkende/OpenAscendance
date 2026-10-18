// OpenAscendance: entry point. The command line is described in
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

#ifdef SDL_PLATFORM_ANDROID
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <format>
#include <fstream>
#include <memory>

#include "app/install_bundle.h"
#endif

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

#ifdef SDL_PLATFORM_ANDROID

// Copies asset `path` of the bundle to host file `to` (an `oa::BundleCopier`).
void copy_bundle_asset(const std::string& path, const std::filesystem::path& to) {
  const std::string asset = std::format("{}/{}", oa::kBundleDir, path);
  const std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> in(SDL_IOFromFile(asset.c_str(), "rb"), SDL_CloseIO);
  if (!in) {
    throw oa::DataError(std::format("cannot open the asset {}: {}", asset, SDL_GetError()));
  }
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  std::vector<char> buffer(std::size_t{1} << 20);
  for (;;) {
    const std::size_t n = SDL_ReadIO(in.get(), buffer.data(), buffer.size());
    if (n == 0) {
      break;
    }
    out.write(buffer.data(), static_cast<std::streamsize>(n));
  }
  if (SDL_GetIOStatus(in.get()) == SDL_IO_STATUS_ERROR) {
    throw oa::DataError(std::format("cannot read the asset {}: {}", asset, SDL_GetError()));
  }
  if (!out) {
    throw std::runtime_error(std::format("cannot write {}", to.string()));
  }
}

// Returns the install embedded in the package (app/install_bundle.h), copied
// into the app's storage on the first start and remembered as the install
// setup's would be, so that OpenAscendance installed over this package finds
// it; nothing when the package embeds none (the OpenAscendance edition: the
// install setup provides the files).
std::optional<oa::Install> bundled_install() {
  std::size_t size = 0;
  void* data = SDL_LoadFile(std::string(oa::kBundleIndexName).c_str(), &size);
  if (data == nullptr) {
    return std::nullopt;
  }
  const std::string index(static_cast<const char*>(data), size);
  SDL_free(data);
  const std::filesystem::path dir = oa::android_install_dir();
  if (oa::update_bundle_copy(index, copy_bundle_asset, dir)) {
    oa::log_info("game files copied from the package to {}", dir.string());
  }
  oa::InstallCheck check = oa::check_install_folder(dir);
  if (!check.install) {
    throw oa::DataError(check.problem);
  }
  oa::save_install_config(oa::user_dir() / oa::kInstallConfigName, dir);
  return std::move(check.install);
}

#endif

// Returns the install the game runs from: on Android, the one embedded in
// the package if any; else `desktop_install`'s.
std::optional<oa::Install> startup_install(const oa::CommandLine& options) {
#ifdef SDL_PLATFORM_ANDROID
  if (std::optional<oa::Install> bundled = bundled_install()) {
    return bundled;
  }
#endif
  return desktop_install(options);
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
    const std::optional<oa::Install> install = startup_install(options);
    if (!install) {
      return 0;
    }
    oa::GameOptions game{.settings = {},
                         .settings_file = {},
                         .keys = options.keys,
                         .screenshot = {},
                         .frames = options.frames,
                         .user_dir = {},
                         .seed = options.seed,
                         .trace = {}};
    if (options.trace) {
      game.trace = *options.trace;
    }
    if (options.screenshot) {
      game.screenshot = *options.screenshot;
      if (options.save_dir) {
        game.user_dir = *options.save_dir;
      }
    } else {
      game.user_dir = oa::user_dir();
    }
    if (!game.user_dir.empty()) {
      game.settings_file = game.user_dir / oa::kOptionsFile;
      game.settings = oa::load_options(game.settings_file);
    }
    game.settings.scaling = options.scaling.value_or(game.settings.scaling);
    game.settings.fullscreen = options.fullscreen.value_or(game.settings.fullscreen);
    game.settings.variant = options.variant.value_or(game.settings.variant);
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
