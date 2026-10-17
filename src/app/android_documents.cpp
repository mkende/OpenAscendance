#include "app/android_documents.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_system.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <format>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "core/error.h"

namespace oa {

namespace {

// GameActivity's static methods. Looked up once from SDL's main thread:
// FindClass on threads attached from native code does not see the app's
// classes, while the global class reference works on any thread.
struct Bridge {
  jclass activity = nullptr;
  jmethodID pick_folder = nullptr;
  jmethodID tree_root = nullptr;
  jmethodID list_documents = nullptr;
  jmethodID open_document = nullptr;
};

// Returns the JNI environment of the calling thread (attached by SDL).
JNIEnv* jni_env() {
  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  if (env == nullptr) {
    throw std::runtime_error(std::format("no JNI environment: {}", SDL_GetError()));
  }
  return env;
}

Bridge make_bridge() {
  JNIEnv* env = jni_env();
  auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
  if (activity == nullptr) {
    throw std::runtime_error(std::format("no Android activity: {}", SDL_GetError()));
  }
  const jclass local = env->GetObjectClass(activity);
  env->DeleteLocalRef(activity);
  Bridge b;
  b.activity = static_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  b.pick_folder = env->GetStaticMethodID(b.activity, "pickFolder", "()Ljava/lang/String;");
  b.tree_root = env->GetStaticMethodID(b.activity, "documentTreeRoot", "(Ljava/lang/String;)Ljava/lang/String;");
  b.list_documents =
      env->GetStaticMethodID(b.activity, "listDocuments", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
  b.open_document = env->GetStaticMethodID(b.activity, "openDocument", "(Ljava/lang/String;Ljava/lang/String;)I");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    throw std::runtime_error("GameActivity lacks the methods of the install setup");
  }
  return b;
}

const Bridge& bridge() {
  static const Bridge b = make_bridge();
  return b;
}

// Frees the local references made in its scope (threads attached from
// native code never return to Java, where they would be freed).
class LocalFrame {
 public:
  explicit LocalFrame(JNIEnv* env) : env_(env) { env_->PushLocalFrame(16); }
  ~LocalFrame() { env_->PopLocalFrame(nullptr); }
  LocalFrame(const LocalFrame&) = delete;
  LocalFrame& operator=(const LocalFrame&) = delete;

 private:
  JNIEnv* env_;
};

// Returns whether a Java exception was thrown (logged and cleared).
bool java_failed(JNIEnv* env) {
  if (!env->ExceptionCheck()) {
    return false;
  }
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

// Returns the content of `s`, empty for null.
std::string to_string(JNIEnv* env, jstring s) {
  if (s == nullptr) {
    return {};
  }
  const char* chars = env->GetStringUTFChars(s, nullptr);
  std::string out(chars);
  env->ReleaseStringUTFChars(s, chars);
  return out;
}

// Reads file descriptor `fd` to its end and closes it; nothing on a read
// error.
std::optional<std::vector<std::uint8_t>> read_fd(int fd) {
  std::vector<std::uint8_t> data;
  struct stat st{};
  if (fstat(fd, &st) == 0 && st.st_size > 0) {
    data.reserve(static_cast<std::size_t>(st.st_size));
  }
  std::vector<std::uint8_t> buffer(std::size_t{1} << 16);
  for (;;) {
    const ssize_t n = ::read(fd, buffer.data(), buffer.size());
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n < 0) {
      ::close(fd);
      return std::nullopt;
    }
    if (n == 0) {
      break;
    }
    data.insert(data.end(), buffer.begin(), buffer.begin() + n);
  }
  ::close(fd);
  return data;
}

// Returns the document id of the root of tree `tree_uri`; throws `DataError`
// when it is not a tree URI.
std::string tree_root(JNIEnv* env, const std::string& tree_uri) {
  const LocalFrame frame(env);
  const auto root = static_cast<jstring>(
      env->CallStaticObjectMethod(bridge().activity, bridge().tree_root, env->NewStringUTF(tree_uri.c_str())));
  if (java_failed(env) || root == nullptr) {
    throw DataError(std::format("{} is not a folder", tree_uri));
  }
  return to_string(env, root);
}

// Returns the content of document `id` of tree `tree_uri`, nothing when it
// cannot be read.
std::optional<std::vector<std::uint8_t>> read_document(const std::string& tree_uri, const std::string& id) {
  JNIEnv* env = jni_env();
  jint fd = -1;
  {
    const LocalFrame frame(env);
    fd = env->CallStaticIntMethod(bridge().activity, bridge().open_document, env->NewStringUTF(tree_uri.c_str()),
                                  env->NewStringUTF(id.c_str()));
    if (java_failed(env)) {
      return std::nullopt;
    }
  }
  if (fd < 0) {
    return std::nullopt;
  }
  return read_fd(fd);
}

}  // namespace

std::optional<std::string> pick_android_folder() {
  JNIEnv* env = jni_env();
  const LocalFrame frame(env);
  const auto uri = static_cast<jstring>(env->CallStaticObjectMethod(bridge().activity, bridge().pick_folder));
  if (java_failed(env)) {
    throw std::runtime_error("the folder picker failed");
  }
  if (uri == nullptr) {
    return std::nullopt;
  }
  return to_string(env, uri);
}

std::vector<AndroidDocument> list_android_folder(const std::string& tree_uri) {
  JNIEnv* env = jni_env();
  const std::string root = tree_root(env, tree_uri);
  const LocalFrame frame(env);
  const auto list = static_cast<jobjectArray>(env->CallStaticObjectMethod(bridge().activity, bridge().list_documents,
                                                                          env->NewStringUTF(tree_uri.c_str()),
                                                                          env->NewStringUTF(root.c_str())));
  if (java_failed(env) || list == nullptr) {
    throw DataError(std::format("cannot list the folder {} (permission revoked or folder removed?)", tree_uri));
  }
  const jsize n = env->GetArrayLength(list);
  std::vector<AndroidDocument> out;
  for (jsize i = 0; i + 3 < n; i += 4) {
    // The strings of each entry are released before the next ones are made,
    // within the frame's capacity.
    const LocalFrame entry_frame(env);
    AndroidDocument d;
    d.name = to_string(env, static_cast<jstring>(env->GetObjectArrayElement(list, i)));
    d.id = to_string(env, static_cast<jstring>(env->GetObjectArrayElement(list, i + 1)));
    d.directory = to_string(env, static_cast<jstring>(env->GetObjectArrayElement(list, i + 2))) == "d";
    const std::string size = to_string(env, static_cast<jstring>(env->GetObjectArrayElement(list, i + 3)));
    std::from_chars(size.data(), size.data() + size.size(), d.size);
    out.push_back(std::move(d));
  }
  return out;
}

void copy_android_documents(const std::string& tree_uri, std::span<const AndroidDocument> files,
                            const std::filesystem::path& dest, const std::function<void(std::uint64_t)>& progress) {
  std::error_code ec;
  std::filesystem::create_directories(dest, ec);
  std::uint64_t done = 0;
  for (const AndroidDocument& f : files) {
    // A name that is not a plain file name would land outside `dest`.
    if (f.name.empty() || f.name == "." || f.name == ".." || f.name.find('/') != std::string::npos) {
      throw DataError(std::format("{}: unusable file name '{}'", tree_uri, f.name));
    }
    const std::optional<std::vector<std::uint8_t>> data = read_document(tree_uri, f.id);
    if (!data) {
      throw DataError(std::format("cannot read {} (in {})", f.name, tree_uri));
    }
    if (data->size() != f.size) {
      throw DataError(std::format("{} (in {}): {} bytes, the listing says {}", f.name, tree_uri, data->size(), f.size));
    }
    const std::filesystem::path target = dest / f.name;
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
    out.close();
    if (!out) {
      throw std::runtime_error(std::format("cannot write {}", target.string()));
    }
    done += f.size;
    progress(done);
  }
}

}  // namespace oa
