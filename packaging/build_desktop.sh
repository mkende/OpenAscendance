#!/usr/bin/env bash
# Builds the desktop packages of a release into dist/ (adapted from
# SilentHeritage's):
#   linux    OpenAscendance-<version>-linux-<arch>.tar.gz: the executable, its
#            .desktop file and icon, README.txt and the licences; and
#            OpenAscendance-<version>-linux-<arch>.AppImage, the same as a
#            single file (<arch> the host's: x86_64 or aarch64).
#   windows  OpenAscendance-<version>-windows-x64.zip: the executable (its
#            icon, version and manifest in its resources), README.txt and
#            the licences; cross-compiled (cmake/mingw-clang.cmake).
#   macos    OpenAscendance-<version>-macos.dmg: the app (one universal
#            executable, arm64 and x86_64, for macOS 13.3 and later; its
#            icon; README.txt and the licences in its resources), ad-hoc
#            signed, next to a link to /Applications and README.txt.
# The release workflow runs it (.github/workflows/build.yml), on Ubuntu 22.04
# for Linux (packaging/linux/install_build_deps.sh), 24.04 for Windows
# (packaging/windows/install_build_deps.sh), macOS 15 for macOS.
#
# The executables run from any folder and keep nothing next to themselves:
# the game asks for the install's folder on the first start and keeps its
# own files in the per-user application folder. The icons are the committed
# pictures of packaging/icon (render_icons.sh).
#
# Usage: packaging/build_desktop.sh linux|windows|macos
# Environment (defaults in brackets):
#   OA_VERSION         the version, MAJOR.MINOR.PATCH[-suffix] [0.0.0-dev]
#   SOURCE_DATE_EPOCH  the time stamp of the packages' files [the last commit's]
#   OA_APPIMAGE_RUNTIME  the AppImage runtime [downloaded, its SHA-256 checked]
#   OA_CMAKE_ARGS      extra arguments of the CMake configuration
#
# It runs with macOS's bash 3.2: no associative arrays, and the arrays that
# can be empty are expanded as ${a[@]+"${a[@]}"} (set -u).
set -euo pipefail

readonly platform="${1:-}"
case "${platform}" in
  linux | windows | macos) ;;
  *)
    echo "usage: $0 linux|windows|macos" >&2
    exit 1
    ;;
esac

readonly root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly version="${OA_VERSION:-0.0.0-dev}"
if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
  SOURCE_DATE_EPOCH="$(git -C "${root}" log -1 --format=%ct)"
fi
export SOURCE_DATE_EPOCH
readonly dist="${root}/dist"
readonly work="${root}/build/package-${platform}"
readonly icons="${root}/packaging/icon"
readonly name="OpenAscendance-${version}"

# The AppImage runtime (https://github.com/AppImage/type2-runtime), pinned:
# its release and the SHA-256 of each architecture's file.
readonly appimage_runtime_release=20251108
appimage_runtime_sha256() {
  case "$1" in
    x86_64) echo 2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d ;;
    aarch64) echo 00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444 ;;
  esac
}

# The oldest macOS the app runs on (libc++'s <format>).
readonly macos_minimum=13.3

cmake_extra=()
read -r -a cmake_extra <<< "${OA_CMAKE_ARGS:-}"
readonly cmake_extra

readonly build_dir="${work}/build"

# build [cmake args...]: the game's executable, release build. Sets
# edition_name, edition_executable, edition_app_id and edition_pref_org (the
# build's edition.sh), and sdl_src (SDL's sources, for its licence).
build() {
  cmake -S "${root}" -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE=Release -DOA_BUILD_TESTS=OFF -DOA_SANITIZE=OFF \
    -DOA_VERSION="${version}" "$@" ${cmake_extra[@]+"${cmake_extra[@]}"}
  source "${build_dir}/edition.sh"
  sdl_src="$(sed -n 's/^SDL3_SOURCE_DIR:STATIC=//p' "${build_dir}/CMakeCache.txt")"
  ninja -C "${build_dir}" "${edition_executable}"
}

# stage_docs <dir>: the README and the licences of a package.
stage_docs() {
  local dir="$1"
  sed -e "s/@NAME@/${edition_name}/g" -e "s/@VERSION@/${version}/g" -e "s/@EXECUTABLE@/${edition_executable}/g" \
    -e "s/@PREF_ORG@/${edition_pref_org}/g" "${root}/packaging/README-${platform}.txt" > "${dir}/README.txt"
  mkdir -p "${dir}/licenses"
  # The project's own licence, once it has one.
  if [[ -f "${root}/LICENSE" ]]; then
    cp "${root}/LICENSE" "${dir}/licenses/OpenAscendance.txt"
  fi
  cp "${sdl_src}/LICENSE.txt" "${dir}/licenses/SDL3.txt"
}

# fix_times <dir>: every file and folder under <dir> stamped SOURCE_DATE_EPOCH
# (GNU date, else BSD date).
fix_times() {
  local stamp
  stamp="$(date -u -d "@${SOURCE_DATE_EPOCH}" +%Y%m%d%H%M.%S 2>/dev/null ||
    date -u -r "${SOURCE_DATE_EPOCH}" +%Y%m%d%H%M.%S)"
  find "$1" -exec env TZ=UTC touch -h -t "${stamp}" {} +
}

# appimage_runtime <arch>: the path of the AppImage runtime for <arch>.
appimage_runtime() {
  local arch="$1"
  if [[ -n "${OA_APPIMAGE_RUNTIME:-}" ]]; then
    echo "${OA_APPIMAGE_RUNTIME}"
    return
  fi
  local file="${work}/runtime-${arch}"
  if [[ ! -f "${file}" ]]; then
    curl -fsSL -o "${file}.part" \
      "https://github.com/AppImage/type2-runtime/releases/download/${appimage_runtime_release}/runtime-${arch}"
    echo "$(appimage_runtime_sha256 "${arch}")  ${file}.part" | sha256sum -c - >&2
    mv "${file}.part" "${file}"
  fi
  echo "${file}"
}

package_linux() {
  local arch
  arch="$(uname -m)"
  build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_LINKER_TYPE=LLD
  local desktop
  desktop="$(sed -e "s/@NAME@/${edition_name}/g" -e "s/@EXECUTABLE@/${edition_executable}/g" \
    -e "s/@APP_ID@/${edition_app_id}/g" "${root}/packaging/linux/app.desktop.in")"

  # The tarball: the executable and, for a desktop menu entry, its .desktop
  # file and icon (README.txt says where to put them).
  local tree="${work}/${name}-linux-${arch}"
  rm -rf "${tree}"
  mkdir -p "${tree}"
  install -m 755 -s "${build_dir}/src/app/${edition_executable}" "${tree}/"
  echo "${desktop}" > "${tree}/${edition_app_id}.desktop"
  cp "${icons}/icon-256.png" "${tree}/${edition_app_id}.png"
  stage_docs "${tree}"
  fix_times "${tree}"
  tar -C "${work}" --sort=name --owner=0 --group=0 --numeric-owner --mode='go-w' -cf - "$(basename "${tree}")" |
    gzip -9 -n > "${dist}/${name}-linux-${arch}.tar.gz"

  # The AppImage: an AppDir (the executable in usr/bin, AppRun its link,
  # the .desktop file and icon at the top and in usr/share) as a squashfs
  # after the runtime.
  local appdir="${work}/AppDir"
  rm -rf "${appdir}"
  mkdir -p "${appdir}/usr/bin" "${appdir}/usr/share/applications" "${appdir}/usr/share/doc/${edition_executable}"
  install -m 755 -s "${build_dir}/src/app/${edition_executable}" "${appdir}/usr/bin/"
  ln -s "usr/bin/${edition_executable}" "${appdir}/AppRun"
  echo "${desktop}" > "${appdir}/usr/share/applications/${edition_app_id}.desktop"
  ln -s "usr/share/applications/${edition_app_id}.desktop" "${appdir}/${edition_app_id}.desktop"
  local size
  for size in 16 24 32 48 64 128 256 512; do
    mkdir -p "${appdir}/usr/share/icons/hicolor/${size}x${size}/apps"
    cp "${icons}/icon-${size}.png" "${appdir}/usr/share/icons/hicolor/${size}x${size}/apps/${edition_app_id}.png"
  done
  ln -s "usr/share/icons/hicolor/256x256/apps/${edition_app_id}.png" "${appdir}/${edition_app_id}.png"
  ln -s "${edition_app_id}.png" "${appdir}/.DirIcon"
  stage_docs "${appdir}/usr/share/doc/${edition_executable}"
  fix_times "${appdir}"
  rm -f "${work}/image.squashfs"
  # (mksquashfs stamps every file and the image SOURCE_DATE_EPOCH.)
  mksquashfs "${appdir}" "${work}/image.squashfs" -root-owned -noappend -comp zstd -quiet
  local appimage="${dist}/${name}-linux-${arch}.AppImage"
  cat "$(appimage_runtime "${arch}")" "${work}/image.squashfs" > "${appimage}"
  chmod 755 "${appimage}"
}

package_windows() {
  build -DCMAKE_TOOLCHAIN_FILE="${root}/cmake/mingw-clang.cmake" -DOA_WINDOWS_ICON="${icons}/icon.ico"
  local tree="${work}/${name}-windows-x64"
  rm -rf "${tree}"
  mkdir -p "${tree}"
  llvm-strip -o "${tree}/${edition_executable}.exe" "${build_dir}/src/app/${edition_executable}.exe" 2>/dev/null ||
    x86_64-w64-mingw32-strip -o "${tree}/${edition_executable}.exe" "${build_dir}/src/app/${edition_executable}.exe"
  stage_docs "${tree}"
  # Windows line endings for the texts, read with Notepad.
  find "${tree}" -name '*.txt' -exec sed -i 's/\r*$/\r/' {} +
  fix_times "${tree}"
  local zip="${dist}/${name}-windows-x64.zip"
  rm -f "${zip}"
  (
    cd "${work}"
    find "$(basename "${tree}")" | LC_ALL=C sort | TZ=UTC zip -q -X -9 "${zip}" -@
  )
}

package_macos() {
  build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET="${macos_minimum}"

  # The app: the executable, its Info.plist and icon, the README and the
  # licences in its resources.
  local stage="${work}/dmg"
  rm -rf "${stage:?}"
  local app="${stage}/${edition_name}.app"
  mkdir -p "${app}/Contents/MacOS" "${app}/Contents/Resources"
  cp "${build_dir}/src/app/${edition_executable}" "${app}/Contents/MacOS/"
  strip -x "${app}/Contents/MacOS/${edition_executable}"
  # The .icns from the committed PNGs (iconutil's names: <size>[@2x]).
  local iconset="${work}/AppIcon.iconset"
  rm -rf "${iconset}"
  mkdir -p "${iconset}"
  local size
  for size in 16 32 128 256 512; do
    cp "${icons}/icon-${size}.png" "${iconset}/icon_${size}x${size}.png"
    cp "${icons}/icon-$((size * 2)).png" "${iconset}/icon_${size}x${size}@2x.png"
  done
  iconutil -c icns -o "${app}/Contents/Resources/AppIcon.icns" "${iconset}"
  # CFBundleShortVersionString and CFBundleVersion: numbers only.
  sed -e "s/@NAME@/${edition_name}/g" -e "s/@EXECUTABLE@/${edition_executable}/g" \
    -e "s/@APP_ID@/${edition_app_id}/g" -e "s/@VERSION@/${version%%-*}/g" \
    -e "s/@MINIMUM_SYSTEM@/${macos_minimum}/g" "${root}/packaging/macos/Info.plist.in" > "${app}/Contents/Info.plist"
  printf 'APPL????' > "${app}/Contents/PkgInfo"
  stage_docs "${app}/Contents/Resources"
  # The disk image's window: the app, where to drag it, the README.
  cp "${app}/Contents/Resources/README.txt" "${stage}/README.txt"
  ln -s /Applications "${stage}/Applications"
  fix_times "${stage}"
  # Ad-hoc signed (no certificate): Apple Silicon runs no unsigned code.
  codesign --force --deep --sign - "${app}"
  codesign --verify --deep --strict "${app}"
  local dmg="${dist}/${name}-macos.dmg"
  rm -f "${dmg}"
  hdiutil create -quiet -volname "${edition_name} ${version}" -srcfolder "${stage}" -fs HFS+ -format UDZO "${dmg}"
}

mkdir -p "${dist}" "${work}"
"package_${platform}"
ls -l "${dist}"
