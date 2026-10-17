// Helpers of the game-core tests: the install's tables (OA_DATA) and the
// original's reference runs (OA_ORACLE = `work/oracle`, `docs/12-oracle.md`).
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/install.h"
#include "game/tables.h"
#include "game_data.h"

namespace oa::game::oracle_test {

// Returns the tables of the OA_DATA install, nothing when it is unset.
inline const GameTables* install_tables() {
  static const std::optional<GameTables> tables = []() -> std::optional<GameTables> {
    const auto install = ::oa::test::game_install();
    if (!install) {
      return std::nullopt;
    }
    return load_game_tables(GameFiles(*install));
  }();
  return tables ? &*tables : nullptr;
}

// Returns the OA_ORACLE directory, nothing when it is unset.
inline std::optional<std::filesystem::path> oracle_dir() {
  const char* dir = std::getenv("OA_ORACLE");
  if (dir == nullptr || *dir == '\0') {
    return std::nullopt;
  }
  return std::filesystem::path(dir);
}

// Returns the file `relative` of the OA_ORACLE directory, nothing when
// OA_ORACLE is unset or the file is missing.
inline std::optional<std::vector<std::uint8_t>> oracle_file(const std::string& relative) {
  const auto dir = oracle_dir();
  if (!dir || !std::filesystem::exists(*dir / relative)) {
    return std::nullopt;
  }
  return read_file(*dir / relative);
}

// Returns the first offset where `a` and `b` differ, or -1.
inline std::ptrdiff_t first_difference(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    if (a[i] != b[i]) {
      return static_cast<std::ptrdiff_t>(i);
    }
  }
  return a.size() == b.size() ? -1 : static_cast<std::ptrdiff_t>(std::min(a.size(), b.size()));
}

}  // namespace oa::game::oracle_test

#define OA_REQUIRE_TABLES(tables)                                                   \
  const ::oa::game::GameTables* tables = ::oa::game::oracle_test::install_tables(); \
  if (tables == nullptr) {                                                          \
    SKIP("OA_DATA is not set to an Ascendancy install");                            \
  }
