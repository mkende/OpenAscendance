#include "assets/help_txt.h"

#include <cctype>
#include <format>
#include <vector>

#include "assets/text_lines.h"
#include "core/error.h"

namespace oa {

namespace {

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

// The whitespace-separated tokens of `line` (`sscanf("%s ...")`).
std::vector<std::string> tokens(std::string_view line) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
      ++i;
    }
    const std::size_t start = i;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) == 0) {
      ++i;
    }
    if (i > start) {
      out.emplace_back(line.substr(start, i - start));
    }
  }
  return out;
}

// `%d` of `token`: an optional sign and the digits that follow (nothing
// when there are none).
std::optional<int> scan_int(std::string_view token) {
  std::size_t i = 0;
  bool negative = false;
  if (i < token.size() && (token[i] == '-' || token[i] == '+')) {
    negative = token[i] == '-';
    ++i;
  }
  if (i >= token.size() || std::isdigit(static_cast<unsigned char>(token[i])) == 0) {
    return std::nullopt;
  }
  int v = 0;
  for (; i < token.size() && std::isdigit(static_cast<unsigned char>(token[i])) != 0; ++i) {
    v = v * 10 + (token[i] - '0');
  }
  return negative ? -v : v;
}

// The integers of `t` after the keyword, as many as `sscanf("%s %d ...")`
// converts (it stops at the first that is not a number).
std::vector<int> scan_ints(const std::vector<std::string>& t, std::size_t count) {
  std::vector<int> out;
  for (std::size_t i = 1; i < t.size() && out.size() < count; ++i) {
    const std::optional<int> v = scan_int(t[i]);
    if (!v) {
      break;
    }
    out.push_back(*v);
  }
  return out;
}

}  // namespace

HelpTopic parse_help_topic(std::span<const std::uint8_t> data, std::string_view topic, std::string_view context,
                           int center_x, int center_y) {
  TextLineReader reader(data);
  const auto next = [&]() {
    std::optional<std::string> line = reader.content_line();
    if (!line) {
      throw DataError(std::format("{}: unexpected end of file in topic '{}'", context, topic));
    }
    return *line;
  };
  // The topic's line.
  for (;;) {
    const std::vector<std::string> t = tokens(next());
    if (t.empty() || !iequals(t[0], "HELP")) {
      continue;
    }
    const std::string_view name = t.size() > 1 ? std::string_view(t[1]) : std::string_view();
    if (iequals(name, topic)) {
      break;
    }
    if (iequals(name, "END")) {
      throw DataError(std::format("{}: Topic '{}' not found in help file.", context, topic));
    }
  }

  HelpTopic out;
  int current = 0;   // the element slot the modifiers change
  bool any = false;  // whether an element was created
  int texts = 0;
  int button = 0;
  bool any_button = false;
  const auto new_element = [&]() -> HelpElement& {
    if (++out.element_count > HelpTopic::kMaxElements) {
      throw DataError(std::format("{}: topic '{}' has more than {} elements", context, topic, HelpTopic::kMaxElements));
    }
    if (any) {
      ++current;
    }
    any = true;
    return out.elements[static_cast<std::size_t>(current)];
  };
  for (;;) {
    const std::string line = next();
    const std::vector<std::string> t = tokens(line);
    if (t.empty()) {
      continue;
    }
    const std::string_view key = t[0];
    HelpElement& e = out.elements[static_cast<std::size_t>(current)];
    HelpButton& b = out.buttons[static_cast<std::size_t>(button)];
    if (iequals(key, "HELP")) {
      return out;
    }
    if (iequals(key, "POS")) {
      const std::vector<int> v = scan_ints(t, 2);
      if (!v.empty()) {
        e.x = v[0];
      }
      if (v.size() > 1) {
        e.y = v[1];
      }
    } else if (iequals(key, "SHAPE")) {
      HelpElement& s = new_element();
      s.kind = HelpElement::Kind::kShape;
      s.shape_index = 0;
      s.frame = 0;
      s.x = 0;
      s.y = 0;
      if (const std::vector<int> v = scan_ints(t, 4); v.size() == 4) {
        s.shape_index = v[0];
        s.frame = v[1];
        s.x = v[2];
        s.y = v[3];
      }
    } else if (iequals(key, "BUTTON")) {
      if (any_button) {
        ++button;
      }
      any_button = true;
      if (button >= HelpTopic::kMaxButtons) {
        throw DataError(std::format("{}: topic '{}' has more than {} buttons", context, topic, HelpTopic::kMaxButtons));
      }
      out.buttons[static_cast<std::size_t>(button)].shown = true;
    } else if (iequals(key, "TITLE")) {
      if (t.size() > 1) {
        b.title = t[1];
      }
    } else if (iequals(key, "MSG")) {
      if (const std::vector<int> v = scan_ints(t, 1); !v.empty()) {
        b.message = v[0];
      }
    } else if (iequals(key, "PANE")) {
      if (const std::vector<int> v = scan_ints(t, 4); v.size() == 4) {
        b.pane = Pane{v[0], v[1], v[2], v[3]};
      }
    } else if (iequals(key, "BPOS")) {
      if (t.size() > 1 && iequals(t[1], "BL")) {
        b.corner = 'L';
      } else if (t.size() > 1 && iequals(t[1], "BR")) {
        b.corner = 'R';
      }
    } else if (iequals(key, "TEXT")) {
      if (texts == HelpTopic::kMaxTexts) {
        throw DataError(std::format("{}: topic '{}' has more than {} texts", context, topic, HelpTopic::kMaxTexts));
      }
      ++texts;
      HelpElement& s = new_element();
      s.kind = HelpElement::Kind::kText;
      s.flags = 0;
      s.text.clear();
      s.small_font = false;
      s.x = 0;
      s.y = 0;
      s.color = -1;
      if (const std::vector<int> v = scan_ints(t, 3); v.size() == 3) {
        s.x = v[0];
        s.y = v[1];
        s.color = v[2];
      }
      // The lines as `fgets` returns them (with their line end) up to
      // ENDTEXT.
      int length = 0;
      for (;;) {
        const std::string l = next();
        if (l.size() >= 7 && iequals(std::string_view(l).substr(0, 7), "ENDTEXT")) {
          break;
        }
        length += static_cast<int>(l.size()) + 1;
        if (length > HelpTopic::kMaxText) {
          throw DataError(std::format("{}: a text of topic '{}' is too long", context, topic));
        }
        s.text += l;
        s.text += '\n';
      }
      for (char& c : s.text) {
        if (c == '\n') {
          c = ' ';
        } else if (c == '@') {
          c = '\n';
        }
      }
    } else if (iequals(key, "CENTERPOS")) {
      e.x = center_x;
      e.y = center_y;
    } else if (iequals(key, "FONT")) {
      if (t.size() > 1 && iequals(t[1], "LARGE")) {
        e.small_font = false;
      } else if (t.size() > 1 && iequals(t[1], "SMALL")) {
        e.small_font = true;
      }
    } else if (iequals(key, "FLAGS")) {
      // sscanf("%s %s %s %s %s %s"): at most five flags.
      for (std::size_t i = 1; i < t.size() && i < 6; ++i) {
        if (iequals(t[i], "TJ_HC")) {
          e.flags |= 2;
        } else if (iequals(t[i], "TJ_VC")) {
          e.flags |= 1;
        } else if (iequals(t[i], "TJ_R")) {
          e.flags |= 0x20;
        } else if (iequals(t[i], "TJ_B")) {
          e.flags |= 0x10;
        }
      }
    } else if (iequals(key, "NEWPAGE")) {
      e.page_end = true;
      ++out.pages;
    }
  }
}

}  // namespace oa
