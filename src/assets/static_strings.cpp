#include "assets/static_strings.h"

#include <optional>

#include "assets/text_lines.h"
#include "core/error.h"

namespace oa {

std::vector<std::string> parse_static_strings(std::span<const std::uint8_t> data) {
  TextLineReader reader(data);
  std::vector<std::string> out;
  for (;;) {
    std::optional<std::string> line = reader.content_line();
    if (!line) {
      throw DataError("static.txt: no 'end' line");
    }
    if (*line == "end") {
      return out;
    }
    if (line->front() != '"') {
      continue;
    }
    std::string s;
    std::size_t i = 1;
    for (;;) {
      if (i >= line->size()) {
        // The string goes on on the next line.
        line = reader.line();
        if (!line) {
          throw DataError("static.txt: unterminated string");
        }
        i = 0;
        continue;
      }
      const char c = (*line)[i++];
      if (c == '"') {
        break;
      }
      if (c == '\\') {
        if (i >= line->size()) {
          continue;
        }
        const char e = (*line)[i++];
        s += e == 'n' ? '\n' : e;
      } else {
        s += c;
      }
    }
    out.push_back(std::move(s));
  }
}

}  // namespace oa
