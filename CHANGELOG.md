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

### Changed

- README and `THIRD_PARTY_NOTICES.md` state the origin and license of the example photos (also the
  cat photo in the GUI screenshot) and which third-party libraries the AppImage and the Flatpak contain.
- The README's project layout no longer mentions the removed development log.

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
