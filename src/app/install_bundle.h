// The game files embedded in the Android package of the local Ascendancy
// edition (CMake option OA_EDITION, android/build_apk.sh): the install's files
// under the APK's "bundle" asset directory, listed by the "bundle_index.txt"
// asset. `Install` reads host files only, so they are copied into a host
// folder on the first start (and again when the package's bundle changes).
// Free of SDL, for the tests; app/main.cpp reads the assets.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace oa {

// The asset listing the bundle, and the asset directory of its files.
inline constexpr std::string_view kBundleIndexName = "bundle_index.txt";
inline constexpr std::string_view kBundleDir = "bundle";

// A file of the bundle: its path relative to the bundle ('/' separators) and
// size.
struct BundleFile {
  std::string path;
  std::uint64_t size = 0;
};

// Returns the files of bundle index `index`, a text:
//
//   OA-BUNDLE 1
//   <size> <path>
//   ...
//
// Throws `DataError` when it is malformed, or a path is absolute or has a
// ".." component.
std::vector<BundleFile> parse_bundle_index(std::string_view index);

// Copies the bundle's file `path` to host file `to` (created or replaced);
// throws `DataError` or `std::runtime_error` on failure.
using BundleCopier = std::function<void(const std::string& path, const std::filesystem::path& to)>;

// Makes host folder `dest` a copy of the bundle of index `index`: nothing to
// do when `dest` already holds this index (as `kBundleIndexName`) and every
// file of it at its size; else every file is copied with `copy` into a
// temporary folder beside `dest`, which then replaces it. Returns whether
// the files were copied. Throws `DataError` on a malformed index or a copied
// file of another size than listed, `std::runtime_error` when the folder
// cannot be written (no temporary folder left behind).
bool update_bundle_copy(std::string_view index, const BundleCopier& copy, const std::filesystem::path& dest);

}  // namespace oa
