#include "ui/flicwnd.h"

#include <string>

#include "core/error.h"
#include "core/fixed_width.h"
#include "core/log.h"
#include "ui/winmgr.h"

namespace oa {

FlicWnd::FlicWnd(WinMgr& wm, const WindowRecord& record)
    : GWindow(wm, record, nullptr),
      file_(record.extras.at("FLICFILE")),
      loop_count_(std::stoi(record.extras.at("LOOPCOUNT"))) {}

bool FlicWnd::preload() {
  try {
    if (!flic_) {
      flic_ = std::make_unique<Flic>(wm_.services(), file_);
    }
  } catch (const DataError& e) {
    log_error("{}", e.what());
    return false;
  }
  flic_->preload();
  return true;
}

void FlicWnd::finish() {
  wm_.kill_timer(id_, kEventTick);
  wm_.post_message(message_, param1_, param2_);
}

bool FlicWnd::handle_event(int type, int a, int b) {
  UiServices& s = wm_.services();
  switch (type) {
    case kEventActivate:
      loops_ = 0;
      s.show_pointer(false);
      wm_.cancel_pending_fade();
      if (!preload()) {
        s.show_pointer(true);
        wm_.post_message(message_, param1_, param2_);
        return false;
      }
      s.sound.stop_tune();
      GWindow::handle_event(type, a, b);
      wm_.set_timer(id_, kEventTick, 0, 0, 0);
      flic_->set_frame_rate(kStartFrameRate);
      return false;
    case kEventDeactivate: {
      if (flic_) {
        flic_->stop_timer();
      }
      // 64 steps to black (a vertical blank each), the music volume from 127
      // down by 2 per step (the first value, -127, is ignored).
      int volume = -0x7f;
      for (int k = 0; k < 0x40; ++k) {
        s.gsystem.fade_to_color_step(k);
        s.gsystem.wait_vblank();
        s.sound.set_music_volume(volume);
        volume = wrap_i8(volume - 2);  // a char: -127 - 2 wraps to 127
      }
      s.sound.stop_tune();
      s.gsystem.set_dac_range(0, 256);
      vfx_pane_wipe(s.gsystem.screen(), GSystem::kScreenPane, 0);
      s.gsystem.load_palette(s.files, "DATA\\game.pal", 0, 256);
      s.gsystem.set_dac_range(0, 256);
      wm_.start_pending_fade();
      s.show_pointer(true);
      flic_.reset();
      GWindow::handle_event(type, a, b);
      return false;
    }
    case kEventTick: {
      if (!flic_) {
        return true;
      }
      const std::optional<bool> played = flic_->play_frame();
      if (played == false && ++loops_ < loop_count_) {
        flic_->preload();
      }
      if (loops_ < loop_count_) {
        return true;
      }
      finish();
      return true;
    }
    case kEventKey:
    case kEventLeftClick:
    case kEventRightClick:
      finish();
      return true;
    default:
      return GWindow::handle_event(type, a, b);
  }
}

}  // namespace oa
