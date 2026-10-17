// Autodesk FLC animations (`data\intro65a.flc`, the loose `LOGIC.FLC`),
// decoded like the original's player (`Flic_play_frame` 0x28dd4,
// `docs/02-graphics-formats.md`): the chunks COLOR_256, DELTA_FLC, BRUN, COPY
// and PSTAMP, with the original's quirks.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/image.h"

namespace oa {

struct FlcHeader {
  std::uint32_t size = 0;
  int frames = 0;  // without the ring frame
  int width = 0;
  int height = 0;
  int depth = 0;
  std::uint32_t speed_ms = 0;  // ignored by the game
};

// What a decoded frame held, for the player's side effects.
struct FlcFrameChunks {
  bool color256 = false;
  bool brun = false;
  bool copy = false;
};

// A FLC file loaded whole.
class FlcFile {
 public:
  // Receives a palette entry of a COLOR_256 chunk: index, 8-bit r, g, b.
  using ColorSink = std::function<void(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b)>;

  // Parses the header and indexes the frames; throws `DataError` naming
  // `context` when it is not an FLC (magic 0xAF12) or a frame overruns.
  explicit FlcFile(std::vector<std::uint8_t> data, std::string_view context = "FLC");

  const FlcHeader& header() const { return header_; }

  // Decodes frame `n` (0-based, below `header().frames + 1`, the ring frame
  // last) onto `screen` with its top-left corner at (x, y), as the previous
  // frames left it. COLOR_256 entries go to `colors`, except those whose
  // position in their packet is in `protect_first .. protect_first +
  // protect_count - 1` (`Flic_decode_color256` 0x2938c protects the subtitle
  // colours that way, without consuming them). A DELTA_FLC "last pixel" word
  // and zero words are ignored (`0x2a460`). Throws `DataError` on unknown
  // chunks (LC, BLACK, COLOR_64 assert in the original) or bad data.
  FlcFrameChunks decode_frame(int n, Image8& screen, int x, int y, const ColorSink& colors, int protect_first = 256,
                              int protect_count = 0) const;

 private:
  std::vector<std::uint8_t> data_;
  std::string context_;
  FlcHeader header_;
  std::vector<std::size_t> frame_offsets_;  // frames + ring frame
};

}  // namespace oa
