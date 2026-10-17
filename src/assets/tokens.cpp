#include "assets/tokens.h"

#include <charconv>
#include <format>

#include "core/error.h"

namespace oa {

namespace {

bool is_space(std::uint8_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

}  // namespace

TokenReader::TokenReader(std::span<const std::uint8_t> data, std::string_view context)
    : data_(data), context_(context) {
  // The DOS end-of-file mark ends the text.
  for (std::size_t i = 0; i < data_.size(); ++i) {
    if (data_[i] == 0x1a) {
      data_ = data_.first(i);
      break;
    }
  }
}

void TokenReader::skip_space() {
  while (pos_ < data_.size() && is_space(data_[pos_])) {
    ++pos_;
  }
}

bool TokenReader::at_end() {
  skip_space();
  return pos_ == data_.size();
}

std::string TokenReader::word() {
  if (at_end()) {
    fail("unexpected end of the text");
  }
  std::string out;
  while (pos_ < data_.size() && !is_space(data_[pos_])) {
    out += static_cast<char>(data_[pos_++]);
  }
  return out;
}

int TokenReader::integer() {
  const std::string token = word();
  int v = 0;
  const char* begin = token.data();
  if (*begin == '+') {
    ++begin;
  }
  const auto [end, ec] = std::from_chars(begin, token.data() + token.size(), v);
  if (ec != std::errc{} || end != token.data() + token.size()) {
    fail(std::format("'{}' is not a number", token));
  }
  return v;
}

std::string TokenReader::keyword_word() {
  word();
  return word();
}

int TokenReader::keyword_integer() {
  word();
  return integer();
}

void TokenReader::fail(std::string_view message) const {
  throw DataError(std::format("{} @{}: {}", context_, pos_, message));
}

std::string replace_with_spaces(std::string_view s, char c) {
  std::string out(s);
  for (char& ch : out) {
    if (ch == c) {
      ch = ' ';
    }
  }
  return out;
}

}  // namespace oa
