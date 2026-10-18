// The install setup: when the game files are not available, native dialogs
// (the game's own fonts and art being part of what is missing) ask the player
// for the folder of the original install, check it and remember it
// (app/install_source.h). Adapted from SilentHeritage's app/install_setup.h.
// On Android, the folder chosen is copied into the app's storage
// (app/android_documents.h) and the copy is the install remembered.
#pragma once

#include <SDL3/SDL_platform_defines.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/install.h"

namespace oa {

// Runs the setup: a message explaining what is needed (after `problems`,
// what was wrong with the previous sources), the folder picker, and a new
// choice while the folder is not an install. Writes the choice to
// `config_file`. Returns the install chosen, nothing when the player quits or
// no picker is available (said in a message). Throws `std::runtime_error`
// when the choice cannot be remembered.
std::optional<Install> run_install_setup(const std::vector<std::string>& problems,
                                         const std::filesystem::path& config_file);

#ifdef SDL_PLATFORM_ANDROID
// Returns the folder of the app's storage holding the copy of the game files
// (of the setup, or of the package's bundle: app/install_bundle.h); throws
// `std::runtime_error` when there is no internal storage.
std::filesystem::path android_install_dir();
#endif

// Returns the player's SDL preference directory (which holds INSTALL.CFG);
// throws `std::runtime_error` when SDL has none.
std::filesystem::path user_dir();

}  // namespace oa
