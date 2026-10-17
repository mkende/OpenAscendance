#include "ui/shape_cache.h"

#include <cctype>

namespace oa {

const ShapeTable& ShapeCache::get(std::string_view file) {
  std::string key(file);
  for (char& c : key) {
    c = c == '/' ? '\\' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  std::unique_ptr<ShapeTable>& table = tables_[key];
  if (!table) {
    table = std::make_unique<ShapeTable>(files_.load(file), file);
  }
  return *table;
}

}  // namespace oa
