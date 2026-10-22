// The FLIC window (`flicwin.cpp`, class `FlicWnd`, `windows.txt` TYPE 1,
// ctor 0x2a5a0, vtable 0x85b34): plays its FLICFILE when its screen is
// entered (the intro, screen 6), then posts its SENDMESSAGE (3: back).
#pragma once

#include <memory>

#include "ui/flic.h"
#include "ui/gwindow.h"

namespace oa {

class FlicWnd final : public GWindow {
 public:
  // The FLIC's start rate (`Flic_set_frame_rate(0x12)`), before the script's
  // DELAY events.
  static constexpr int kStartFrameRate = 18;

  // Reads FLICFILE and LOOPCOUNT from `record`; the FLIC is opened when the
  // window is activated.
  FlicWnd(WinMgr& wm, const WindowRecord& record);

  // `FlicWnd_handle_event` (0x2a710): 1 starts the animation (pointer
  // hidden, menu music stopped), 10 plays a frame, 3/4/5 skip, 2 fades the
  // picture and music out and reloads `game.pal` for the next screen's fade.
  bool handle_event(int type, int a, int b) override;
  // The whole screen (a click anywhere skips).
  void press_areas(std::vector<PressArea>& out) const override { out.push_back(PressArea{Pane{}, false}); }
  // `FlicWnd_draw` (0x2a9dc): nothing (the frames draw themselves).
  void draw(int /*mode*/) override {}

  // The player while the window plays, null otherwise.
  const Flic* flic() const { return flic_.get(); }

 private:
  // `FlicWnd_preload`: returns false when the animation cannot be loaded.
  bool preload();
  void finish();

  std::string file_;
  int loop_count_;
  int loops_ = 0;
  std::unique_ptr<Flic> flic_;
};

}  // namespace oa
