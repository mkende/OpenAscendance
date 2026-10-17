// Diagnostic messages of the program: one line each, prefixed by the
// executable's name, to standard error unless a sink is set.
#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace oa {

enum class LogLevel : std::uint8_t { kDebug, kInfo, kWarning, kError };

// Receives each message (without line end).
using LogSink = std::function<void(LogLevel, std::string_view)>;

// Sends the messages to `sink` from now on; an empty sink restores standard
// error. Not thread-safe against concurrent `log` calls.
void set_log_sink(LogSink sink);

// Sets the lowest level written (kInfo by default).
void set_log_level(LogLevel level);

// Writes `message` at `level`.
void log(LogLevel level, std::string_view message);

template <typename... Args>
void log_info(std::format_string<Args...> fmt, Args&&... args) {
  log(LogLevel::kInfo, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_warning(std::format_string<Args...> fmt, Args&&... args) {
  log(LogLevel::kWarning, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_error(std::format_string<Args...> fmt, Args&&... args) {
  log(LogLevel::kError, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void log_debug(std::format_string<Args...> fmt, Args&&... args) {
  log(LogLevel::kDebug, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace oa
