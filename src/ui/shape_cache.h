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
#include <vector>

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

  // A loaded table: its file (upper case) and size in bytes.
  struct Entry {
    std::string file;
    std::size_t bytes = 0;
  };
  // The loaded tables, by file name (the developer's debug window).
  std::vector<Entry> entries() const;

 private:
  const GameFiles& files_;
  std::map<std::string, std::unique_ptr<ShapeTable>> tables_;
};

}  // namespace oa
