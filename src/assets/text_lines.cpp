#include "assets/text_lines.h"

namespace oa {

std::optional<std::string> TextLineReader::line() {
  if (pos_ >= data_.size() || data_[pos_] == 0x1a) {
    return std::nullopt;
  }
  std::string out;
  while (pos_ < data_.size() && out.size() < kLineBuffer - 1) {
    const char c = static_cast<char>(data_[pos_++]);
    if (c == '\n') {
      return out;
    }
    if (c == 0x1a) {
      pos_ = data_.size();
      return out;
    }
    // Text mode drops the CR of CRLF.
    if (c != '\r') {
      out += c;
    }
  }
  return out;
}

std::optional<std::string> TextLineReader::content_line() {
  while (std::optional<std::string> l = line()) {
    if (!l->empty() && l->front() != ' ' && l->front() != '\t' && l->front() != '/' && l->front() != '#') {
      return l;
    }
  }
  return std::nullopt;
}

}  // namespace oa
