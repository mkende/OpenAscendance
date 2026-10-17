# Cross-compiles the game for Android (API level 24 and up) with the NDK's
# clang, for one ABI: arm64-v8a (the default) or x86_64 (OA_ANDROID_ABI).
# src/app then builds libmain.so for SDL's Java activity.
# Usage (android/build_apk.sh does it, once per ABI of the package):
#   cmake -B build/android/native-arm64-v8a -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/android.cmake \
#     -DOA_ANDROID_NDK=<sdk>/ndk/<version> [-DOA_ANDROID_ABI=x86_64]
# The NDK may also be given by the ANDROID_NDK_HOME environment variable.
if(OA_ANDROID_NDK)
  # Seen by the try_compile projects, which re-read this file.
  set(ENV{OA_ANDROID_NDK} "${OA_ANDROID_NDK}")
elseif(DEFINED ENV{OA_ANDROID_NDK})
  set(OA_ANDROID_NDK "$ENV{OA_ANDROID_NDK}")
elseif(DEFINED ENV{ANDROID_NDK_HOME})
  set(OA_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
else()
  message(FATAL_ERROR "android.cmake: set OA_ANDROID_NDK (or ANDROID_NDK_HOME) to the NDK directory")
endif()
if(OA_ANDROID_ABI)
  set(ENV{OA_ANDROID_ABI} "${OA_ANDROID_ABI}")
elseif(DEFINED ENV{OA_ANDROID_ABI})
  set(OA_ANDROID_ABI "$ENV{OA_ANDROID_ABI}")
else()
  set(OA_ANDROID_ABI arm64-v8a)
endif()
if(NOT OA_ANDROID_ABI MATCHES "^(arm64-v8a|x86_64)$")
  message(FATAL_ERROR "android.cmake: OA_ANDROID_ABI must be arm64-v8a or x86_64, not '${OA_ANDROID_ABI}'")
endif()

set(ANDROID_ABI ${OA_ANDROID_ABI})
set(ANDROID_PLATFORM android-24)
set(ANDROID_STL c++_static)
include("${OA_ANDROID_NDK}/build/cmake/android.toolchain.cmake")
