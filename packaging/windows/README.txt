renderHAM for Windows
=====================

renderHAM turns photos into Amiga HAM6 pictures (IFF ILBM) and Ken Burns style IFF animations.

Start
-----
Double-click renderHAMgui.exe for the program with a window. Keep the folder together: the
DLL files next to the program belong to it, and so does the "translations" folder (the German
user interface).

renderHAM.exe is the command-line tool. Open a terminal in this folder and try
    renderHAM --help

Windows may warn
----------------
This program is not signed, so Windows SmartScreen may show "Windows protected your PC" when you
start it for the first time. Click "More info", then "Run anyway". (If you downloaded the ZIP file
and Windows blocks the files after unpacking: right-click the ZIP file, Properties, tick
"Unblock", and unpack it again.)

About
-----
renderHAM is free software under the MIT License (see LICENSE); the libraries it uses are listed
in THIRD_PARTY_NOTICES.md. The DLL files in this folder are free software of other authors; the
folder "licenses" has their license texts, and licenses\PACKAGES.txt says which package, version
and license each DLL belongs to. It is AI-generated software: the code was written by Claude (Anthropic)
under the direction of rawbit (@ravvbit). Project page and manual: https://github.com/ravvbit/renderham
