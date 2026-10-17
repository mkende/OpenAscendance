cmake_minimum_required(VERSION 3.25)
project(open_ascendance LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++20 modules: no dependency scan (which needs clang-scan-deps).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(OA_BUILD_TESTS "Build the unit tests" ON)
option(OA_SANITIZE "Use ASan/UBSan in debug builds (Linux only)" ON)

# The product: its name (window title, messages), executable, application id
# (SDL's, the Android package, the macOS bundle, the Linux .desktop file) and
# SDL per-user folder, in the generated header core/edition.h and, for the
# packaging scripts, edition.sh in the build directory.
set(OA_NAME "Open Ascendance")
set(OA_EXECUTABLE open_ascendance)
set(OA_APP_ID io.github.mkende.openascendance)
# Empty: the per-user folder is just "<app>" (SDL_GetPrefPath skips an empty org).
set(OA_PREF_ORG "")
# The version of the build, MAJOR.MINOR.PATCH with an optional -suffix: the
# release's tag without its "v" (.github/workflows/release.yml), in
# core/edition.h and the Windows version resource.
set(OA_VERSION "0.0.0-dev" CACHE STRING "The version: MAJOR.MINOR.PATCH[-suffix]")
if(NOT OA_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[0-9A-Za-z.-]+)?$")
  message(FATAL_ERROR "OA_VERSION must be MAJOR.MINOR.PATCH[-suffix], not '${OA_VERSION}'")
endif()
set(OA_VERSION_MAJOR ${CMAKE_MATCH_1})
set(OA_VERSION_MINOR ${CMAKE_MATCH_2})
set(OA_VERSION_PATCH ${CMAKE_MATCH_3})
configure_file(src/core/edition.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/core/edition.h @ONLY)
configure_file(cmake/edition.sh.in ${CMAKE_CURRENT_BINARY_DIR}/edition.sh @ONLY)

# Release builds carry no path of the build machine (__FILE__ in SDL's and
# our messages); before the dependencies, so that it applies to them too.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-ffile-prefix-map=${CMAKE_BINARY_DIR}/=build/ -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=)
endif()

include(FetchContent)

# Third-party dependencies, pinned to a commit (a tag can be moved) and built
# from source (the same versions as SilentHeritage). An existing checkout can
# be used with -DFETCHCONTENT_SOURCE_DIR_SDL3=<dir> (and _CATCH2).
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG 829a65d769d935c4852f8159e964312c0957260a  # release-3.4.18
  SYSTEM)
FetchContent_MakeAvailable(SDL3)

if(OA_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG 317ac1ed4c0bb6e6b91eafc817e05c488feffcb3  # v3.16.0
    SYSTEM)
  FetchContent_MakeAvailable(Catch2)
endif()

# Warning and sanitizer flags for our own code only (not the dependencies).
add_library(oa_options INTERFACE)
target_compile_options(oa_options INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wimplicit-int-conversion
  -Wshorten-64-to-32 -Werror)
# No fused multiply-add: the same floating-point results on every processor.
target_compile_options(oa_options INTERFACE -ffp-contract=off)
target_include_directories(oa_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated)
if(OA_SANITIZE AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
  target_compile_options(oa_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(oa_options INTERFACE -fsanitize=address,undefined)
endif()
if(WIN32)
  # A self-contained .exe: no dependency on the mingw runtime DLLs.
  target_link_options(oa_options INTERFACE -static)
  # mingw's <intrin.h> (pulled in by the SDL headers) includes clang's
  # deprecated <mm3dnow.h>, whose #warning would fail the build.
  target_compile_options(oa_options INTERFACE -Wno-\#warnings)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  # The C++ runtime linked in: the Linux executable needs only glibc and the
  # system libraries SDL opens at run time.
  target_link_options(oa_options INTERFACE -static-libstdc++ -static-libgcc)
endif()

# Declares module library `name` from SOURCES, linking DEPS publicly.
function(oa_add_module name)
  cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})
  add_library(${name} STATIC ${ARG_SOURCES})
  target_link_libraries(${name} PUBLIC oa_options ${ARG_DEPS})
endfunction()

add_subdirectory(src/core)
add_subdirectory(src/assets)
add_subdirectory(src/game)
add_subdirectory(src/gfx)
add_subdirectory(src/audio)
add_subdirectory(src/platform)
add_subdirectory(src/ui)
add_subdirectory(src/screens)
add_subdirectory(src/app)

if(OA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
