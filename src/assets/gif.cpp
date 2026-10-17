#include "assets/gif.h"

#include <array>
#include <format>
#include <string>
#include <vector>

#include "core/byte_reader.h"
#include "core/error.h"

namespace oa {

namespace {

constexpr int kMaxCodes = 4096;

// Decodes the GIF LZW stream `data` (sub-blocks joined) into `out`, stopping
// at the end code, the end of the data or once `out` is full.
void lzw_decode(std::span<const std::uint8_t> data, int min_code_size, std::span<std::uint8_t> out,
                std::string_view context) {
  if (min_code_size < 2 || min_code_size > 8) {
    throw DataError(std::format("{}: bad LZW code size {}", context, min_code_size));
  }
  const int clear = 1 << min_code_size;
  const int end = clear + 1;
  // Each code is its prefix code plus a last byte; `first` is the first byte
  // of its string, `length` the string's length.
  std::array<std::uint16_t, kMaxCodes> prefix{};
  std::array<std::uint8_t, kMaxCodes> suffix{};
  std::array<std::uint8_t, kMaxCodes> first{};
  std::array<std::uint16_t, kMaxCodes> length{};
  for (int i = 0; i < clear; ++i) {
    suffix[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
    first[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
    length[static_cast<std::size_t>(i)] = 1;
  }
  int next = end + 1;
  int code_size = min_code_size + 1;
  int prev = -1;
  std::uint32_t bits = 0;
  int nbits = 0;
  std::size_t pos = 0;
  std::size_t written = 0;
  while (written < out.size()) {
    while (nbits < code_size) {
      if (pos >= data.size()) {
        return;
      }
      bits |= std::uint32_t{data[pos++]} << nbits;
      nbits += 8;
    }
    const int code = static_cast<int>(bits & ((1u << code_size) - 1));
    bits >>= code_size;
    nbits -= code_size;
    if (code == clear) {
      next = end + 1;
      code_size = min_code_size + 1;
      prev = -1;
      continue;
    }
    if (code == end) {
      return;
    }
    const int string_code = code;
    if (code > next || (code == next && (prev < 0 || next == kMaxCodes))) {
      throw DataError(std::format("{}: bad LZW code {}", context, code));
    }
    if (prev >= 0 && next < kMaxCodes) {
      const auto n = static_cast<std::size_t>(next);
      const auto p = static_cast<std::size_t>(prev);
      prefix[n] = static_cast<std::uint16_t>(prev);
      // The new string is prev + first byte of the current one (which, for
      // the code being defined, is prev's own first byte).
      suffix[n] = code == next ? first[p] : first[static_cast<std::size_t>(code)];
      first[n] = first[p];
      length[n] = static_cast<std::uint16_t>(length[p] + 1);
      ++next;
      if (next == (1 << code_size) && code_size < 12) {
        ++code_size;
      }
    }
    // Writes the string of `string_code` backwards from its last byte.
    const std::size_t len = length[static_cast<std::size_t>(string_code)];
    std::size_t at = written + len;
    for (int c = string_code; at > written;) {
      --at;
      if (at < out.size()) {
        out[at] = suffix[static_cast<std::size_t>(c)];
      }
      c = prefix[static_cast<std::size_t>(c)];
    }
    written += len;
    prev = code;
  }
}

// Skips a colour table of the size given by `flags` (bit 7 present, bits
// 0..2 the size exponent).
void skip_color_table(ByteReader& r, std::uint8_t flags) {
  if ((flags & 0x80) != 0) {
    r.skip(std::size_t{3} << ((flags & 7) + 1));
  }
}

}  // namespace

Image8 decode_gif(std::span<const std::uint8_t> data, std::string_view context) {
  ByteReader r(data, context);
  const std::string signature = r.fixed_string(6);
  if (signature != "GIF87a" && signature != "GIF89a") {
    r.fail("not a GIF file");
  }
  r.skip(4);  // screen size
  const std::uint8_t flags = r.u8();
  r.skip(2);  // background colour, aspect
  skip_color_table(r, flags);
  while (!r.at_end()) {
    const std::uint8_t tag = r.u8();
    if (tag == 0x21) {
      r.u8();
      while (const std::uint8_t n = r.u8()) {
        r.skip(n);
      }
    } else if (tag == 0x2c) {
      r.skip(4);  // image position
      const int w = r.u16();
      const int h = r.u16();
      const std::uint8_t image_flags = r.u8();
      if ((image_flags & 0x40) != 0) {
        r.fail("interlaced GIF not supported");
      }
      skip_color_table(r, image_flags);
      const int min_code_size = r.u8();
      std::vector<std::uint8_t> blocks;
      while (const std::uint8_t n = r.u8()) {
        const auto b = r.bytes(n);
        blocks.insert(blocks.end(), b.begin(), b.end());
      }
      Image8 image(w, h);
      lzw_decode(blocks, min_code_size, image.pixels(), context);
      return image;
    } else {
      break;
    }
  }
  r.fail("no image in GIF");
}

}  // namespace oa
