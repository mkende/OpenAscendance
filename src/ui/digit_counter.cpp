#include "ui/digit_counter.h"

#include <format>

#include "ui/winmgr.h"

namespace oa {

namespace {

constexpr int kBackgroundFrame = 12;
constexpr int kDigitStep = 0x1f;

}  // namespace

bool DigitCounter::handle_event(int type, int a, int b) {
  if (type == kEventRefresh) {
    draw(kDrawNormal);
    return false;
  }
  return GWindow::handle_event(type, a, b);
}

void DigitCounter::draw(int /*mode*/) {
  if (shapes_ == nullptr) {
    return;
  }
  vfx_shape_draw(wm_.screen(), pane_, shapes_->at(static_cast<std::size_t>(digit_frames_ + kBackgroundFrame)), 0, 0);
  const GameSession& game = wm_.services().game;
  const std::string digits = std::format("{:05d}", game.day());
  const int color = game.race_color(game.player_race());
  for (int i = 0; i < 5; ++i) {
    const int frame = digit_frames_ + (digits[static_cast<std::size_t>(i)] - '0');
    wm_.draw_colored(pane_, shapes_->at(static_cast<std::size_t>(frame)), i * kDigitStep + 2, 0, color);
  }
}

}  // namespace oa
