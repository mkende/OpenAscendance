// The screenshot writer's GIF encoder (`gif_lzw_*` 0x30fc0-0x31228).

#include <map>
#include <utility>

#include "assets/gif.h"

namespace oa {

namespace {

constexpr int kClearCode = 0x100;
constexpr int kEndCode = 0x101;
constexpr int kFirstCode = 0x102;
constexpr int kMaxCodes = 0x1000;

void put_u16(std::vector<std::uint8_t>& out, int v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xff));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
}

// The LZW code stream, packed LSB first into 255-byte sub-blocks.
class CodeWriter {
 public:
  void put(int code, int bits) {
    acc_ |= static_cast<std::uint32_t>(code) << used_;
    used_ += bits;
    while (used_ >= 8) {
      bytes_.push_back(static_cast<std::uint8_t>(acc_ & 0xff));
      acc_ >>= 8;
      used_ -= 8;
    }
  }
  // Flushes the last bits and appends the sub-blocks and their terminator
  // to `out`.
  void finish(std::vector<std::uint8_t>& out) {
    if (used_ > 0) {
      bytes_.push_back(static_cast<std::uint8_t>(acc_ & 0xff));
    }
    for (std::size_t i = 0; i < bytes_.size(); i += 255) {
      const std::size_t n = std::min<std::size_t>(255, bytes_.size() - i);
      out.push_back(static_cast<std::uint8_t>(n));
      out.insert(out.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(i),
                 bytes_.begin() + static_cast<std::ptrdiff_t>(i + n));
    }
    out.push_back(0);
  }

 private:
  std::vector<std::uint8_t> bytes_;
  std::uint32_t acc_ = 0;
  int used_ = 0;
};

}  // namespace

std::vector<std::uint8_t> encode_gif(const Image8& image, const VgaPalette& palette) {
  std::vector<std::uint8_t> out{'G', 'I', 'F', '8', '7', 'a'};
  put_u16(out, image.width());
  put_u16(out, image.height());
  out.push_back(0x87);  // global table of 256 colours, 8 bits
  out.push_back(0xff);  // background
  out.push_back(0);
  for (const std::uint8_t v : palette.dac) {
    out.push_back(static_cast<std::uint8_t>(v * 255 / 63));
  }
  out.push_back(',');
  put_u16(out, 0);
  put_u16(out, 0);
  put_u16(out, image.width());
  put_u16(out, image.height());
  out.push_back(7);
  out.push_back(8);  // LZW minimum code size

  CodeWriter codes;
  int bits = 9;
  int next = kFirstCode;
  std::map<std::pair<int, std::uint8_t>, int> table;
  codes.put(kClearCode, bits);
  const std::span<const std::uint8_t> pixels = image.pixels();
  int prefix = pixels.empty() ? 0 : pixels[0];
  for (std::size_t i = 1; i < pixels.size(); ++i) {
    const std::uint8_t c = pixels[i];
    if (const auto it = table.find({prefix, c}); it != table.end()) {
      prefix = it->second;
      continue;
    }
    codes.put(prefix, bits);
    table[{prefix, c}] = next++;
    if (next > (1 << bits)) {
      if (next > kMaxCodes) {
        codes.put(kClearCode, bits);
        table.clear();
        next = kFirstCode;
        bits = 9;
      } else {
        ++bits;
      }
    }
    prefix = c;
  }
  codes.put(prefix, bits);
  codes.put(kEndCode, bits);
  codes.finish(out);
  out.push_back(';');
  return out;
}

}  // namespace oa
