# Building Open Ascendance

How to build Open Ascendance for each platform and make its packages
(adapted from SilentHeritage's). The [README](README.md) has the native
build, the command line and the tests; [RELEASING.md](RELEASING.md)
describes how the release packages are made and published.

The game never ships the original files: every build reads the player's
own Ascendancy install at run time (the install setup asks for its folder
on the first start).

## Linux

CMake 3.25 or later, Ninja, clang (and lld for the release packages), git;
SDL3 also needs the usual X11 or Wayland, audio and GL development packages
(`packaging/linux/install_build_deps.sh` lists them).

    cmake -S . -B build/debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    ninja -C build/debug
    ASAN_OPTIONS=detect_leaks=0 OA_DATA=<install dir> ctest --test-dir build/debug -j <processors>

`-DFETCHCONTENT_SOURCE_DIR_SDL3=<dir>` (and `_CATCH2`) reuses a checkout
instead of downloading it. `-DOA_VERSION=` sets the version
(`MAJOR.MINOR.PATCH[-suffix]`, `0.0.0-dev` by default).

## Windows

Cross-compiled from Linux with clang, lld and the mingw-w64 runtime
(`packaging/windows/install_build_deps.sh`), without the tests (Catch2's
test discovery runs the test program at build time):

    cmake -S . -B build/windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-clang.cmake \
      -DCMAKE_BUILD_TYPE=Release -DOA_BUILD_TESTS=OFF
    ninja -C build/windows open_ascendance

The executable is self-contained (no DLL beyond Windows' own), windowed
(started from a console, it writes to that console), with the icon of
`packaging/icon/icon.ico` (`-DOA_WINDOWS_ICON=`), its version and its
manifest in its resources. Under WSL it runs directly:

    build/windows/src/app/open_ascendance.exe --data "$(wslpath -w <install dir>)" \
      --screenshot "$(wslpath -w /tmp)\\shot.bmp" --frames 200

## macOS

Apple's clang and Ninja (`brew install ninja`), for macOS 13.3 or later:

    cmake -S . -B build/debug -G Ninja -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3
    ninja -C build/debug

The app bundle and the disk image are made by
`packaging/build_desktop.sh macos` (below), with a universal executable
(arm64 and x86_64).

## Android

[`android/build_apk.sh`](android/build_apk.sh) builds a package for
arm64-v8a and x86_64 (Android 7.0 or later) without Gradle: CMake with the
NDK for the native library (`cmake/android.cmake`, `libmain.so` loaded by
SDL's Java activity), javac and d8 for SDL's activity and ours
(`android/java`), aapt2, zipalign and apksigner.

    ANDROID_HOME=<sdk> android/build_apk.sh [--unsigned]

It needs the Android SDK (`platforms/android-36`, `build-tools/36.1.0`), NDK
29.0.14206865, a JDK 17 or later, ninja and zip; the other settings are at
the top of the script (`OA_SDL_SRC=<dir>` reuses an SDL checkout). On this
machine the SDK is SilentHeritage's:

    ANDROID_HOME=~/projects/SH/work/android-sdk android/build_apk.sh

The package is `build/android/OpenAscendance-debug.apk`, signed with
`debug.keystore` at the project's root (created on first use, not
committed; keep it, since an installed package only accepts updates signed
with the same key). On the first start, the game asks for the install's
folder through the system's folder picker and copies its files into the
app's storage (`src/app/android_documents.h`); the folder may then be
removed. Install and follow the log with:

    adb install -r build/android/OpenAscendance-debug.apk
    adb logcat -s SDL SDL/APP GameActivity

## Packages

`packaging/build_desktop.sh linux|windows|macos` builds the release
packages into `dist/` (described at its top): the Linux tarball and
AppImage (needs `mksquashfs`), the Windows zip, the macOS disk image (on a
Mac). `OA_VERSION` sets their version.

## Icon

The icon is a placeholder (an original stylised galaxy, not the game's
art): `packaging/icon/open_ascendance.svg`. `packaging/icon/render_icons.sh`
renders the committed pictures from it (the desktop PNGs, the Windows
`.ico`, Android's launcher icons in `android/res/mipmap-*`), with python3
and the librsvg and cairo libraries, and ImageMagick 7.
