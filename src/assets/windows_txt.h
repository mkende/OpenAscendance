// `windows.txt`, the window and screen layout read by `WinMgr_load_windows`
// (0x52a80, `docs/07-ui-framework.md`, "`windows.txt` semantics"): `%s %d` /
// `%s %s` pairs read in a fixed order per record type, the keyword names
// being ignored (the file has typos such as `cX0` that the game never sees).
#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace oa {

// A TEXTITEM (kind 0) or SHAPEITEM (kind 1) of a window.
struct WindowItem {
  int kind = 0;
  std::string text;  // the text (`^` kept), or the shape file
  int flags = 0;     // text: font flags; shape: the frame
  int x = 0;
  int y = 0;
};

// A window record (TYPE other than 100, 200, 300).
struct WindowRecord {
  int type = 0;
  std::string name;  // `^` kept (drawn as the font's `^` glyph, a blank)
  std::string parent = "NONE";
  std::vector<int> screens;   // STATE: the first one gives the shape table
  bool mouse_focus = false;   // MOUSEFOCUS
  bool button_state = false;  // BUTTONSTATE (TYPE 25)
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  int shape_frame = -1;           // SHAPEFRAME (-1 = text button)
  int digit_frames = 0;           // DIGITFRAMES (TYPE 15)
  int sound = -1;                 // SOUNDCHANNEL
  int message = 0;                // SENDMESSAGE
  int param1 = 0;                 // SENDPARAM1
  int param2 = 0;                 // SENDPARAM2
  std::string help = "0";         // HELPINDEX
  std::vector<WindowItem> items;  // WINITEMS
  // The class-specific values, under the file's keyword spelling of this
  // project's reading order (`FLICFILE`, `LOOPCOUNT`, `RACE1CONTROL`...):
  // each value's tokens joined by single spaces.
  std::map<std::string, std::string> extras;
};

struct WindowsTxt {
  int mem_req = 0;
  std::vector<std::string> haze_tables;  // TYPE 200, by index
  std::string small_font;                // TYPE 300
  std::string large_font;
  std::map<int, std::string> screen_shapes;  // TYPE 100: screen -> shape file
  std::vector<WindowRecord> windows;         // in file order
};

// Parses `windows.txt`; throws `DataError` on a short file or an unknown
// record type.
WindowsTxt parse_windows_txt(std::span<const std::uint8_t> data);

}  // namespace oa
