#include "app/install_bundle.h"

#include <charconv>
#include <format>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <system_error>

#include "core/error.h"
#include "core/install.h"

namespace oa {

namespace {

constexpr std::string_view kHeader = "OA-BUNDLE 1";

// Returns whether `path` is relative and stays in the bundle: no empty, "."
// or ".." component.
bool safe_relative(std::string_view path) {
  if (path.empty() || path.front() == '/' || path.back() == '/') {
    return false;
  }
  std::size_t start = 0;
  for (;;) {
    const std::size_t slash = path.find('/', start);
    const std::string_view part = path.substr(start, slash == std::string_view::npos ? path.npos : slash - start);
    if (part.empty() || part == "." || part == ".." || part.find('\\') != std::string_view::npos) {
      return false;
    }
    if (slash == std::string_view::npos) {
      return true;
    }
    start = slash + 1;
  }
}

// Returns the size of host file `path`, nothing when it is not a regular file.
std::optional<std::uint64_t> regular_file_size(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return std::nullopt;
  }
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    return std::nullopt;
  }
  return size;
}

// Returns whether `dest` holds index `index` and its files at their sizes.
bool copy_is_current(std::string_view index, const std::vector<BundleFile>& files, const std::filesystem::path& dest) {
  const std::filesystem::path saved = dest / std::string(kBundleIndexName);
  if (!regular_file_size(saved)) {
    return false;
  }
  std::vector<std::uint8_t> text;
  try {
    text = read_file(saved);
  } catch (const DataError&) {
    return false;
  }
  if (std::string_view(reinterpret_cast<const char*>(text.data()), text.size()) != index) {
    return false;
  }
  for (const BundleFile& f : files) {
    if (regular_file_size(dest / f.path) != f.size) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::vector<BundleFile> parse_bundle_index(std::string_view index) {
  std::vector<BundleFile> files;
  bool header = true;
  while (!index.empty()) {
    const std::size_t eol = index.find('\n');
    std::string_view line = index.substr(0, eol);
    index = eol == std::string_view::npos ? std::string_view() : index.substr(eol + 1);
    if (header) {
      if (line != kHeader) {
        throw DataError(std::format("not a bundle index (no {} header)", kHeader));
      }
      header = false;
      continue;
    }
    const std::size_t space = line.find(' ');
    BundleFile f;
    const char* end = line.data() + (space == std::string_view::npos ? line.size() : space);
    const auto [ptr, ec] = std::from_chars(line.data(), end, f.size);
    if (space == std::string_view::npos || ec != std::errc() || ptr != end) {
      throw DataError(std::format("bundle index: malformed line '{}'", line));
    }
    f.path = std::string(line.substr(space + 1));
    if (!safe_relative(f.path)) {
      throw DataError(std::format("bundle index: bad path '{}'", f.path));
    }
    files.push_back(std::move(f));
  }
  if (header) {
    throw DataError(std::format("not a bundle index (no {} header)", kHeader));
  }
  return files;
}

bool update_bundle_copy(std::string_view index, const BundleCopier& copy, const std::filesystem::path& dest) {
  const std::vector<BundleFile> files = parse_bundle_index(index);
  if (copy_is_current(index, files, dest)) {
    return false;
  }
  const std::filesystem::path temp = dest.string() + ".tmp";
  std::filesystem::remove_all(temp);
  try {
    for (const BundleFile& f : files) {
      const std::filesystem::path to = temp / f.path;
      std::filesystem::create_directories(to.parent_path());
      copy(f.path, to);
      if (regular_file_size(to) != f.size) {
        throw DataError(std::format("bundle: {} is not of the size listed ({} bytes)", f.path, f.size));
      }
    }
    {
      std::ofstream out(temp / std::string(kBundleIndexName), std::ios::binary | std::ios::trunc);
      out.write(index.data(), static_cast<std::streamsize>(index.size()));
      if (!out) {
        throw std::runtime_error(std::format("cannot write {}", (temp / std::string(kBundleIndexName)).string()));
      }
    }
    std::filesystem::remove_all(dest);
    std::filesystem::rename(temp, dest);
  } catch (const std::runtime_error&) {
    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    throw;
  }
  return true;
}

}  // namespace oa
