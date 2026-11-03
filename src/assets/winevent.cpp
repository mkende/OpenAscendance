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

namespace {

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  put_u16(out, static_cast<std::uint16_t>(v));
  put_u16(out, static_cast<std::uint16_t>(v >> 16));
}

}  // namespace

std::vector<std::uint8_t> serialize_trace(const Trace& trace) {
  std::vector<std::uint8_t> out(trace.header.begin(), trace.header.end());
  for (const TraceRecord& r : trace.records) {
    put_u32(out, r.time);
    put_u32(out, r.window);
    put_u16(out, static_cast<std::uint16_t>(r.type));
    put_u16(out, r.msg);
    put_u32(out, r.a);
    put_u32(out, r.b);
    put_u32(out, r.c);
  }
  return out;
}

std::string trace_text(const Trace& trace) {
  std::string out;
  // The C `%d` of the original's 32-bit and 16-bit fields.
  const auto i32 = [](std::uint32_t v) { return static_cast<std::int32_t>(v); };
  const auto i16 = [](std::uint16_t v) { return static_cast<std::int16_t>(v); };
  for (const TraceRecord& r : trace.records) {
    switch (r.type) {
      case TraceEventType::kMouseMove:
        out += std::format(" TIME[{:05d}]  EVT_MOUSE_POS    :  Moved to ({:03d},{:03d})\n", r.time, i32(r.a), i32(r.b));
        break;
      case TraceEventType::kMouseButton:
        out += std::format(" TIME[{:05d}]  EVT_MOUSE_BUTTON :  {} Button {}\n", r.time, r.a != 0 ? "Right" : "Left",
                           r.b != 0 ? "DOWN" : "UP");
        break;
      case TraceEventType::kKey:
        out += std::format(" TIME[{:05d}]  EVT_KEYBOARD     :  '{:c}'  ({:04d})\n", r.time,
                           static_cast<char>(r.b & 0xff), i32(r.a));
        break;
      case TraceEventType::kRequestTimer:
      case TraceEventType::kCancelTimer:
        out += std::format(" TIME[{:05d}]  {:<15}:  HWND[{:04d}] MSG[{:04d}] P1[{:04d}] P2[{:04d}]\n", r.time,
                           r.type == TraceEventType::kRequestTimer ? "EVT_REQ_CONT" : "EVT_CAN_CONT", i32(r.window),
                           i16(r.msg), i32(r.a), i32(r.b));
        break;
      case TraceEventType::kRequestNotify:
      case TraceEventType::kCancelNotify:
        out += std::format(" TIME[{:05d}]  {:<15}:  HWND[{:04d}] MSG[{:04d}] P1[{:04d}] P2[{:04d}] F[{:04d}]\n", r.time,
                           r.type == TraceEventType::kRequestNotify ? "EVT_REQ_CHANGE" : "EVT_CAN_CHANGE",
                           i32(r.window), i16(r.msg), i32(r.a), i32(r.b), i32(r.c));
        break;
    }
  }
  return out;
}

}  // namespace oa
