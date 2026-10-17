#include "assets/flc.h"

#include <format>

#include "core/byte_reader.h"

namespace oa {

namespace {

constexpr std::uint16_t kFlcMagic = 0xaf12;
constexpr std::uint16_t kFrameMagic = 0xf1fa;
constexpr std::size_t kHeaderSize = 128;

enum ChunkType : std::uint16_t {
  kColor256 = 4,
  kDeltaFlc = 7,
  kByteRun = 15,
  kCopy = 16,
  kPostageStamp = 18,
};

void put(Image8& screen, int x, int y, std::uint8_t v) {
  if (screen.contains(x, y)) {
    screen.set(x, y, v);
  }
}

void decode_color256(ByteReader r, const FlcFile::ColorSink& colors, int protect_first, int protect_count) {
  const int packets = r.u16();
  int idx = 0;
  for (int p = 0; p < packets; ++p) {
    idx += r.u8();
    const int count = r.u8();
    const int n = count == 0 ? 256 : count;
    for (int i = 0; i < n; ++i) {
      if (i >= protect_first && i < protect_first + protect_count) {
        continue;  // neither written nor consumed (the original's quirk)
      }
      const std::uint8_t red = r.u8();
      const std::uint8_t green = r.u8();
      const std::uint8_t blue = r.u8();
      colors(idx, red, green, blue);
      ++idx;
    }
  }
}

void decode_brun(ByteReader r, Image8& screen, int x0, int y0, int w, int h) {
  for (int y = 0; y < h; ++y) {
    r.u8();  // the obsolete packet count
    int x = 0;
    while (x < w) {
      const auto count = static_cast<std::int8_t>(r.u8());
      if (count < 0) {
        for (int i = 0; i < -count; ++i, ++x) {
          put(screen, x0 + x, y0 + y, r.u8());
        }
      } else {
        const std::uint8_t v = r.u8();
        for (int i = 0; i < count; ++i, ++x) {
          put(screen, x0 + x, y0 + y, v);
        }
      }
    }
  }
}

void decode_copy(ByteReader r, Image8& screen, int x0, int y0, int w, int h) {
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      put(screen, x0 + x, y0 + y, r.u8());
    }
  }
}

void decode_delta_flc(ByteReader r, Image8& screen, int x0, int y0) {
  int lines = r.u16();
  int y = y0;
  while (lines > 0) {
    const auto word = static_cast<std::int16_t>(r.u16());
    if (word <= 0) {
      if ((word & 0x4000) != 0) {
        y -= word;
      }
      continue;
    }
    int x = x0;
    for (int p = 0; p < word; ++p) {
      x += r.u8();
      const auto count = static_cast<std::int8_t>(r.u8());
      if (count >= 0) {
        for (int i = 0; i < 2 * count; ++i, ++x) {
          put(screen, x, y, r.u8());
        }
      } else {
        const std::uint8_t a = r.u8();
        const std::uint8_t b = r.u8();
        for (int i = 0; i < -count; ++i, x += 2) {
          put(screen, x, y, a);
          put(screen, x + 1, y, b);
        }
      }
    }
    ++y;
    --lines;
  }
}

}  // namespace

FlcFile::FlcFile(std::vector<std::uint8_t> data, std::string_view context) : data_(std::move(data)), context_(context) {
  ByteReader r(data_, context_);
  header_.size = r.u32();
  if (r.u16() != kFlcMagic) {
    r.fail("not an FLC file");
  }
  header_.frames = r.u16();
  header_.width = r.u16();
  header_.height = r.u16();
  header_.depth = r.u16();
  r.u16();  // flags
  header_.speed_ms = r.u32();
  r.seek(0x50);
  std::size_t pos = r.u32();  // oframe1
  if (pos < kHeaderSize) {
    pos = kHeaderSize;
  }
  for (int f = 0; f <= header_.frames; ++f) {
    r.seek(pos);
    const std::uint32_t size = r.u32();
    if (r.u16() != kFrameMagic) {
      r.fail(std::format("frame {} has no frame magic", f));
    }
    if (size < 16 || pos + size > data_.size()) {
      r.fail(std::format("frame {} overruns the file", f));
    }
    frame_offsets_.push_back(pos);
    pos += size;
  }
}

FlcFrameChunks FlcFile::decode_frame(int n, Image8& screen, int x, int y, const ColorSink& colors, int protect_first,
                                     int protect_count) const {
  FlcFrameChunks out;
  ByteReader r(data_, context_);
  r.seek(frame_offsets_.at(static_cast<std::size_t>(n)));
  const std::uint32_t frame_size = r.u32();
  r.u16();
  const int chunks = r.u16();
  r.skip(8);
  ByteReader frame = r.sub(frame_size - 16);
  for (int c = 0; c < chunks; ++c) {
    const std::uint32_t size = frame.u32();
    const std::uint16_t type = frame.u16();
    if (size < 6) {
      frame.fail(std::format("chunk of {} bytes", size));
    }
    const ByteReader body = frame.sub(size - 6);
    switch (type) {
      case kColor256:
        out.color256 = true;
        decode_color256(body, colors, protect_first, protect_count);
        break;
      case kDeltaFlc:
        decode_delta_flc(body, screen, x, y);
        break;
      case kByteRun:
        out.brun = true;
        decode_brun(body, screen, x, y, header_.width, header_.height);
        break;
      case kCopy:
        out.copy = true;
        decode_copy(body, screen, x, y, header_.width, header_.height);
        break;
      case kPostageStamp:
        break;
      default:
        frame.fail(std::format("unsupported FLC chunk type {}", type));
    }
  }
  return out;
}

}  // namespace oa
