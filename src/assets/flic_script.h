// The FLIC script `SUBTITLE.TXT` (`Flic_load_script` 0x298fc,
// `docs/02-audio-formats.md`, "Intro cutscene script"): the subtitle box and
// font, and the frame-numbered events (subtitles, frame rate, music).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace oa {

// A script event (12 bytes in the original: `u16 frame`, `u16 type`, two
// `u32` arguments).
struct FlicEvent {
  enum Type : std::uint16_t {
    kShowSubtitle = 1,  // a1: subtitle index
    kHideSubtitle = 2,
    kFrameRate = 3,     // a1: rate in Hz (0 stops the timer)
    kMusicVolume = 11,  // a1: channel (ignored), a2: volume 0..127
    kPlayTune = 30,     // a1: tune id
    kStopTune = 31,
  };

  std::uint16_t frame = 0;
  std::uint16_t type = 0;
  std::uint32_t a1 = 0;
  std::uint32_t a2 = 0;
};
static_assert(sizeof(FlicEvent) == 12);

struct FlicScript {
  static constexpr std::size_t kMaxEvents = 128;
  // Subtitles show this many frames before their FRAME_START.
  static constexpr int kSubtitleLead = 32;

  // TITLEBOX: the subtitle rectangle, inclusive.
  int title_x0 = 0;
  int title_y0 = 0;
  int title_x1 = 0;
  int title_y1 = 0;
  // SUBFONT.
  std::string font_file;
  std::string palette_file;
  int palette_start = 0;
  int palette_count = 0;
  int back_color = 0;
  // The subtitle texts (`_` as spaces), by index.
  std::vector<std::string> subtitles;
  // The events, sorted by frame with the Watcom `qsort` (not stable).
  std::vector<FlicEvent> events;
};

// Parses `SUBTITLE.TXT`: `%s %d` records whose keyword names are ignored, the
// number giving the record type (0 TITLEBOX, 1 SUBFONT, 2 SUBTITLE, 3 DELAY,
// 11 FADEMUSIC, 16 DIGITALMUSIC, -1 END). Throws `DataError` on an unknown
// type, a subtitle starting at frame 32 or before, or more than 128 events.
FlicScript parse_flic_script(std::span<const std::uint8_t> data);

}  // namespace oa
