#include "assets/pal.h"

#include <algorithm>
#include <format>

#include "core/error.h"

namespace oa {

int load_palette(VgaPalette& palette, std::span<const std::uint8_t> data, int first, int count,
                 std::string_view context) {
  if (data.empty() || data.size() % 3 != 0 || data.size() > palette.dac.size()) {
    throw DataError(std::format("{}: bad palette size {}", context, data.size()));
  }
  if (std::ranges::any_of(data, [](std::uint8_t v) { return v > 63; })) {
    throw DataError(std::format("{}: palette value above 63 (not 6-bit VGA)", context));
  }
  const int colors = static_cast<int>(data.size() / 3);
  const int n = count < 0 ? colors : std::min(count, colors);
  if (first < 0 || first + n > 256) {
    throw DataError(std::format("{}: {} colours do not fit from index {}", context, n, first));
  }
  std::copy_n(data.begin(), n * 3, palette.dac.begin() + first * 3);
  return n;
}

HazeTable parse_haze_table(std::span<const std::uint8_t> data, std::string_view context) {
  HazeTable table{};
  if (data.size() != table.size()) {
    throw DataError(std::format("{}: haze table is {} bytes, expected 256", context, data.size()));
  }
  std::ranges::copy(data, table.begin());
  return table;
}

}  // namespace oa
