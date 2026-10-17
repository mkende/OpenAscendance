// The Android side of the install setup (app/install_setup.h): the system
// folder picker and the files of the folder chosen, read through the Storage
// Access Framework, by the static methods of
// android/java/.../GameActivity.java (adapted from SilentHeritage's
// app/android_documents.h). The game reads a copy of the files in the app's
// storage (`Install` needs host files). Call from SDL_main: needs SDL's
// Android activity.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace oa {

// Shows the system folder picker and waits until it is closed; returns the
// tree URI chosen (its read permission persisted), nothing when cancelled.
// Throws `std::runtime_error` when the activity cannot be reached.
std::optional<std::string> pick_android_folder();

// A file or directory of a document tree.
struct AndroidDocument {
  std::string name;
  std::string id;  // the document id
  bool directory = false;
  std::uint64_t size = 0;  // of a file
};

// Returns the entries at the top of the folder of tree `tree_uri`, in any
// order. Throws `DataError` when it is not a tree URI or cannot be listed.
std::vector<AndroidDocument> list_android_folder(const std::string& tree_uri);

// Copies `files` (entries of `list_android_folder(tree_uri)`) into host
// directory `dest` (created; files replaced), calling `progress` with the
// number of bytes copied so far after each file. Throws `DataError` when a
// file cannot be read or has another size than listed, `std::runtime_error`
// when one cannot be written.
void copy_android_documents(const std::string& tree_uri, std::span<const AndroidDocument> files,
                            const std::filesystem::path& dest, const std::function<void(std::uint64_t)>& progress);

}  // namespace oa
