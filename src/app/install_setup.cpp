#include "app/install_setup.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#include <atomic>
#include <format>
#include <span>
#include <stdexcept>

#include "app/install_source.h"
#include "core/edition.h"
#include "core/log.h"

namespace oa {

namespace {

struct Choice {
  int id;
  const char* text;
};

// Shows a message box with `choices` (the first is the Enter default, the
// last the Esc one); returns the id of the button pressed, -1 when the box is
// closed. Throws `std::runtime_error` when no message box can be shown.
int ask(SDL_MessageBoxFlags kind, const std::string& text, std::span<const Choice> choices) {
  std::vector<SDL_MessageBoxButtonData> buttons;
  for (std::size_t i = 0; i < choices.size(); ++i) {
    SDL_MessageBoxButtonFlags flags = 0;
    if (i == 0) {
      flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    }
    if (i + 1 == choices.size()) {
      flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    }
    buttons.push_back({.flags = flags, .buttonID = choices[i].id, .text = choices[i].text});
  }
  const std::string title = std::format("{}: game files", edition::kName);
  const SDL_MessageBoxData box{.flags = kind | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT,
                               .window = nullptr,
                               .title = title.c_str(),
                               .message = text.c_str(),
                               .numbuttons = static_cast<int>(buttons.size()),
                               .buttons = buttons.data(),
                               .colorScheme = nullptr};
  int pressed = -1;
  if (!SDL_ShowMessageBox(&box, &pressed)) {
    throw std::runtime_error(std::format("{}\n(no message box: {})", text, SDL_GetError()));
  }
  return pressed;
}

// The outcome of SDL's folder dialog, set by its callback (on any thread).
struct FolderPick {
  std::atomic<bool> done = false;
  bool failed = false;
  std::string error;
  std::optional<std::string> folder;
};

void SDLCALL on_folder_picked(void* userdata, const char* const* files, int /*filter*/) {
  auto* pick = static_cast<FolderPick*>(userdata);
  if (files == nullptr) {
    pick->failed = true;
    pick->error = SDL_GetError();
  } else if (files[0] != nullptr) {
    pick->folder = files[0];
  }
  pick->done.store(true, std::memory_order_release);
}

// Shows the system folder dialog and waits for it; returns the folder chosen,
// nothing when cancelled. Throws `std::runtime_error` when no dialog is
// available.
std::optional<std::string> pick_folder(const std::string& start_folder) {
  if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) {
    throw std::runtime_error(std::format("SDL_InitSubSystem(EVENTS): {}", SDL_GetError()));
  }
  FolderPick pick;
  SDL_ShowOpenFolderDialog(on_folder_picked, &pick, nullptr, start_folder.empty() ? nullptr : start_folder.c_str(),
                           false);
  // The dialog runs on a thread of its own or answers through events.
  while (!pick.done.load(std::memory_order_acquire)) {
    SDL_PumpEvents();
    SDL_Delay(10);
  }
  SDL_QuitSubSystem(SDL_INIT_EVENTS);
  if (pick.failed) {
    throw std::runtime_error(std::format("no folder dialog is available: {}", pick.error));
  }
  return pick.folder;
}

}  // namespace

std::optional<Install> run_install_setup(const std::vector<std::string>& problems,
                                         const std::filesystem::path& config_file) {
  std::string lead;
  for (const std::string& p : problems) {
    lead += p + "\n";
  }
  if (!lead.empty()) {
    lead += "\n";
  }
  constexpr Choice kChoose[] = {{1, "Choose folder..."}, {0, "Quit"}};
  if (ask(SDL_MESSAGEBOX_INFORMATION,
          lead + edition::kName +
              " needs the files of the original game, Ascendancy (The Logic Factory, 1995): the folder where it "
              "is installed, which holds COB.CFG and the ASCEND00..02.COB archives.\n\nChoose that folder?",
          kChoose) != 1) {
    return std::nullopt;
  }
  std::string start;
  for (;;) {
    std::optional<std::string> folder;
    try {
      folder = pick_folder(start);
    } catch (const std::runtime_error& e) {
      log_error("{}", e.what());
      constexpr Choice kOk[] = {{0, "OK"}};
      ask(SDL_MESSAGEBOX_ERROR,
          std::format("{}.\n\nStart the game with --data <the folder of the original install>.", e.what()), kOk);
      return std::nullopt;
    }
    if (!folder) {
      return std::nullopt;
    }
    InstallCheck check = check_install_folder(*folder);
    if (check.install) {
      save_install_config(config_file, *folder);
      log_info("game files in {} (remembered in {})", *folder, config_file.string());
      return std::move(check.install);
    }
    constexpr Choice kAgain[] = {{1, "Choose another folder..."}, {0, "Quit"}};
    if (ask(SDL_MESSAGEBOX_WARNING,
            std::format("{} is not an Ascendancy install: {}.\n\nChoose the folder holding COB.CFG.", *folder,
                        check.problem),
            kAgain) != 1) {
      return std::nullopt;
    }
    start = *folder;
  }
}

std::filesystem::path user_dir() {
  char* pref = SDL_GetPrefPath(edition::kPrefOrg, edition::kPrefApp);
  if (pref == nullptr) {
    throw std::runtime_error(std::format("no preference directory: {}", SDL_GetError()));
  }
  std::filesystem::path dir(pref);
  SDL_free(pref);
  return dir;
}

}  // namespace oa
