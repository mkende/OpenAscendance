#!/usr/bin/env bash
# Builds the Android package of OpenAscendance without Gradle (adapted from
# SilentHeritage's): CMake for the native library, javac, d8, aapt2,
# zipalign, apksigner. The package holds the native library of every ABI
# (arm64-v8a for phones and tablets, x86_64 for Chromebooks and emulators)
# and, for the OpenAscendance edition, no game file: the install setup
# copies them from the folder of the player's install on the first start
# (app/install_setup.h).
#
#   build/android/OpenAscendance-debug.apk     signed with the debug key
#   build/android/OpenAscendance-unsigned.apk  with --unsigned: aligned,
#       ready for apksigner (android/sign_apk.sh signs the release's with
#       the release key)
#
# With --edition=ascendancy, the package is the local debugging build of the
# Ascendancy edition (CMake option OA_EDITION, io.github.mkende.ascendancy),
# build/android/Ascendancy-debug.apk (or -unsigned.apk), with the game files
# of the OA_DATA install embedded, copied into the app's storage on the first
# start (app/install_bundle.h). It contains the original game's files: never
# distribute it. It is refused when the CI environment variable is set (never
# built in a workflow).
#
# The package is reproducible: the same sources, SDK, NDK and JDK, and
# SOURCE_DATE_EPOCH, give the same unsigned package.
#
# Usage: android/build_apk.sh [--edition=open|ascendancy] [--unsigned]
# Environment (defaults in brackets):
#   ANDROID_HOME  the Android SDK (required; ANDROID_SDK_ROOT is accepted
#                 too), holding platforms/android-36 and build-tools/36.1.0
#   ANDROID_NDK_HOME  the NDK [$ANDROID_HOME/ndk/29.0.14206865]
#   JAVA_HOME     a JDK 17+ [the JDK of the javac in PATH]
#   OA_DATA       the original install embedded in the Ascendancy package
#                 (required by it only)
#   OA_KEYSTORE   the debug keystore, created if missing (a standard Android
#                 debug key) [debug.keystore at the project's root,
#                 git-ignored]
#   OA_VERSION_CODE, OA_VERSION_NAME  the package version [1, 0.0.0-dev];
#                 the version name is also the native build's OA_VERSION
#   OA_ABIS       the ABIs of the package [arm64-v8a x86_64]
#   OA_SDL_SRC    an SDL checkout to build from [fetched by CMake]
#   SOURCE_DATE_EPOCH  the time stamp of the package's files [the last
#                 commit's]
#   OA_CMAKE_ARGS extra arguments of the native library's CMake configuration
set -euo pipefail

edition=open
signed=true
for arg in "$@"; do
  case "${arg}" in
    --edition=open | --edition=ascendancy) edition="${arg#--edition=}" ;;
    --unsigned) signed=false ;;
    *)
      echo "usage: $0 [--edition=open|ascendancy] [--unsigned]" >&2
      exit 1
      ;;
  esac
done
readonly edition signed
if [[ -n "${CI:-}" && "${edition}" == ascendancy ]]; then
  echo "build_apk.sh: the Ascendancy edition is a local build only, never built in CI" >&2
  exit 1
fi
# Whether the game files are embedded, and the package's name.
embedded=false
apk_name=OpenAscendance
if [[ "${edition}" == ascendancy ]]; then
  embedded=true
  apk_name=Ascendancy
fi
readonly embedded apk_name

readonly root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [[ -z "${sdk}" ]]; then
  echo "build_apk.sh: set ANDROID_HOME to the Android SDK" >&2
  exit 1
fi
readonly platform_jar="${sdk}/platforms/android-36/android.jar"
readonly build_tools="${sdk}/build-tools/36.1.0"
readonly ndk="${ANDROID_NDK_HOME:-${sdk}/ndk/29.0.14206865}"
readonly ndk_bin="${ndk}/toolchains/llvm/prebuilt/linux-x86_64/bin"
if [[ -z "${JAVA_HOME:-}" ]] && command -v javac > /dev/null; then
  JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"
fi
export JAVA_HOME="${JAVA_HOME:-}"
export PATH="${JAVA_HOME}/bin:${PATH}"
readonly data="${OA_DATA:-}"
readonly keystore="${OA_KEYSTORE:-${root}/debug.keystore}"
readonly version_code="${OA_VERSION_CODE:-1}"
readonly version_name="${OA_VERSION_NAME:-0.0.0-dev}"
read -r -a abis <<< "${OA_ABIS:-arm64-v8a x86_64}"
readonly abis
if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
  SOURCE_DATE_EPOCH="$(git -C "${root}" log -1 --format=%ct)"
fi
export SOURCE_DATE_EPOCH
readonly min_sdk=24
readonly target_sdk=36

readonly android="${root}/android"
readonly out="${root}/build/android"
# The native build directories, one per ABI: native-<abi>.
readonly native="${out}/native"
readonly stage="${out}/stage"

required=("${platform_jar}" "${build_tools}/aapt2" "${ndk}/build/cmake/android.toolchain.cmake"
  "${JAVA_HOME}/bin/javac")
tools=(cmake ninja zip git)
if [[ "${embedded}" == true ]]; then
  if [[ -z "${data}" ]]; then
    echo "build_apk.sh: set OA_DATA to the original install (edition ${edition})" >&2
    exit 1
  fi
  required+=("${data}/COB.CFG")
  tools+=(rsync)
fi
for f in "${required[@]}"; do
  if [[ ! -e "${f}" ]]; then
    echo "build_apk.sh: missing ${f}" >&2
    exit 1
  fi
done
for tool in "${tools[@]}"; do
  if ! command -v "${tool}" > /dev/null; then
    echo "build_apk.sh: ${tool} not found" >&2
    exit 1
  fi
done

# 1. The native library, the game with SDL linked in, as libmain.so, for
# every ABI. Sets edition_name, edition_executable and edition_app_id (the
# build's edition.sh).
cmake_args=()
read -r -a cmake_args <<< "${OA_CMAKE_ARGS:-}"
if [[ -n "${OA_SDL_SRC:-}" ]]; then
  cmake_args+=("-DFETCHCONTENT_SOURCE_DIR_SDL3=${OA_SDL_SRC}")
fi
for abi in "${abis[@]}"; do
  cmake -S "${root}" -B "${native}-${abi}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${root}/cmake/android.cmake" -DOA_ANDROID_NDK="${ndk}" -DOA_ANDROID_ABI="${abi}" \
    -DCMAKE_BUILD_TYPE=Release -DOA_BUILD_TESTS=OFF -DOA_SANITIZE=OFF -DOA_VERSION="${version_name}" -DOA_EDITION="${edition}" \
    ${cmake_args[@]+"${cmake_args[@]}"}
  source "${native}-${abi}/edition.sh"
  ninja -C "${native}-${abi}" "${edition_executable}"
done
sdl_src="$(sed -n 's/^SDL3_SOURCE_DIR:STATIC=//p' "${native}-${abis[0]}/CMakeCache.txt")"

# 2. Java: SDL's activity (from the sources the native build used) and ours,
# compiled to classes.dex.
rm -rf "${stage}"
mkdir -p "${stage}/classes" "${stage}/res"
mapfile -t java_sources < <(find "${sdl_src}/android-project/app/src/main/java" "${android}/java" -name '*.java' |
  LC_ALL=C sort)
"${JAVA_HOME}/bin/javac" -source 8 -target 8 -bootclasspath "${platform_jar}" -Xlint:-options -nowarn \
  -d "${stage}/classes" "${java_sources[@]}"
mapfile -t classes < <(find "${stage}/classes" -name '*.class' | LC_ALL=C sort)
"${build_tools}/d8" --release --min-api "${min_sdk}" --lib "${platform_jar}" --output "${stage}" "${classes[@]}"

stored=()
for abi in "${abis[@]}"; do
  mkdir -p "${stage}/lib/${abi}"
  "${ndk_bin}/llvm-strip" --strip-unneeded -o "${stage}/lib/${abi}/libmain.so" "${native}-${abi}/src/app/libmain.so"
  stored+=("lib/${abi}/libmain.so")
done

# 3. The game files of the Ascendancy edition (app/install_bundle.h): the
# install without the DOS programs, the sound drivers and their
# configuration, Windows' desktop.ini files and the player's state (the
# saves, RESUME.GAM, ASCEND.CFG's options: a fresh package starts with none;
# the game writes its own in the app's storage), and their index (sizes and
# paths). The patterns ignore case.
deflated=(classes.dex)
if [[ "${embedded}" == true ]]; then
  # Prints rsync pattern $1 matching any case.
  any_case() {
    local result= c i
    for ((i = 0; i < ${#1}; i++)); do
      c="${1:i:1}"
      if [[ "${c}" =~ [A-Za-z] ]]; then
        result+="[${c^^}${c,,}]"
      else
        result+="${c}"
      fi
    done
    echo "${result}"
  }
  excludes=()
  for pattern in desktop.ini '*.exe' '*.dig' '*.dll' '*.mdi' '*.lst' '*.ini' '*.log' '*.sav' '*.gam' ascend.cfg; do
    excludes+=("--exclude=$(any_case "${pattern}")")
  done
  mkdir -p "${stage}/assets/bundle"
  rsync -a "${excludes[@]}" "${data}/" "${stage}/assets/bundle/"
  {
    echo "OA-BUNDLE 1"
    (cd "${stage}/assets/bundle" && find . -type f -printf '%s %P\n' | LC_ALL=C sort -k2)
  } > "${stage}/assets/bundle_index.txt"
  mapfile -t -O 1 deflated < <(cd "${stage}" && find assets -type f | LC_ALL=C sort)
fi

# 4. Resources: the committed ones (the launcher icons rendered by
# packaging/icon/render_icons.sh) and the product's name. The manifest's
# package (the Java package) becomes the applicationId.
cp -r "${android}/res/." "${stage}/res/"
printf '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <string name="app_name">%s</string>\n</resources>\n' \
  "${edition_name}" > "${stage}/res/values/strings.xml"
"${build_tools}/aapt2" compile --dir "${stage}/res" -o "${stage}/res.zip"
"${build_tools}/aapt2" link -o "${stage}/unsigned.apk" -I "${platform_jar}" \
  --manifest "${android}/AndroidManifest.xml" --rename-manifest-package "${edition_app_id}" \
  --min-sdk-version "${min_sdk}" --target-sdk-version "${target_sdk}" \
  --version-code "${version_code}" --version-name "${version_name}" \
  "${stage}/res.zip"

# 5. The package: classes.dex, the game files if any (compressed: they are
# copied out, not read in place) and the native libraries (loaded from the
# APK in place: extractNativeLibs="false", hence stored uncompressed), added
# in a fixed order with SOURCE_DATE_EPOCH as their time and no extra
# attributes, aligned and signed (unless --unsigned).
(
  cd "${stage}"
  touch -d "@${SOURCE_DATE_EPOCH}" "${deflated[@]}" "${stored[@]}"
  TZ=UTC zip -q -X unsigned.apk "${deflated[@]}"
  TZ=UTC zip -q -X -0 unsigned.apk "${stored[@]}"
)
"${build_tools}/zipalign" -f -P 16 4 "${stage}/unsigned.apk" "${stage}/aligned.apk"
if [[ "${signed}" == true ]]; then
  if [[ ! -f "${keystore}" ]]; then
    mkdir -p "$(dirname "${keystore}")"
    "${JAVA_HOME}/bin/keytool" -genkeypair -keystore "${keystore}" -storepass android -keypass android \
      -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
  fi
  apk="${out}/${apk_name}-debug.apk"
  "${build_tools}/apksigner" sign --ks "${keystore}" --ks-pass pass:android --key-pass pass:android \
    --ks-key-alias androiddebugkey --out "${apk}" "${stage}/aligned.apk"
else
  apk="${out}/${apk_name}-unsigned.apk"
  mv "${stage}/aligned.apk" "${apk}"
fi
rm -rf "${stage}"
echo "${apk}: ${edition_name} (${edition_app_id}, ${version_name}, version code ${version_code}, ${abis[*]})"
