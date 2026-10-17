#include "app/install_source.h"

#include <format>
#include <fstream>
#include <stdexcept>

#include "assets/cfile.h"
#include "core/error.h"

namespace oa {

namespace {

constexpr std::string_view kHeader = "OA-INSTALL 1";
constexpr std::string_view kFolder = "folder ";

}  // namespace

std::optional<std::filesystem::path> load_install_config(const std::filesystem::path& file) {
  std::error_code ec;
  if (!std::filesystem::exists(file, ec)) {
    return std::nullopt;
  }
  const std::vector<std::uint8_t> data = read_file(file);
  std::string_view text(reinterpret_cast<const char*>(data.data()), data.size());
  const auto next_line = [&text] {
    const std::size_t eol = text.find('\n');
    std::string_view line = text.substr(0, eol);
    text = eol == std::string_view::npos ? std::string_view() : text.substr(eol + 1);
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    return line;
  };
  if (next_line() != kHeader) {
    throw DataError(std::format("{}: not an install configuration", file.string()));
  }
  const std::string_view line = next_line();
  if (!line.starts_with(kFolder) || line.size() == kFolder.size()) {
    throw DataError(std::format("{}: not an install configuration", file.string()));
  }
  return std::filesystem::path(std::string(line.substr(kFolder.size())));
}

void save_install_config(const std::filesystem::path& file, const std::filesystem::path& folder) {
  const std::string location = folder.string();
  if (location.find_first_of("\r\n") != std::string::npos) {
    throw std::runtime_error(std::format("cannot remember the folder '{}'", location));
  }
  std::error_code ec;
  std::filesystem::create_directories(file.parent_path(), ec);
  // Through a temporary file: an interrupted write keeps the old choice.
  std::filesystem::path tmp = file;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << kHeader << '\n' << kFolder << location << '\n';
    if (!out) {
      throw std::runtime_error(std::format("cannot write {}", tmp.string()));
    }
  }
  std::filesystem::rename(tmp, file, ec);
  if (ec) {
    throw std::runtime_error(std::format("cannot write {}: {}", file.string(), ec.message()));
  }
}

InstallCheck check_install_folder(const std::filesystem::path& dir) {
  try {
    Install install(dir);
    // The archives must open too.
    const CobDirectory directory(install);
    return {std::move(install), {}};
  } catch (const DataError& e) {
    return {std::nullopt, e.what()};
  }
}

StartupInstall choose_startup_install(const std::optional<std::filesystem::path>& data_dir, bool force_setup,
                                      const std::optional<std::filesystem::path>& remembered) {
  StartupInstall out;
  if (force_setup) {
    return out;
  }
  for (const auto& candidate : {data_dir, remembered}) {
    if (!candidate) {
      continue;
    }
    InstallCheck check = check_install_folder(*candidate);
    if (check.install) {
      out.install = std::move(check.install);
      return out;
    }
    out.problems.push_back(check.problem);
  }
  return out;
}

}  // namespace oa
