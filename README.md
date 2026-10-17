# Open Ascendance

A faithful open-source remake of **Ascendancy** (The Logic Factory, 1995) in
C++20 and SDL3, built from the reverse engineering in this repository
(`../docs`). It reads the original game files from the player's own install
(the folder holding `COB.CFG` and the `ASCEND00..02.COB` archives).

Status: milestone 1, the platform layer, the asset decoders, the logo, the
opening menu and the intro with its subtitles and music.

## Layout

The modules follow the original's (`docs/05-subsystems.md`), comments citing
the functions they port with their `PATCH.EXE` address:

- `src/core`: install access, the Watcom `rand` and `qsort`, logging.
- `src/assets`: the COB archives (`cfile`), shapes, palettes, haze tables,
  fonts, GIF, FLC, sounds and the text tables.
- `src/gfx`: the VFX blitters, the fonts, `GSystem` (screen, DAC, fades).
- `src/audio`: the mixer and `SoundSystem` (music and effects).
- `src/platform`: the SDL window, input, clock and sound output.
- `src/ui`: `WinMgr`, `GWindow`, the FLIC player and window.
- `src/app`: `main`, the command line and the install setup.

## Build

CMake 3.25 or later, Ninja, clang and git (CMake downloads SDL3 and Catch2;
`-DFETCHCONTENT_SOURCE_DIR_SDL3=<dir>` and `_CATCH2` reuse a checkout):

    cmake -S . -B build/debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    ninja -C build/debug

Debug builds (the default) enable ASan and UBSan; under WSLg set
`ASAN_OPTIONS=detect_leaks=0`. Add `-DCMAKE_BUILD_TYPE=Release` for a release
build.

## Run

    build/debug/src/app/open_ascendance [--data <install dir> | --setup] [--sharp]

Without `--data` the game asks once for the install folder and remembers it.
Alt+Enter toggles fullscreen. `--screenshot <file.bmp> [--frames N]` with
`--keys <k1,k2,...>` runs headless and saves a frame
([`src/app/command_line.h`](src/app/command_line.h)).

## Tests

    ninja -C build/debug oa_tests
    OA_DATA=<install dir> ctest --test-dir build/debug -j <processors>

Tests of the original files' content are skipped when `OA_DATA` is unset.
