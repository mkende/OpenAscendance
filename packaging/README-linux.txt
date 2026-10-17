@NAME@ @VERSION@
==============================

An open-source remake of Ascendancy (The Logic Factory, 1995). It is only
an engine: you need your own copy of the original game.

Running
-------

    ./@EXECUTABLE@

On the first start, the game asks for the folder of your Ascendancy
install (the one holding COB.CFG and the ASCEND00..02.COB archives) and
remembers it; `--setup` asks again, `--data <folder>` gives it on the
command line. The game keeps its own files in
~/.local/share/@PREF_ORG@/@NAME@/, never in the install. BUILD.md, on the
project's page, describes the other options.

The program needs only the system's C library (glibc 2.35 or later, as in
Ubuntu 22.04 and Debian 12): SDL, linked in, uses the X11 or Wayland
display and the PulseAudio, PipeWire or ALSA sound of the system.

Menu entry
----------

To add the game to your desktop's menu, put the program in a folder of
your PATH and the .desktop file and the icon where your desktop finds them:

    install -m 755 @EXECUTABLE@ ~/.local/bin/
    install -m 644 *.desktop ~/.local/share/applications/
    install -m 644 *.png ~/.local/share/icons/hicolor/256x256/apps/

(The AppImage of the same release needs none of this.)

Licences
--------

The licences are in the licenses folder (SDL: zlib licence).

@NAME@ is not affiliated with or endorsed by The Logic Factory or any
owner of the Ascendancy trademark or of the original game.
