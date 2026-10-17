// Access to the player's original Ascendancy install (adapted from
// SilentHeritage's core/install.h, host folders only for now).
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace oa {

// The original game install: the folder holding `COB.CFG`, the `.COB`
// archives it lists and `LOGIC.FLC`. Paths are given in the game's DOS
// spelling (`DATA\LOGO.GIF`, '\\' or '/' separators, empty and "." components
// ignored) and matched case-insensitively, since installs copied from the CD
// may be lower- or mixed-case. Immutable and cheap to copy.
class Install {
 public:
  // The file whose presence makes a folder an install.
  static constexpr std::string_view kMarkerFile = "COB.CFG";

  // The install in host directory `root`. Throws `DataError` if `root` has no
  // `COB.CFG`.
  explicit Install(std::filesystem::path root);

  const std::filesystem::path& root() const { return root_; }

  // Returns whether `dos_path` names a file or a directory of the install.
  bool exists(std::string_view dos_path) const;

  // Returns the whole content of file `dos_path`, nothing if it is missing;
  // throws `DataError` if it exists but cannot be read.
  std::optional<std::vector<std::uint8_t>> try_read(std::string_view dos_path) const;

  // Returns the whole content of file `dos_path`; throws `DataError` if it
  // is missing or cannot be read.
  std::vector<std::uint8_t> read(std::string_view dos_path) const;

  // Returns the host path of `dos_path`, nothing if it does not exist.
  std::optional<std::filesystem::path> find(std::string_view dos_path) const;

 private:
  std::filesystem::path root_;
};

// Reads a whole file from the host file system; throws `DataError` on failure.
std::vector<std::uint8_t> read_file(const std::filesystem::path& path);

}  // namespace oa
