// The C `sprintf` of the original's texts: their formats come from the game
// files (`static.txt`, the help topics), so they are applied at run time.
#pragma once

#include <algorithm>
#include <cstdio>
#include <string>

namespace oa {

// Returns `format` (a C format of the game's texts: `%s` with C strings,
// `%d` with ints) applied to `args`, cut at 1023 characters (the original's
// buffers are smaller).
template <typename... Args>
std::string c_format(const std::string& format, Args... args) {
  std::string buffer(1024, '\0');
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
  const int n = std::snprintf(buffer.data(), buffer.size(), format.c_str(), args...);
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  buffer.resize(n < 0 ? 0 : std::min<std::size_t>(static_cast<std::size_t>(n), buffer.size() - 1));
  return buffer;
}

}  // namespace oa
