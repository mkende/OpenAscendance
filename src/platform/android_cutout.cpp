#include "platform/android_cutout.h"

#include <SDL3/SDL_system.h>
#include <jni.h>

#include "core/log.h"

namespace oa {

namespace {

// GameActivity's `cutoutInsets`, looked up once from SDL's main thread.
struct Bridge {
  jclass activity = nullptr;
  jmethodID cutout_insets = nullptr;
};

Bridge make_bridge() {
  Bridge b;
  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
  if (env == nullptr || activity == nullptr) {
    log_warning("no Android activity: the display cutout is ignored");
    return b;
  }
  const jclass local = env->GetObjectClass(activity);
  env->DeleteLocalRef(activity);
  b.activity = static_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  b.cutout_insets = env->GetStaticMethodID(b.activity, "cutoutInsets", "()[I");
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    b.cutout_insets = nullptr;
    log_warning("GameActivity has no cutoutInsets: the display cutout is ignored");
  }
  return b;
}

}  // namespace

std::array<int, 4> android_cutout_insets() {
  static const Bridge bridge = make_bridge();
  std::array<int, 4> insets{};
  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  if (bridge.cutout_insets == nullptr || env == nullptr) {
    return insets;
  }
  auto array = static_cast<jintArray>(env->CallStaticObjectMethod(bridge.activity, bridge.cutout_insets));
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return insets;
  }
  if (array != nullptr && env->GetArrayLength(array) == 4) {
    env->GetIntArrayRegion(array, 0, 4, insets.data());
  }
  if (array != nullptr) {
    env->DeleteLocalRef(array);
  }
  return insets;
}

}  // namespace oa
