#include "platform/touch.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace oa {

void TouchMapper::mouse(EventType type, int x, int y, MouseButton button) {
  input_.feed(InputEvent{.type = type, .x = x, .y = y, .button = button, .touch = true});
}

std::int64_t TouchMapper::spread() const {
  if (!second_) {
    return 0;
  }
  const std::int64_t dx = first_.x - second_->x;
  const std::int64_t dy = first_.y - second_->y;
  return dx * dx + dy * dy;
}

double TouchMapper::close_spread() const {
  if (down_.empty()) {
    return 0;
  }
  double cx = 0;
  double cy = 0;
  for (const Finger& f : down_) {
    cx += f.x;
    cy += f.y;
  }
  const auto n = static_cast<double>(down_.size());
  cx /= n;
  cy /= n;
  double sum = 0;
  for (const Finger& f : down_) {
    sum += std::hypot(f.x - cx, f.y - cy);
  }
  return sum / n;
}

void TouchMapper::start_pinch() {
  phase_ = Phase::kPinch;
  input_.begin_gesture(pinch_x_, pinch_y_, false);
  const std::int64_t now = spread();
  if (pinch_spread_ > 0 && now > 0 && now != pinch_spread_) {
    input_.zoom_gesture(static_cast<float>(std::sqrt(static_cast<double>(now) / static_cast<double>(pinch_spread_))));
  }
}

void TouchMapper::finish(bool tap) {
  switch (phase_) {
    case Phase::kPending:
      if (tap) {
        mouse(EventType::kMouseDown, start_x_, start_y_, MouseButton::kLeft);
        mouse(EventType::kMouseUp, start_x_, start_y_, MouseButton::kLeft);
      }
      break;
    case Phase::kLongPress:
      mouse(EventType::kMouseUp, first_.x, first_.y, MouseButton::kRight);
      break;
    case Phase::kDrag:
    case Phase::kPinch:
      input_.end_gesture();
      break;
    case Phase::kIdle:
    case Phase::kPinchDelay:
    case Phase::kClose:
    case Phase::kDone:
      break;
  }
}

void TouchMapper::feed(const InputEvent& event) {
  const Finger finger{.id = event.finger, .x = event.x, .y = event.y};
  const auto found = std::ranges::find(down_, finger.id, &Finger::id);
  switch (event.type) {
    case EventType::kFingerDown:
      if (found != down_.end()) {
        *found = finger;
      } else {
        down_.push_back(finger);
      }
      if (phase_ == Phase::kIdle) {
        first_ = finger;
        second_.reset();
        start_x_ = finger.x;
        start_y_ = finger.y;
        down_ms_ = event.time_ms;
        if (skip_query_ && skip_query_()) {
          input_.feed(InputEvent{.type = EventType::kKeyDown, .key = ' '});
          phase_ = Phase::kDone;
          return;
        }
        phase_ = Phase::kPending;
        mouse(EventType::kMouseMove, finger.x, finger.y);
        return;
      }
      if (phase_ == Phase::kDone) {
        return;
      }
      if (down_.size() >= 3) {
        if (phase_ != Phase::kClose) {
          finish(false);
          phase_ = Phase::kClose;
        }
        close_from_ = close_spread();
        return;
      }
      if (phase_ == Phase::kPending || phase_ == Phase::kDrag) {
        finish(false);
        second_ = finger;
        phase_ = Phase::kPinchDelay;
        pinch_ms_ = event.time_ms;
        pinch_spread_ = spread();
        pinch_x_ = (first_.x + second_->x) / 2;
        pinch_y_ = (first_.y + second_->y) / 2;
      }
      return;
    case EventType::kFingerMove: {
      if (found == down_.end()) {
        return;
      }
      *found = finger;
      if (phase_ == Phase::kClose) {
        const double now = close_spread();
        if (now < close_from_ * kCloseRatio && close_from_ - now > kSlop) {
          input_.feed(InputEvent{.type = EventType::kBack});
          phase_ = Phase::kDone;
        }
        return;
      }
      const bool is_first = finger.id == first_.id;
      const bool is_second = second_ && finger.id == second_->id;
      if (phase_ == Phase::kPinchDelay && (is_first || is_second)) {
        (is_first ? first_ : *second_) = finger;
        if (event.time_ms >= pinch_ms_ + kPinchDelay) {
          start_pinch();
        }
        return;
      }
      if (phase_ == Phase::kPinch && (is_first || is_second)) {
        const std::int64_t before = spread();
        (is_first ? first_ : *second_) = finger;
        const std::int64_t after = spread();
        if (before > 0 && after > 0) {
          input_.zoom_gesture(static_cast<float>(std::sqrt(static_cast<double>(after) / static_cast<double>(before))));
        }
        return;
      }
      if (!is_first) {
        return;
      }
      if (phase_ == Phase::kPending &&
          (std::abs(finger.x - start_x_) > kSlop || std::abs(finger.y - start_y_) > kSlop)) {
        phase_ = Phase::kDrag;
        input_.begin_gesture(start_x_, start_y_, false);
        input_.drag_gesture(finger.x - start_x_, finger.y - start_y_);
      } else if (phase_ == Phase::kDrag) {
        input_.drag_gesture(finger.x - first_.x, finger.y - first_.y);
      } else if (phase_ == Phase::kLongPress) {
        mouse(EventType::kMouseMove, finger.x, finger.y);
      } else if (phase_ != Phase::kPending) {
        return;
      }
      first_ = finger;
      return;
    }
    case EventType::kFingerUp:
    case EventType::kFingerCancel:
      if (found != down_.end()) {
        down_.erase(found);
      }
      if (phase_ == Phase::kIdle) {
        return;
      }
      if (phase_ == Phase::kPinchDelay && event.type == EventType::kFingerUp) {
        // A pinch quicker than the delay still zooms.
        start_pinch();
      }
      if (phase_ == Phase::kPinchDelay || phase_ == Phase::kPinch || phase_ == Phase::kClose ||
          finger.id == first_.id) {
        finish(event.type == EventType::kFingerUp);
        phase_ = Phase::kDone;
      }
      if (down_.empty()) {
        phase_ = Phase::kIdle;
      }
      return;
    default:
      return;
  }
}

void TouchMapper::update(std::uint64_t now_ms) {
  if (phase_ == Phase::kPending && now_ms - down_ms_ >= kLongPress) {
    phase_ = Phase::kLongPress;
    mouse(EventType::kMouseDown, start_x_, start_y_, MouseButton::kRight);
  } else if (phase_ == Phase::kPinchDelay && now_ms >= pinch_ms_ + kPinchDelay) {
    start_pinch();
  }
}

}  // namespace oa
