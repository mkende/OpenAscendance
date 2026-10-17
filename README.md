# Open Ascendance

Open Ascendance is an open-source remake of *Ascendancy*, the space
strategy game published by The Logic Factory in 1995. It runs on Windows,
macOS, Linux and Android, and plays both the standard game (version 1.6.5)
and the Antagonizer edition.

Status: in development. The front end (logo, intro, opening menu, new game,
species history, load and save), the star map, the System Display and its
battles, the planets, research and diplomacy screens, the days with their
game events up to the end of the game, and the tutorials are playable; the
saves are compatible with the original's.

## Playing

1. **Get the original game.** Open Ascendance is only an engine: it plays
   your own copy of Ascendancy and does not include any of its files. It
   needs an installed version of the original game (the folder holding
   `COB.CFG` and the `ASCEND00..02.COB` archives) to read them from.

2. **Download** the package for your system from the
   [latest release](../../releases/latest):

   - **Windows** 10 or later (x64): unzip the `.zip` anywhere and start the
     program. It is not signed, so Windows SmartScreen will ask for
     confirmation the first time you run it: click "More info", then "Run
     anyway".
   - **macOS** 13.3 or later (Apple Silicon and Intel): open the `.dmg` and
     drag the app onto Applications. As it is not notarized by Apple, macOS
     will refuse to open it the first time. Click Done, then go to System
     Settings, Privacy & Security, and click "Open Anyway".
   - **Linux** (x86_64 or aarch64, with glibc 2.35 or later, as in Ubuntu
     22.04 and Debian 12): make the `.AppImage` executable and run it. The
     `.tar.gz` contains the same program, with a menu entry and an icon.
   - **Android** 7.0 or later (phones, tablets and Chromebooks): the `.apk`,
     which you will need to sideload.

3. **Start it.** The first time you launch the game, it asks for the folder
   where Ascendancy is installed and remembers it. On Android, the folder's
   files are copied into the app (the folder may then be removed). Open
   Ascendance never writes into the original install: your saves and options
   go to an Open Ascendance folder in your home folder.

## Classic and Extended

- **Classic** plays the original game as it was, with the same rules,
  screens and AI, and even its bugs, in two variants chosen in the opening
  menu's **Options** (for the next new game, kept in its saves):
  **Standard** (Ascendancy 1.6.5) and **Antagonizer** (the Antagonizer
  edition's opponents and rule changes). The interface is only improved
  where it never changes the game: per-user saves outside the original's
  folder, scaling of the original 640x480 picture to any window size (smooth
  or sharp pixels), full screen with Alt+Enter, music and sound toggles.

- **Extended** (planned): the classic game with fixes and improvements.

## Building and contributing

See [BUILD.md](BUILD.md) to learn how to build, run and test Open
Ascendance, and how its sources are organised, and
[RELEASING.md](RELEASING.md) for how its releases are made.

The remake is built from a reverse engineering of the original game, kept
in a separate, private repository: its specifications (file formats, data
structures, the game's rules and AI) and the reference runs of the original
that the tests compare with. Code comments cite the original functions they
port by their `PATCH.EXE` address, and the specifications of that
repository by their path there (`docs/…`, `recon/…`, `tools/…`,
`tests/oracle/…`).

## Disclaimer

This project is not affiliated with or endorsed by The Logic Factory,
Virgin Interactive, or any owner of the Ascendancy trademark or of the
original game. Its source code contains no data from the original game.
"Ascendancy" is a trademark of its owner and is only used here to say what
this software is compatible with.

## License

Open Ascendance is released under the MIT license; see [LICENSE](LICENSE).
SDL3 (zlib license) and Catch2 (Boost Software License) are downloaded at
build time and are not part of this repository. The files of the original
game are, of course, neither included nor covered by this license.
