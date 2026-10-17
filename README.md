# Open Ascendance

A faithful open-source remake of **Ascendancy** (The Logic Factory, 1995) in
C++20 and SDL3, built from the reverse engineering in this repository
(`../docs`). It reads the original game files from the player's own install
(the folder holding `COB.CFG` and the `ASCEND00..02.COB` archives).

Status: milestone 1, the platform layer, the asset decoders, the logo, the
opening menu and the intro with its subtitles and music, the window toolkit
(buttons, toggles, list boxes, help, modal dialogs, help mode, exit corners,
hotkeys, `ascend.cfg`) and the front-end screens (new game, species history,
load/save lists); milestone 2 (in progress), the game screens over the game
core: the star map (its new-game, species and diplomacy variants), the saves
with their window state, the planet and research screens; the diplomacy
(milestone 6): species selection, negotiation, intelligence and the special
abilities window; the System Display (milestone 5): the 3D star-system view
and the battles played in it; the days played from the star map (Next Day,
the auto next turn, the game-event boxes and the screens they lead to, the
end of the game) and the tutorials (replays of their recorded input).

## Layout

The modules follow the original's (`docs/05-subsystems.md`), comments citing
the functions they port with their `PATCH.EXE` address:

- `src/core`: install access, the Watcom `rand` and `qsort`, logging.
- `src/assets`: the COB archives (`cfile`), shapes, palettes, haze tables,
  fonts, GIF, FLC, sounds and the text tables.
- `src/game`: the headless game core (no SDL): the game tables, the state
  (`Cosmos`, races, stars, lanes, planets, ships, research), the saves, the
  galaxy generation and the day (`cosmos.h`: `cosmos_end_day` and the
  contested-star walk; planets, research, ships and gizmos, diplomacy, the
  battle rules and the System Display's turn loop `battle_control.h`), with
  each variant's AI behind `ai.h` (Standard: `race_ai`, `battle`;
  Antagonizer: `antag_*`).
- `src/gfx`: the VFX blitters, the fonts, `GSystem` (screen, DAC, fades).
- `src/audio`: the mixer and `SoundSystem` (music and effects).
- `src/platform`: the SDL window, input, clock and sound output.
- `src/ui`: `WinMgr` (with the game events and the trace replay), `GWindow`
  and the toolkit's window classes, the game-event box, the FLIC player and
  window, `GameSession` (the game core as the screens see it)
  and `GameCoreSession` (it over `src/game`: saves, new game, the day).
- `src/screens`: the screens (new game, species history, load/save and
  tutorial lists, the star map, the System Display, planets, research,
  negotiation, intelligence, end of game).
- `src/app`: `main`, the command line and the install setup.

## Build

CMake 3.25 or later, Ninja, clang and git (CMake downloads SDL3 and Catch2;
`-DFETCHCONTENT_SOURCE_DIR_SDL3=<dir>` and `_CATCH2` reuse a checkout):

    cmake -S . -B build/debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    ninja -C build/debug

Debug builds (the default) enable ASan and UBSan; under WSLg set
`ASAN_OPTIONS=detect_leaks=0`. Add `-DCMAKE_BUILD_TYPE=Release` for a release
build. [BUILD.md](BUILD.md) has the Windows, macOS and Android builds and the
packages, [RELEASING.md](RELEASING.md) the releases.

## Run

    build/debug/src/app/open_ascendance [--data <install dir> | --setup]
        [--sharp | --smooth] [--fullscreen | --window] [--variant standard|antagonizer]

Without `--data` the game asks once for the install folder and remembers it.
Alt+Enter toggles fullscreen. `--screenshot <file.bmp> [--frames N]` with
`--keys <k1,k2,...>` runs headless and saves a frame, `--save-dir <dir>`
keeping its saves and Alt+G screenshots, and `--seed N` starts the galaxy of
the oracle's runs; `--trace <file.bin>` replays a window-event trace (an
oracle run's `trace.bin`) from the opening menu
([`src/app/command_line.h`](src/app/command_line.h)).

## Options

The opening menu has an **Options** button, the remake's own (the seven
original buttons are a little shorter to make room). Its screen sets:

- **Game variant**: the rules and opponents of the next new game, Standard
  (Ascendancy 1.6.5) or Antagonizer (the Antagonizer edition's). A game keeps
  its variant in its saves; the new-game screen names a variant other than
  Standard under its summary.
- **Display**: a window or the full screen (Alt+Enter also switches).
- **Scaling**: smooth or sharp pixels.
- **Music** and **Sound effects**: on or off (also Alt+M and Alt+N).

OK (Enter) keeps the settings, Cancel (Esc) leaves them; Shift and a click on
a row explains it. They are kept in `options.cfg` in the user folder (the
sound in the original's `ascend.cfg`, which stays readable by the original);
`--sharp`/`--smooth`, `--fullscreen`/`--window` and `--variant` override them
for a run. While a tutorial plays, the opening menu has its original layout.

## Tests

    ninja -C build/debug oa_tests oa_game_tests
    OA_DATA=<install dir> ctest --test-dir build/debug -j <processors>

Tests of the original files' content are skipped when `OA_DATA` is unset;
the comparisons with the original's saves (`docs/12-oracle.md`) when
`OA_ORACLE` does not name the `work/oracle` folder of the reference runs.
The day tests (`tests/day_test.cpp`, tag `[day]`) replay every case of the
corpus; with `OA_DUMP_DIR` set they write the remake's save of the first
diverging day, to compare with `tools/savdiff.py`. `tests/day_ui_test.cpp`
plays the oracle's runs through the user interface from their own traces
(new games, days with their game-event boxes, the tutorials) and compares
the saves and Alt+G screenshots (`OA_DAY_UI_RUN` picks runs by name,
`OA_DUMP_DIR` keeps the remake's files).
