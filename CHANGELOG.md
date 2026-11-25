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

- **Linux packages**: a GitHub Actions workflow (`.github/workflows/linux-packages.yml`) builds, tests
  and packages renderHAM for x86_64 Linux as a Debian package, an AppImage and a Flatpak, and drafts a
  GitHub release with checksums when a version tag is pushed.
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

### Changed

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

### Fixed

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
