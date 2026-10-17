#include "assets/windows_txt.h"

#include <format>
#include <initializer_list>
#include <string_view>

#include "assets/tokens.h"

namespace oa {

namespace {

// The fields of a record, in reading order.
enum class Field : std::uint8_t {
  kName,
  kParent,
  kState,
  kMouseFocus,
  kButtonState,
  kRect,
  kShapeFrame,
  kDigitFrames,
  kSound,
  kMessage,  // SENDMESSAGE, SENDPARAM1, SENDPARAM2
  kHelp,
  kItems,
  kControls,  // NUMCONTROLS n, then n records of 7 integers
};

// A class-specific value: its name and whether it is a string.
struct Extra {
  std::string_view key;
  bool string;
};

struct Layout {
  std::vector<Field> fields;  // the generic part
  // The class-specific values, read after the field `extras_after`.
  std::vector<Extra> extras;
  Field extras_after = Field::kRect;
  // Extra values read after the messages and help (TYPE 1).
  std::vector<Extra> trailing;
};

const std::vector<Field> kGeneric{Field::kName, Field::kParent,     Field::kState, Field::kMouseFocus,
                                  Field::kRect, Field::kShapeFrame, Field::kSound, Field::kMessage,
                                  Field::kHelp, Field::kItems};

Layout layout_of(int type, TokenReader& r) {
  switch (type) {
    case 1:
      return {{Field::kName, Field::kParent, Field::kState, Field::kRect, Field::kMessage, Field::kHelp},
              {{"FLICFILE", true},
               {"BUFFERMEGS", false},
               {"PRELOADMEGS", false},
               {"MAXFRAME_KB", false},
               {"READSIZE_KB", false},
               {"SKIPFRAME", false}},
              Field::kRect,
              {{"LOOPCOUNT", false}, {"SHOWINFO", false}}};
    case 2:
      return {{Field::kName, Field::kParent, Field::kState, Field::kRect, Field::kMessage, Field::kHelp},
              {{"BACKGRNDCOLOR", false},
               {"STAR_SHAPES", true},
               {"SHIP_SHAPES", true},
               {"NUM_STARS", false},
               {"NUM_LANES", false},
               {"SMALLSEED", false},
               {"MEDIUMSEED", false},
               {"LARGESEED", false},
               {"HUGESEED", false},
               {"CURSORSHAPE", false}},
              Field::kRect,
              {}};
    case 3:
      return {{Field::kName, Field::kParent, Field::kState, Field::kRect, Field::kShapeFrame, Field::kMessage,
               Field::kHelp, Field::kControls},
              {},
              Field::kRect,
              {}};
    case 4:
      return {{Field::kName, Field::kParent, Field::kState, Field::kRect, Field::kMessage, Field::kHelp},
              {{"BACKGRNDCOLOR", false}, {"CURSORSHAPE", false}},
              Field::kRect,
              {}};
    case 8:
      return {{Field::kName, Field::kParent, Field::kState, Field::kMouseFocus, Field::kRect, Field::kSound,
               Field::kMessage, Field::kHelp},
              {},
              Field::kRect,
              {}};
    case 10:
      return {{Field::kName, Field::kParent, Field::kState, Field::kMouseFocus, Field::kRect, Field::kShapeFrame,
               Field::kSound, Field::kMessage, Field::kHelp},
              {},
              Field::kRect,
              {}};
    case 13:
      return {{Field::kName, Field::kRect},
              {{"cX0", false}, {"cY0", false}, {"cX1", false}, {"cY1", false}, {"SHAPEFILE", true}},
              Field::kRect,
              {}};
    case 15:
      return {{Field::kName, Field::kParent, Field::kState, Field::kMouseFocus, Field::kRect, Field::kShapeFrame,
               Field::kDigitFrames, Field::kSound, Field::kMessage, Field::kHelp, Field::kItems},
              {},
              Field::kRect,
              {}};
    case 25:
      return {{Field::kName, Field::kParent, Field::kState, Field::kMouseFocus, Field::kButtonState, Field::kRect,
               Field::kShapeFrame, Field::kSound, Field::kMessage, Field::kHelp, Field::kItems},
              {},
              Field::kRect,
              {}};
    case 0:
    case 5:
    case 6:
    case 7:
    case 9:
    case 11:
    case 12:
    case 14:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 26:
    case 29:
    case 30:
    case 31:
    case 32:
      return {kGeneric, {}, Field::kRect, {}};
    default:
      r.fail(std::format("unknown window TYPE {}", type));
  }
}

void read_extras(TokenReader& r, const std::vector<Extra>& extras, WindowRecord& w) {
  for (const Extra& e : extras) {
    w.extras[std::string(e.key)] = e.string ? r.keyword_word() : std::to_string(r.keyword_integer());
  }
}

WindowRecord read_window(TokenReader& r, int type) {
  const Layout layout = layout_of(type, r);
  WindowRecord w;
  w.type = type;
  for (const Field f : layout.fields) {
    switch (f) {
      case Field::kName:
        w.name = r.keyword_word();
        break;
      case Field::kParent:
        w.parent = r.keyword_word();
        break;
      case Field::kState: {
        const int n = r.keyword_integer();
        for (int i = 0; i < n; ++i) {
          w.screens.push_back(r.integer());
        }
        break;
      }
      case Field::kMouseFocus:
        w.mouse_focus = r.keyword_integer() != 0;
        break;
      case Field::kButtonState:
        w.button_state = r.keyword_integer() != 0;
        break;
      case Field::kRect:
        w.x0 = r.keyword_integer();
        w.y0 = r.keyword_integer();
        w.x1 = r.keyword_integer();
        w.y1 = r.keyword_integer();
        break;
      case Field::kShapeFrame:
        w.shape_frame = r.keyword_integer();
        break;
      case Field::kDigitFrames:
        w.digit_frames = r.keyword_integer();
        break;
      case Field::kSound:
        w.sound = r.keyword_integer();
        break;
      case Field::kMessage:
        w.message = r.keyword_integer();
        w.param1 = r.keyword_integer();
        w.param2 = r.keyword_integer();
        break;
      case Field::kHelp:
        w.help = r.keyword_word();
        break;
      case Field::kItems: {
        const int n = r.keyword_integer();
        for (int i = 0; i < n; ++i) {
          WindowItem item;
          item.kind = r.keyword_integer();
          item.text = r.word();
          item.flags = r.integer();
          item.x = r.integer();
          item.y = r.integer();
          w.items.push_back(std::move(item));
        }
        break;
      }
      case Field::kControls: {
        const int n = r.keyword_integer();
        w.extras["NUMCONTROLS"] = std::to_string(n);
        for (int i = 0; i < n; ++i) {
          std::string key = r.word();
          std::string values;
          for (int v = 0; v < 7; ++v) {
            values += (v ? " " : "") + std::to_string(r.integer());
          }
          w.extras[std::format("RACE{}CONTROL", i + 1)] = std::move(values);
        }
        break;
      }
    }
    if (f == layout.extras_after) {
      read_extras(r, layout.extras, w);
    }
  }
  read_extras(r, layout.trailing, w);
  return w;
}

}  // namespace

WindowsTxt parse_windows_txt(std::span<const std::uint8_t> data) {
  WindowsTxt out;
  TokenReader r(data, "windows.txt");
  out.mem_req = r.keyword_integer();
  for (;;) {
    const int type = r.keyword_integer();
    switch (type) {
      case -1:
        return out;
      case 100: {
        const int screen = r.keyword_integer();
        out.screen_shapes[screen] = r.keyword_word();
        break;
      }
      case 200: {
        const int n = r.keyword_integer();
        for (int i = 0; i < n; ++i) {
          out.haze_tables.push_back(r.keyword_word());
        }
        break;
      }
      case 300:
        out.small_font = r.keyword_word();
        out.large_font = r.keyword_word();
        break;
      default:
        out.windows.push_back(read_window(r, type));
    }
  }
}

}  // namespace oa
