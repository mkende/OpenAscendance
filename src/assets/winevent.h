// The window-event traces (`WINEVENT.BIN`, the tutorials' `<name>.bin`,
// `tools/winevent.py`, `docs/02-audio-formats.md` "Tutorials"): a 16-byte
// header, then records of 24 bytes, the inputs the window manager replays
// (`WinEvent_load_trace` 0x5510c, `WinEvent_replay` 0x54f90).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace oa {

// Trace record types: the inputs, then the timer and notification requests
// seen while recording (skipped by the replay).
enum class TraceEventType : std::uint16_t {
  kMouseMove = 1,     // a = x, b = y
  kMouseButton = 2,   // a != 0 right button, b != 0 down, msg != 0 Shift held
  kKey = 3,           // a = BIOS scan code, b low word = character, msg = BIOS shift flags
  kRequestTimer = 4,  // EVT_REQ_CONT
  kCancelTimer = 5,   // EVT_CAN_CONT
  kRequestNotify = 6,  // EVT_REQ_CHANGE
  kCancelNotify = 7,   // EVT_CAN_CHANGE
};

struct TraceRecord {
  std::uint32_t time = 0;  // the main-loop pass of the window manager's counter
  std::uint32_t window = 0;
  TraceEventType type = TraceEventType::kMouseMove;
  std::uint16_t msg = 0;
  std::uint32_t a = 0;
  std::uint32_t b = 0;
  std::uint32_t c = 0;
};

struct Trace {
  // The most bytes of records the window manager reads (5000 records).
  static constexpr std::size_t kMaxRecordBytes = 120000;
  static constexpr std::size_t kHeaderSize = 16;
  static constexpr std::size_t kRecordSize = 24;

  std::array<std::uint8_t, kHeaderSize> header{};
  std::vector<TraceRecord> records;
};

// Parses a trace. Throws `DataError` naming `context` when the size is not a
// header plus whole records, there are more than `kMaxRecordBytes` of them,
// or a record's type is unknown (the replay's "Bad Trace Event").
Trace parse_trace(std::span<const std::uint8_t> data, std::string_view context);

}  // namespace oa
