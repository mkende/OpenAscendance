#include "core/log.h"

#include <cstdio>

#include "core/edition.h"

namespace oa {

namespace {

LogSink g_sink;
LogLevel g_level = LogLevel::kInfo;

std::string_view level_name(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "debug";
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kWarning:
      return "warning";
    case LogLevel::kError:
      return "error";
  }
  return "?";
}

}  // namespace

void set_log_sink(LogSink sink) { g_sink = std::move(sink); }

void set_log_level(LogLevel level) { g_level = level; }

void log(LogLevel level, std::string_view message) {
  if (level < g_level) {
    return;
  }
  if (g_sink) {
    g_sink(level, message);
    return;
  }
  const std::string_view name = level_name(level);
  std::fprintf(stderr, "%s: %.*s: %.*s\n", edition::kExecutable, static_cast<int>(name.size()), name.data(),
               static_cast<int>(message.size()), message.data());
}

}  // namespace oa
