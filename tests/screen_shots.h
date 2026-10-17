// Screenshots of the screens' tests, for the comparisons with the original's
// (`tools/compare_shot.py`): written as `$OA_SHOTS/<name>.bmp` when the
// OA_SHOTS environment variable names a folder, nothing otherwise.
#pragma once

#include <cstdlib>
#include <filesystem>
#include <string_view>

#include "core/image.h"
#include "platform/screenshot.h"
#include "ui_rig.h"

namespace oa::test {

// Writes the rig's screen with its palette as `<OA_SHOTS>/<name>.bmp`.
inline void write_shot(UiRig& rig, std::string_view name) {
  const char* dir = std::getenv("OA_SHOTS");
  if (dir == nullptr || *dir == '\0') {
    return;
  }
  const std::filesystem::path path = std::filesystem::path(dir) / (std::string(name) + ".bmp");
  save_bmp(path, rig.wm.screen(), Palette::from_vga(rig.gs.shadow()));
}

}  // namespace oa::test
