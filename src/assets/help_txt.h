// The help files (`help.txt`, `gizhelp.txt`, `planhelp.txt`, the tutorials'
// `.txt`): topics of texts, pictures and buttons (`HelpWnd_load_topic`
// 0x2f5d8, `recon/notes/ui.md`, "Help system").
//
// A topic starts at the content line `HELP <name>` (keywords are matched
// case-insensitively) and ends at the next `HELP`; the topic `END` closes
// the file. Its lines:
//
// - `TEXT [x y colour]` ... `ENDTEXT`: a text element (the large font);
//   its lines are joined, each line end becoming a space and `@` a line
//   break.
// - `SHAPE [index frame x y]`: a picture, frame `frame` of `gwshare.txt`
//   table `index`.
// - `POS x y`, `FONT LARGE|SMALL`, `FLAGS TJ_HC|TJ_VC|TJ_R|TJ_B...`,
//   `CENTERPOS`, `NEWPAGE` (the page ends after it): apply to the last
//   element (the first slot before any).
// - `BUTTON` shows the next button (the first one at the first `BUTTON`);
//   `TITLE text`, `MSG n`, `PANE x0 y0 x1 y1`, `BPOS BL|BR` set the current
//   one.
//
// Comment and blank lines (starting with a space, a tab, `/` or `#`) are
// skipped everywhere, also inside a text.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "gfx/vfx.h"

namespace oa {

struct HelpElement {
  enum class Kind : std::uint8_t { kShape = 0, kText = 1 };

  Kind kind = Kind::kShape;
  int shape_index = 0;  // `gwshare.txt` table
  int frame = 0;
  std::string text;  // line breaks as `\n`
  int color = -1;    // -1: the font's default
  bool small_font = false;
  int flags = 0;  // `font_draw_wrapped` flags (TJ_*)
  int x = 0;
  int y = 0;
  bool page_end = false;
};

// What a topic does to a HELPBUTTONn child.
struct HelpButton {
  bool shown = false;
  std::optional<std::string> title;  // the caption (`^` kept)
  std::optional<int> message;        // its SENDPARAM1, the modal result
  std::optional<Pane> pane;          // PANE
  // BPOS: bottom left (`L`) or right (`R`) of the window.
  std::optional<char> corner;
};

struct HelpTopic {
  // The most elements and texts of a topic, and buttons of the window.
  static constexpr int kMaxElements = 9;
  static constexpr int kMaxTexts = 5;
  static constexpr int kMaxButtons = 5;
  // The most characters of a text (line ends counted).
  static constexpr int kMaxText = 0x253;

  int element_count = 0;
  // The element slots: POS and the like before the first element change
  // slot 0, which the first element then takes over.
  std::array<HelpElement, kMaxElements> elements{};
  int pages = 0;  // the NEWPAGE count, the last page's index
  std::array<HelpButton, kMaxButtons> buttons{};
};

// Returns topic `topic` of the help file `data`. `center` is the position
// CENTERPOS sets (half the window's width and height above the buttons).
// Throws `DataError` naming `context` when the topic is missing or passes
// the original's limits.
HelpTopic parse_help_topic(std::span<const std::uint8_t> data, std::string_view topic, std::string_view context,
                           int center_x, int center_y);

}  // namespace oa
