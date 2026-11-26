renderHAM for macOS
===================

renderHAM turns photos into Amiga HAM6 pictures (IFF ILBM) and Ken Burns style IFF animations.

Install
-------
Drag renderHAM.app onto the Applications folder in this window. The program runs on Macs with
an Intel processor and on Macs with Apple Silicon (macOS 11 or newer).

The command-line tool is in the folder "Command-line tool" of this window. Copy the file "renderHAM"
somewhere you like (for example /usr/local/bin) and try  renderHAM --help  in the Terminal.

macOS asks for confirmation the first time
------------------------------------------
This program is not signed with an Apple developer certificate, so macOS says that it cannot
check it for malicious software and refuses to open it by double-click. To open it anyway:
  1. Try to open renderHAM.app once (macOS shows the message).
  2. Open System Settings > Privacy & Security, scroll down to the message about renderHAM
     and click "Open Anyway". Confirm with your password.
(On macOS 14 and older: Control-click the app, choose "Open", then "Open" again.)

If macOS still does not open the app, remove the mark that macOS puts on every file that is
downloaded from the internet (the "quarantine" mark). After renderHAM.app is in the Applications
folder, type this in the Terminal (it asks for your password):
    sudo xattr -r -d com.apple.quarantine /Applications/renderHAM.app
The command changes nothing in the program; it only removes the mark, and macOS stops checking
this one app. Use it only for a copy from the release page of the project
(https://github.com/ravvbit/renderham/releases): its file SHA256SUMS has the checksum of this
disk image.

The command-line tool is blocked the same way when it is downloaded; remove the mark with
    xattr -d com.apple.quarantine renderHAM

About
-----
renderHAM is free software under the MIT License (see LICENSE); the libraries it uses are listed
in THIRD_PARTY_NOTICES.md. The application contains wxWidgets and the libraries that come with it;
their license texts are inside renderHAM.app (Control-click the app, "Show Package Contents",
Contents > Resources > Licenses). It is AI-generated software: the code was written by Claude (Anthropic)
under the direction of rawbit (@ravvbit). Project page and manual: https://github.com/ravvbit/renderham
