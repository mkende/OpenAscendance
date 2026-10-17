// Translation of SDL keys to the BIOS keyboard words the original reads
// with INT 16h (`input_poll` 0x25c8c, `eventwnd`): `scan << 8 | ascii` as a
// US enhanced keyboard gives them (gray cursor keys with ASCII 0xe0, keypad 5
// as 0x4cf0, Alt combinations with ASCII 0), and the BIOS shift state byte
// (0x417). Copied from SilentHeritage's platform/bios_keys.h.
//
// Keyboard events follow the active layout: a character key gives the word
// a US keyboard gives for the character the layout types (the scan code of
// the US key typing it), so that hotkeys matching a character fire on the
// character printed on the key. With Alt and Ctrl, a key typing a letter
// gives the Alt / Ctrl word of that letter and any other key the word of its
// physical position, like the BIOS under a DOS national keyboard driver
// (KEYB FR): Alt+X quits on AZERTY as on a US keyboard. AltGr types the
// layout's AltGr character when it is ASCII, and otherwise acts as Alt.
// Characters outside ASCII give no word. The non-character keys (F-keys,
// cursor block, keypad, Esc, Enter, Tab, Backspace, Space) keep their scan
// codes; Android's Back key (SDLK_AC_BACK) gives the Esc words.
//
// Text typed through the system's text input arrives as `EventType::kText`
// characters, which give the word a US keyboard gives for the character.
#pragma once

#include <cstdint>
#include <optional>

#include "platform/input_event.h"

namespace oa {

// Returns the BIOS keyboard word of the SDL key `key` of a US keyboard
// pressed with the SDL modifiers `mods` (Alt takes precedence over Ctrl,
// Ctrl over Shift; Caps Lock and Num Lock apply to letters and the keypad),
// or nothing for a key that puts no word in the buffer (a lone modifier, an
// unmapped key, combinations the BIOS ignores such as Ctrl-1). A printable
// ASCII `key` that the US keyboard types with Shift (such as '+') gives the
// word of that shifted key.
std::optional<std::uint16_t> bios_key_word(std::uint32_t key, std::uint16_t mods);

// Returns the BIOS word of a `EventType::kKeyDown` event (repeats
// included, like the typematic repeat) or of a `EventType::kText`
// character, nothing for other events. Key events with a scancode follow
// the layout (see the file comment), synthesized ones without are read by
// `bios_key_word(key, mods)`.
std::optional<std::uint16_t> bios_key_word(const InputEvent& event);

// Returns the word a US keyboard gives for typing the character `c`
// (Shift included for the shifted characters), nothing outside printable
// ASCII.
std::optional<std::uint16_t> bios_character_word(char32_t c);

// Returns the BIOS shift state byte (0x417) for the SDL modifiers: bit 0
// right Shift, 1 left Shift, 2 Ctrl, 3 Alt, 4 Scroll Lock, 5 Num Lock,
// 6 Caps Lock.
std::uint8_t bios_shift_state(std::uint16_t mods);

}  // namespace oa
