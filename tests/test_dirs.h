// Folders of the tests: a temporary folder of their own, and the files of
// the original's reference runs (OA_ORACLE = `work/oracle`,
// `docs/12-oracle.md`).
#pragma once

#include <cstdlib>
#include <filesystem>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace oa::test {

// A new folder under the system's temporary folder, removed with the
// object.
struct TempDir {
  explicit TempDir(std::string_view prefix) {
    std::random_device rd;
    path = std::filesystem::temp_directory_path() / std::format("{}_{:08x}", prefix, rd());
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  std::filesystem::path path;
};

// Returns the path of the file `relative` of OA_ORACLE, nothing when
// OA_ORACLE is unset or the file is missing.
inline std::optional<std::filesystem::path> oracle_path(const std::string& relative) {
  const char* dir = std::getenv("OA_ORACLE");
  if (dir == nullptr || *dir == '\0') {
    return std::nullopt;
  }
  std::filesystem::path path = std::filesystem::path(dir) / relative;
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  return path;
}

}  // namespace oa::test
