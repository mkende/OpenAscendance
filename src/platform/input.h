// The original's input layer (`eventwnd`, 0x25a10-0x25cc8): the keyboard
// read through the BIOS (INT 16h, one key per main-loop pass, typematic
// repeat from the buffer) and the mouse polled with edge detection, kept in
// globals the window manager reads (`docs/07-ui-framework.md`, "Input
// layer"). Here they are fed by the platform's input events.
#pragma once

#include <cstdint>
#include <deque>

#include "platform/input_event.h"

namespace oa {

// The input globals (`0xc8484..0xc84d8`) after a poll. Booleans are plain
// `bool` here (0 / -1 in the original).
struct InputState {
  bool key_event = false;         // 0xc8488: a key was read this pass
  bool alt = false;               // 0xc848c
  bool ctrl = false;              // 0xc8490
  bool shift = false;             // 0xc8494: either Shift
  std::uint16_t scan_code = 0;    // 0xc84ac
  std::uint16_t ascii = 0;        // 0xc84ae
  std::uint16_t shift_flags = 0;  // 0xc84b0: BIOS shift state (INT 16h AH=2)
  bool mouse_present = true;      // 0xc84b4
  bool left_down = false;         // 0xc84b8
  bool right_down = false;        // 0xc84bc
  bool left_changed = false;      // 0xc84c4: pressed or released this pass
  bool right_changed = false;     // 0xc84c8
  bool moved = false;             // 0xc84d0
  int x = 0;                      // 0xc84d4
  int y = 0;                      // 0xc84d8
};

// Turns the platform's input events into the original's polled state.
class Input {
 public:
  // The BIOS keyboard buffer's capacity (keys typed beyond it are lost).
  static constexpr std::size_t kKeyBufferSize = 15;

  // Takes one input event: key words go to the keyboard buffer, the mouse
  // updates the pointer and queues its button changes.
  void feed(const InputEvent& event);

  // `input_poll` (0x25c8c): clears this pass's event flags, reads the shift
  // state and at most one key, and the mouse with edge detection against the
  // previous poll. Button changes queued since are applied one per button
  // per poll, so that a press and its release always show on two passes
  // (the original's mouse status could miss a click shorter than a pass).
  const InputState& poll();

  const InputState& state() const { return state_; }

 private:
  struct ButtonChange {
    MouseButton button;
    bool down;
  };

  InputState state_;
  std::deque<std::uint16_t> keys_;
  std::deque<ButtonChange> buttons_;
  std::uint16_t mods_ = 0;
  int mouse_x_ = 0;
  int mouse_y_ = 0;
};

}  // namespace oa
