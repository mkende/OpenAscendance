// Reading of the token tables the original parses with the Watcom CRT's
// `fscanf("%s")` / `fscanf("%d")` (`docs/02-data-formats.md`): whitespace-
// separated tokens of DOS text (cp437, CRLF, an optional trailing ^Z).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace oa {

// A cursor over the tokens of a text file. Reads past the end, or a `%d`
// on a token that is not a number, throw `DataError` naming the context.
class TokenReader {
 public:
  // `data` must outlive the reader.
  explicit TokenReader(std::span<const std::uint8_t> data, std::string_view context = "table");

  // Returns whether only whitespace (or ^Z) is left.
  bool at_end();
  // Returns the next token (`%s`).
  std::string word();
  // Returns the next token as a decimal `int` (`%d`: optional sign, digits;
  // the whole token must be a number).
  int integer();
  // Skips a keyword token and returns the value token after it (`%s %s`).
  std::string keyword_word();
  // Skips a keyword token and returns the integer after it (`%s %d`).
  int keyword_integer();

  // Throws `DataError` with `message`, prefixed by the context.
  [[noreturn]] void fail(std::string_view message) const;

 private:
  void skip_space();

  std::span<const std::uint8_t> data_;
  std::string context_;
  std::size_t pos_ = 0;
};

// Returns `s` with every `c` replaced by a space (`^` and `_` stand for
// spaces in the token tables).
std::string replace_with_spaces(std::string_view s, char c);

}  // namespace oa
