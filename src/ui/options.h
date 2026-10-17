// The remake's own settings, set on its options screen
// (`screens/options_wnd.h`) and kept in `options.cfg` in the user folder,
// apart from the original's `ascend.cfg` (the mute flags and the rotation
// speed, kept byte-compatible by `WinMgr`).
//
// `options.cfg` is text, one `key=value` per line (`#` starts a comment):
//
//     variant=standard        # or antagonizer: the variant of new games
//     fullscreen=0            # or 1
//     scaling=smooth          # or sharp
//
// Unknown keys and bad values are ignored with a warning, so that a file of
// a later version stays readable.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "game/variant.h"
#include "platform/display.h"

namespace oa {

inline constexpr std::string_view kOptionsFile = "options.cfg";

struct RemakeOptions {
  game::Variant variant = game::Variant::kStandard;
  bool fullscreen = false;
  Scaling scaling = Scaling::kSmooth;

  bool operator==(const RemakeOptions&) const = default;
};

// Returns the options of the `options.cfg` text `text`, the defaults for
// the keys it lacks.
RemakeOptions parse_options(std::string_view text);
// Returns the `options.cfg` text of `options` (every key).
std::string format_options(const RemakeOptions& options);
// Returns the options of the file `path`, the defaults when it does not
// exist or cannot be read.
RemakeOptions load_options(const std::filesystem::path& path);
// Writes `options` to `path`; returns false (with a warning) when it cannot
// be written.
bool save_options(const std::filesystem::path& path, const RemakeOptions& options);

// The window settings the options screen changes (the SDL display of the
// program; the tests' fake).
class DisplayControl {
 public:
  virtual ~DisplayControl() = default;
  virtual bool fullscreen() const = 0;
  virtual void set_fullscreen(bool fullscreen) = 0;
  virtual Scaling scaling() const = 0;
  virtual void set_scaling(Scaling scaling) = 0;
};

}  // namespace oa
