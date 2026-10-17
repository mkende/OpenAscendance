@NAME@ @VERSION@
==============================

An open-source remake of Ascendancy (The Logic Factory, 1995). It is only
an engine: you need your own copy of the original game.

Installing
----------

Drag @NAME@ onto the Applications folder. It runs on macOS 13.3 or later,
on Apple Silicon and Intel Macs.

The app is not notarized by Apple: the first time, macOS refuses to open
it ("Apple could not verify..."). Click Done, then open System Settings,
Privacy & Security: under Security, click "Open Anyway" next to
@NAME@, and confirm. macOS remembers it for the next times.

Or, from the Terminal:

    xattr -dr com.apple.quarantine "/Applications/@NAME@.app"

Running
-------

On the first start, the game asks for the folder of your Ascendancy
install (the one holding COB.CFG and the ASCEND00..02.COB archives) and
remembers it. The game keeps its own files in
~/Library/Application Support/@PREF_ORG@/@NAME@/, never in the install.
The project's page describes the command-line options:

    "/Applications/@NAME@.app/Contents/MacOS/@EXECUTABLE@" --setup

Licences
--------

The licences are in the app: @NAME@.app/Contents/Resources/licenses
(SDL: zlib licence).

@NAME@ is not affiliated with or endorsed by The Logic Factory or any
owner of the Ascendancy trademark or of the original game.
