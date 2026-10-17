// Line reading of the text tables the original reads with `fgets` (200-byte
// buffer, DOS text mode): `static.txt`, `help.txt` and the like.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace oa {

// A cursor over the lines of a DOS text file (cp437, CRLF or LF, an optional
// trailing ^Z).
class TextLineReader {
 public:
  // `fgets`' buffer: a longer line comes in pieces of 199 characters.
  static constexpr std::size_t kLineBuffer = 200;

  // `data` must outlive the reader.
  explicit TextLineReader(std::span<const std::uint8_t> data) : data_(data) {}

  // `fgets`: the next line without its line end (a piece of it when longer
  // than the buffer), nothing at the end of the file.
  std::optional<std::string> line();
  // `read_content_line` (0x2f578): the next line not starting with a space,
  // a tab, `/`, `#` or a line end (comments and blank lines); nothing at
  // the end of the file.
  std::optional<std::string> content_line();

 private:
  std::span<const std::uint8_t> data_;
  std::size_t pos_ = 0;
};

}  // namespace oa
