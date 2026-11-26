<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
-->

# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org) (see the README).

## [Unreleased]

### Added

- **Windows and macOS packages**: `renderHAM-<version>-windows-x64.zip` (the GUI, the command-line
  tool, the DLLs they need and the German translation) and `renderHAM-<version>-macos-universal.dmg`
  (`renderHAM.app` for Intel Macs and Apple Silicon, macOS 11 or newer, plus the command-line tool).
  Built by `scripts/package_windows.sh` and `scripts/package_macos.sh`; neither is signed, and a
  `README.txt` inside explains the SmartScreen and Gatekeeper warnings.
- **Windows and macOS builds in CI**: one workflow, `.github/workflows/build.yml`, now builds, tests and
  packages for Linux, Windows (MinGW-w64, MSYS2 UCRT64) and macOS (a universal build for Intel and
  Apple Silicon; wxWidgets 3.2.12 built from source and cached). It unpacks the finished ZIP and disk
  image, runs the command-line tool, converts to a file with a non-ASCII name and starts the GUI with
  a picture; the screenshots are kept as workflow artifacts. The tests run the Intel part of the
  macOS program on an Intel runner; the Apple Silicon part is compiled and linked but not run. A
  version tag drafts a release with the Linux, Windows and macOS packages.
- Application icon for Windows (`renderham.ico`, also in the title bar and the taskbar) and macOS
  (`renderham.icns`), made from the Linux icon by `scripts/make_app_icons.py`.
- renderHAMgui is a windowed program (no console window) on Windows, with a manifest for the modern
  controls and per-monitor DPI, and an application bundle, `renderHAM.app` (with `Info.plist`, the icon
  and the translation in `Contents/Resources`), on macOS.
- New test `unicode_paths`: file names with non-ASCII characters in every place that opens a file.
- **Linux packages**: the workflow builds, tests and packages renderHAM for x86_64 Linux as a Debian
  package, an AppImage and a Flatpak, and drafts a GitHub release with checksums when a version tag is
  pushed.
- `scripts/build_appimage.sh` (AppImage of the GUI) and `packaging/linux/flatpak/` (Flatpak manifest:
  wxWidgets 3.2.12 built from source on the GNOME 50 runtime).
- Desktop integration on Linux: menu entry, AppStream metadata and a placeholder icon in
  `packaging/linux/`, installed by `cmake --install`; the license texts are installed to
  `share/doc/renderham/`.
- **Recent images** (GUI): a "Recent" button next to "Open..." (also `Ctrl+Shift+O`) lists the last nine
  opened images, most recent first; a digit key picks one. The list is kept between sessions, files that
  no longer exist are dropped when the menu opens, and "Clear list" empties it.
- **More input formats**: GIF (first frame), Photoshop PSD (the flattened picture) and binary PPM/PGM can
  now be opened from the file dialog and are named in the desktop entry (`MimeType=`) of the Linux
  packages. The loader read them already; only the lists did not mention them. The new
  `aic::supported_image_formats()` is the one list behind the Open dialog and the desktop entry, and
  a test (`formats`) loads a sample of every listed format and checks that the desktop entry matches.
- **Quality profiles**: "Fast", "Standard" and "Best quality" bundle the palette quantizer, the number of
  refinement passes and the lookahead (`aic::quality_profiles()`, `aic::apply_quality_profile()`). The GUI
  has a "Quality profile" choice at the top of the HAM conversion settings that sets the three controls
  below it and follows them (it shows "Custom" when they match no profile); the command line has
  `--profile <name>` and `--list-profiles`, and the single options still override the profile in either
  order. The figures behind the three are in the README. A test (`profiles`) checks that "Standard" is
  the library default and that the profiles are ordered by quality on a sample picture.
- **Palette files in three formats**: palettes can be saved and loaded as an IFF ILBM picture, as a JASC
  palette (`.pal`, Paint Shop Pro) and as a GIMP palette (`.gpl`), in the GUI ("Load palette..." /
  "Save palette...") and on the command line (`--load-palette`, `--save-palette`). Saving picks the format
  from the file extension, loading from the content. Files that are not palettes of 16 colors are refused
  with the line number of the first problem. A test (`palette_formats`) round-trips all three formats,
  reads hand-written files from other programs and checks the refusals.
- **Linked source and preview** (GUI): zoom and position of the two views are coupled. What you zoom or
  scroll in one view (zoom keys and buttons, `Ctrl`+wheel, wheel, drag, scroll bars, fit) is done in the
  other so that both show the same part of the picture at the same size - also with a crop box, fit,
  fill or stretch, and non-square pixels. The new `aic::canvas_mapping()` (`aic/canvas_mapping.hpp`) is
  the map from a point of the source to its place on the HAM canvas and back, taken from the same
  geometry code the pipeline uses; a test (`canvas_mapping`) checks it against where the pipeline really
  puts a marker for every scaling mode, with crop boxes, with boxes reaching past the source and with
  non-square pixels. The link is the checkbox **Link zoom and pan** in the row of the source transforms; it is on
  by default and remembered.
- **Original view**: the result view choice has a fourth entry, **Original**, which shows the picture the
  encoder started from (cropped, scaled and color-corrected, before the 4096 colors) at the same zoom and
  position, so before and after can be flipped without the picture moving. `F6` cycles all four views,
  `F7` switches between the original and the one you were looking at.
- **Auto preview** (GUI): a toggle next to **Show HAM preview** converts again by itself when a setting
  changes - after a short pause without further changes - and once after opening a picture or rotating
  it. The newest request wins: a conversion that is still running is replaced. Off by default, not
  available for animations, remembered with the other defaults. `F5` works as before.

### Changed

- **The Debian package and the AppImage now run on Ubuntu 22.04 and Linux Mint 21** (they were built on
  Ubuntu 24.04 and needed its newer C library: "version `GLIBC_2.38' not found"). They are built on
  Ubuntu 22.04 now, with wxWidgets 3.2 linked statically because that release only has wxWidgets 3.0
  (`scripts/build_wx_linux.sh`; `WX_CONFIG` tells `build_deb.sh` and `build_appimage.sh` which wxWidgets
  to use). The Debian package no longer depends on a wxWidgets package and needs glibc 2.34 or newer;
  the AppImage needs glibc 2.35 or newer. The wxWidgets licence texts, and those of the libraries
  bundled with it, are part of both packages. The workflow installs both on clean Ubuntu 22.04, Ubuntu
  24.04 and Debian 12 systems and starts the GUI, and runs the tests on Ubuntu 22.04, too.
- GUI: "Lock current palette" now sits in the row of the "Current palette" caption instead of a row of its
  own. That makes room for the new "Quality profile" line, so the window is no taller than before and the
  whole Palette box still fits a 1080p screen.
- The "Load palette..." dialog lists all palette formats (and "All Files"); the "Save palette..." dialog
  has one filter per format and adds the extension of the selected one when the name has none. On Linux
  (GTK) the patterns accept upper-case extensions here, too; the file-filter code of the Open dialog moved
  to `FileFilters.cpp` for that.
- The GUI screenshot in the README shows the new layout.
- The Open dialog starts in the folder of the open picture (or of the most recent one), and the file
  name next to the buttons is shortened in the middle when it is too long (the tooltip shows the path).
- "All Files" in the Open dialog uses the platform's "everything" wildcard (`*`), so files without an
  extension show up there too.
- README and `THIRD_PARTY_NOTICES.md` state the origin and license of the example photos (also the
  cat photo in the GUI screenshot) and which third-party libraries the AppImage and the Flatpak contain.
- The README's project layout no longer mentions the removed development log.
- GUI: the zoom of source and result is no longer independent while **Link zoom and pan** is on (the
  default); untick it for the old behavior. The zoom keys (`Ctrl+=` and so on) act on the result view,
  the source follows; each view's own zoom buttons work on that view and the other follows.
- GUI: `F6` now cycles four result views (preview, HAM map, error map, original).
- GUI: more compact layout, so the whole window fits a 1080p screen: the settings below the pictures take
  fewer rows (Pixels/Percent beside "Enable crop"; width, height, X and Y in two rows; "Enable animation"
  in the row of the keyframe list; the export format, the scaling mode and the interlace flag side by
  side; dithering with its strength, and quantizer with color focus, in one row each; the 16 palette
  swatches in one row; the reset button of the color correction beside its sliders; the frame around the
  preview buttons and the one around the zoom buttons are gone; smaller gaps). At 1920x1080 all settings
  are visible and the two pictures are 260 px instead of 166 px high. The dither strength is shown as a
  percentage only; its tooltip says what it scales.

### Fixed

- The settings panel was cut off at the bottom in a window of the default size: the start height counted
  the picture panes at their bare minimum, while the layout later keeps them at 260 px and takes the
  difference from the settings. The default window width now also follows what the settings need, which
  is more in German than in English.
- The Open dialog on Linux (GTK) hid files whose extension is not lower case, such as the `IMG_0001.JPG`
  of many cameras: GTK matches the filter patterns case-sensitively. The patterns now accept every
  spelling.

## [0.1.0] - 2026-11-22

First public release.

### Added

- **Encoder**: Amiga HAM6 conversion in which palette, dithering and HAM operation are one decision per
  pixel, measured in OKLab; optional beam-search lookahead.
- **Quantizers** `kmeans` (default), `median-cut`, `octree`, with iterative palette refinement,
  color-focus maps and a chroma/luminance weight.
- **Ditherers**: Floyd-Steinberg, Atkinson, Sierra-Lite, Jarvis-Judice-Ninke, Bayer, blue noise, three
  check patterns, and none; adjustable strength.
- **Input handling**: crop, fit/fill/stretch, pixel aspect, rotate and mirror, EXIF orientation,
  transparency over a background color, output-only color correction.
- **Output**: IFF ILBM (HAM6, optional ByteRun1 compression, interlace flag), standalone palette files,
  PNG previews, HAM-operation map and error map; IFF ANIM (ANIM-5) animation files.
- **Quality figures**: mean OKLab error, blurred error and SSIM.
- **Command-line tool** `renderHAM`, **GUI** `renderHAMgui` (English and German) and the **library**
  `renderhamlib`.
- **GUI animation**: keyframes with linearly interpolated crop boxes, parallel frame rendering, a preview
  player with loop and ping-pong, export as numbered IFF files or one ANIM file; ordered-dither patterns
  that move with the picture so pans do not flicker.
- Debian package build script, seven automated tests, and independent verification scripts for the
  output files.
- MIT License, third-party notices, and a statement that the software is AI-generated.

[Unreleased]: https://github.com/ravvbit/renderham/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/ravvbit/renderham/releases/tag/v0.1.0
