#include "assets/winevent.h"

#include <algorithm>
#include <format>

#include "core/byte_reader.h"
#include "core/error.h"

namespace oa {

Trace parse_trace(std::span<const std::uint8_t> data, std::string_view context) {
  if (data.size() < Trace::kHeaderSize || (data.size() - Trace::kHeaderSize) % Trace::kRecordSize != 0 ||
      data.size() - Trace::kHeaderSize > Trace::kMaxRecordBytes) {
    throw DataError(std::format("{}: {} bytes is not a window-event trace", context, data.size()));
  }
  ByteReader in(data, context);
  Trace trace;
  const std::span<const std::uint8_t> header = in.bytes(Trace::kHeaderSize);
  std::copy(header.begin(), header.end(), trace.header.begin());
  while (!in.at_end()) {
    TraceRecord r;
    r.time = in.u32();
    r.window = in.u32();
    const std::uint16_t type = in.u16();
    if (type < static_cast<std::uint16_t>(TraceEventType::kMouseMove) ||
        type > static_cast<std::uint16_t>(TraceEventType::kCancelNotify)) {
      in.fail(std::format("bad trace event type {}", type));
    }
    r.type = static_cast<TraceEventType>(type);
    r.msg = in.u16();
    r.a = in.u32();
    r.b = in.u32();
    r.c = in.u32();
    trace.records.push_back(r);
  }
  return trace;
}

}  // namespace oa
