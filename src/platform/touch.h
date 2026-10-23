// The touch screen as the original's mouse (a remake addition): the fingers'
// events turned into the pointer's moves and button changes that the
// original reads, into the view gestures (`ViewGesture`) and the system's
// Back.
//
// - A finger put down moves the pointer there at once (the hover).
// - Lifted without moving past `kSlop` before `kLongPress`: a left click
//   there (a press and a release).
// - Held still for `kLongPress`: what the long press query answers for the
//   place (`LongPress`): a help click, the right or left button pressed
//   there and released with the finger, or the pointer following the
//   finger.
// - Moved past `kSlop` first: a view drag, without any click.
// - A second finger: a pinch, without any click, until every finger is up.
//   Its zoom starts `kPinchDelay` after the second finger (or when the
//   fingers lift), so that a third finger coming meanwhile zooms nothing.
// - A third finger (or more): nothing until the fingers come together, their
//   mean distance to their centre down to `kCloseRatio` of what it was when
//   the last of them was put down; then Back (`kBack`), once. Lifting one of
//   them first does nothing.
// - Put down while an animation that Space skips plays (`set_skip_query`):
//   Space, at once, and nothing else until every finger is up (the tap
//   never clicks what is under it).
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "platform/input.h"
#include "platform/input_event.h"

namespace oa {

class TouchMapper {
 public:
  // The distance a finger may move and still tap, in screen pixels.
  static constexpr int kSlop = 10;
  // The hold of a long press, in milliseconds.
  static constexpr std::uint64_t kLongPress = 500;
  // The time a third finger has to join a pinch before it zooms, in
  // milliseconds.
  static constexpr std::uint64_t kPinchDelay = 100;
  // The part of their spread three fingers or more come together to before
  // they are Back.
  static constexpr double kCloseRatio = 0.6;
  // Whether the lift of a `LongPress::kMoveDrag` is a left click.
  static constexpr bool kMoveDragLiftClicks = true;

  // `input` gets the mouse events and gestures; it must outlive the mapper.
  explicit TouchMapper(Input& input) : input_(input) {}

  // Takes a finger event (`kFingerDown` .. `kFingerCancel`); others are
  // ignored.
  void feed(const InputEvent& event);
  // Sets the query of whether an animation that Space skips plays (none:
  // never).
  void set_skip_query(std::function<bool()> query) { skip_query_ = std::move(query); }
  // Sets the query of a long press's kind at a place of the screen (none:
  // `LongPress::kRight` everywhere).
  void set_long_press_query(std::function<LongPress(int x, int y)> query) { long_press_query_ = std::move(query); }
  // Starts a long press once a finger has been held `kLongPress`, and
  // starts the zoom of a pinch `kPinchDelay` after its second finger
  // (`now_ms` on the clock of the events' `time_ms`).
  void update(std::uint64_t now_ms);

 private:
  enum class Phase : std::uint8_t {
    kIdle,
    kPending,     // one finger down, not moved nor held yet
    kLongPress,   // a long press held (`held_`)
    kDrag,        // a view drag
    kPinchDelay,  // two fingers, not zooming yet
    kPinch,       // two fingers zooming
    kClose,       // three fingers or more, until they come together
    kDone,        // the gesture is over; waiting for every finger to lift
  };
  struct Finger {
    std::uint64_t id = 0;
    int x = 0;
    int y = 0;
  };

  // Feeds a mouse event made from the touch.
  void mouse(EventType type, int x, int y, MouseButton button = MouseButton::kNone);
  // The squared distance between the two fingers of the pinch.
  std::int64_t spread() const;
  // The mean distance of the fingers down to their centre.
  double close_spread() const;
  // Starts the pinch's zoom: its gesture, and the zoom since the second
  // finger was put down.
  void start_pinch();
  // Ends the phase's gesture or button (`tap`: a pending touch clicks).
  void finish(bool tap);

  Input& input_;
  std::function<bool()> skip_query_;
  std::function<LongPress(int x, int y)> long_press_query_;
  // The button a `kLongPress` holds (none: a `LongPress::kMoveDrag`).
  MouseButton held_ = MouseButton::kNone;
  Phase phase_ = Phase::kIdle;
  // Every finger down, where it is now.
  std::vector<Finger> down_;
  // The finger of a tap, a long press or a drag, and the pinch's second
  // (where the phase last took them).
  Finger first_;
  std::optional<Finger> second_;
  int start_x_ = 0;
  int start_y_ = 0;
  std::uint64_t down_ms_ = 0;
  // The pinch's start: when the second finger was put down, the fingers'
  // squared distance then and their middle.
  std::uint64_t pinch_ms_ = 0;
  std::int64_t pinch_spread_ = 0;
  int pinch_x_ = 0;
  int pinch_y_ = 0;
  // `close_spread` when the last finger of a `kClose` was put down.
  double close_from_ = 0;
};

}  // namespace oa
