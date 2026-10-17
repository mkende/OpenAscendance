// The shape cache (`cache.cpp`, "SHAPE CACHE", `ShapeCache` at 0x11f0f4): the
// shape tables by file name, loaded on demand. The original's 200 named
// slots, compacting buffer and LRU eviction are memory management the
// remake does not need: tables stay loaded once used.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "assets/cfile.h"
#include "assets/shp.h"

namespace oa {

class ShapeCache {
 public:
  // `files` must outlive the cache.
  explicit ShapeCache(const GameFiles& files) : files_(files) {}

  // `ShapeCache_find_or_add` (0x1b270) then `ShapeCache_get` (0x1b084): the
  // table of `file` (case-insensitive), loaded at the first use. Throws
  // `DataError` when the file is missing or malformed. The reference stays
  // valid as long as the cache.
  const ShapeTable& get(std::string_view file);

 private:
  const GameFiles& files_;
  std::map<std::string, std::unique_ptr<ShapeTable>> tables_;
};

}  // namespace oa
