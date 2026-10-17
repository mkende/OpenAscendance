#include "assets/pcm.h"

#include <format>

#include "core/error.h"

namespace oa {

std::vector<std::uint8_t> sfx_samples(std::span<const std::uint8_t> voc, std::string_view context) {
  constexpr std::size_t kSkip = 0x2a;
  if (voc.size() <= kSkip + 1) {
    throw DataError(std::format("{}: sound effect of {} bytes, shorter than its skipped header", context, voc.size()));
  }
  return {voc.begin() + kSkip, voc.end() - 1};
}

}  // namespace oa
