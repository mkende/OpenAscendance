// The FLIC player (`flic.cpp`, class `Flic`, 0x28360-0x2a5a0): an FLC file
// decoded one frame per tick of its own timer, with the `SUBTITLE.TXT`
// script of subtitles, frame rates and music cues
// (`docs/07-ui-framework.md`, "FLIC"; `docs/02-audio-formats.md`).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "assets/flc.h"
#include "assets/flic_script.h"
#include "gfx/font.h"
#include "ui/winmgr.h"

namespace oa {

class Flic {
 public:
  // The script every FLIC opens (`Flic_open` 0x285d4).
  static constexpr std::string_view kScriptFile = "SUBTITLE.TXT";

  // `Flic_open` (0x285d4): loads the script, its font and palette, and the
  // FLC `file`, centred on the screen. Throws `DataError`.
  Flic(UiServices& services, std::string_view file);

  const FlcFile& file() const { return *flc_; }
  const FlicScript& script() const { return script_; }
  // The frame counter (`+0x19c`): the 0-based index of the last frame
  // decoded, -1 before the first.
  int frame() const { return frame_; }
  // The current frame rate in Hz (`+0x1e0`).
  int frame_rate() const { return rate_; }

  // `Flic_preload` (0x28758): rewinds the animation and its script. The
  // original reads 2 MB of it in 64 chunks, a palette fade to black step
  // after each (one vertical blank each), then clears the screen.
  void preload();
  // `Flic_set_frame_rate` (0x2a3a4): starts the frame timer at `hz`.
  void set_frame_rate(int hz);
  // `Flic_stop_timer` (0x2a428).
  void stop_timer();

  // `Flic_play_frame` (0x28dd4): nothing while no timer tick came since the
  // last frame (returns nullopt: ticks are coalesced, frames never skipped);
  // else decodes the next frame, steps the subtitle and palette fades, runs
  // the frame's script events and returns true; returns false after the
  // last frame.
  std::optional<bool> play_frame();

 private:
  struct Subtitle {
    std::string text;
    int dx = 0;
    int dy = 0;
  };

  // Updates the timer flag from the clock (the AIL timer callback
  // `flic_timer_callback` 0x28360).
  void poll_timer();
  void run_script_events();

  UiServices& services_;
  FlicScript script_;
  std::unique_ptr<GameFont> font_;
  std::vector<std::uint8_t> font_palette_;
  std::vector<Subtitle> subtitles_;
  std::unique_ptr<FlcFile> flc_;
  int x_ = 0;
  int y_ = 0;
  int frame_ = -1;
  std::size_t cursor_ = 0;
  int fade_mode_ = 0;     // `+0x92a`: 1 subtitle fade in, 2 out
  int palette_step_ = 0;  // `+0x92c`
  int rate_ = 0;
  bool timer_running_ = false;
  std::uint64_t next_tick_us_ = 0;
  bool frame_due_ = false;  // `0xc84dc`
};

}  // namespace oa
