#include "ui/text_box.h"

#include "ui/winmgr.h"

namespace oa {

void TextBox::draw(int /*mode*/) {
  Image8& screen = wm_.screen();
  vfx_pane_wipe(screen, pane_, kBackColor);
  if (border_) {
    draw_border(kTextColor);
  }
  if (!text_.empty()) {
    GameFont& font = wm_.large_font();
    font.pane() = pane_;
    font.draw_wrapped(screen, 0, 0, text_, flags_, color_, 0xff, pane_.x1 - pane_.x0 - 5);
  }
}

}  // namespace oa
