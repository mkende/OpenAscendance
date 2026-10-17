// The input events the platform layer delivers (from SDL, or scripted by
// `--keys`).
#pragma once

#include <cstdint>

namespace oa {

enum class EventType : std::uint8_t {
  kQuit,  // the window's close button, or the system ending the program
  kKeyDown,
  kKeyUp,
  kMouseMove,
  kMouseDown,
  kMouseUp,
  // A character of the system's text input: its printable ASCII code in
  // `character` (and `key`).
  kText,
};

enum class MouseButton : std::uint8_t { kNone, kLeft, kRight, kMiddle };

// An input event. Mouse coordinates are in pixels of the 640x480 screen (the
// letterbox and the scaling removed; outside the picture they fall out of
// 0..639 x 0..479). `key` is the SDL keycode (printable keys are their
// lower-case ASCII code), `mods` the SDL modifier mask. Key events from the
// keyboard also carry the physical key and the characters the layout types
// with it; synthesized events leave `scancode` 0 and are read as keys of a US
// keyboard.
struct InputEvent {
  EventType type = EventType::kQuit;
  std::uint32_t key = 0;
  std::uint16_t mods = 0;
  std::uint32_t scancode = 0;  // SDL scancode, 0 when synthesized
  // The code point the layout types with the key under the current Shift and
  // Caps Lock state (SDL keycode values).
  char32_t character = 0;
  // The code point typed with AltGr under the same Shift state, 0 when AltGr
  // does not change the character.
  char32_t altgr_character = 0;
  bool repeat = false;
  int x = 0;
  int y = 0;
  MouseButton button = MouseButton::kNone;
};

}  // namespace oa
