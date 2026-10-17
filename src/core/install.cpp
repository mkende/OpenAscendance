#include "core/install.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <string>

#include "core/error.h"

namespace oa {

namespace {

std::string upper(std::string_view s) {
  std::string out(s);
  std::ranges::transform(out, out.begin(),
                         [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
  return out;
}

// Splits a DOS path into its components, dropping empty and "." ones.
std::vector<std::string> split_dos_path(std::string_view dos_path) {
  std::vector<std::string> parts;
  std::string component;
  const auto flush = [&] {
    if (!component.empty() && component != ".") {
      parts.push_back(std::move(component));
    }
    component.clear();
  };
  for (const char c : dos_path) {
    if (c == '/' || c == '\\') {
      flush();
    } else {
      component += c;
    }
  }
  flush();
  return parts;
}

// Finds the entry of `dir` whose name matches `name` case-insensitively.
std::optional<std::filesystem::path> find_entry(const std::filesystem::path& dir, std::string_view name) {
  std::error_code ec;
  const auto exact = dir / std::string(name);
  if (std::filesystem::exists(exact, ec)) {
    return exact;
  }
  const std::string wanted = upper(name);
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (upper(entry.path().filename().string()) == wanted) {
      return entry.path();
    }
  }
  return std::nullopt;
}

}  // namespace

Install::Install(std::filesystem::path root) : root_(std::move(root)) {
  if (!find(kMarkerFile)) {
    throw DataError(std::format("{} is not an Ascendancy install (no {})", root_.string(), kMarkerFile));
  }
}

std::optional<std::filesystem::path> Install::find(std::string_view dos_path) const {
  std::filesystem::path current = root_;
  for (const std::string& name : split_dos_path(dos_path)) {
    if (name == "..") {
      return std::nullopt;
    }
    auto next = find_entry(current, name);
    if (!next) {
      return std::nullopt;
    }
    current = std::move(*next);
  }
  return current;
}

bool Install::exists(std::string_view dos_path) const { return find(dos_path).has_value(); }

std::optional<std::vector<std::uint8_t>> Install::try_read(std::string_view dos_path) const {
  const auto path = find(dos_path);
  if (!path) {
    return std::nullopt;
  }
  return read_file(*path);
}

std::vector<std::uint8_t> Install::read(std::string_view dos_path) const {
  auto data = try_read(dos_path);
  if (!data) {
    throw DataError(std::format("{}: no file {}", root_.string(), dos_path));
  }
  return std::move(*data);
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw DataError(std::format("cannot open {}", path.string()));
  }
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) {
    throw DataError(std::format("cannot read {}", path.string()));
  }
  return data;
}

}  // namespace oa
