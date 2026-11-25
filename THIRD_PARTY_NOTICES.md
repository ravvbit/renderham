# Third-party software

renderHAM itself is released under the [MIT License](LICENSE). It uses the following
third-party components. None of them restricts how renderHAM is licensed.

## Included in this repository

### stb single-file libraries (`third_party/stb/`)

| File | Version | Purpose |
|---|---|---|
| `stb_image.h` | 2.30 | Reading PNG, JPEG, BMP, TGA, ... |
| `stb_image_write.h` | 1.16 | Writing the PNG preview images |
| `stb_image_resize2.h` | 2.18 | Scaling to the target resolution |

Author: Sean Barrett and contributors (`stb_image_resize2.h`: Jeff Roberts and Jorge L Rodriguez).
Source: <https://github.com/nothings/stb>

The stb libraries are available under **your choice of** the MIT License or the Unlicense
(public domain). The full text of both is at the end of each header file, and the files are
included here unmodified. Their MIT copyright line is
`Copyright (c) 2017 Sean Barrett`.

## Used but not included

### wxWidgets (GUI only)

`renderHAMgui` is built against [wxWidgets](https://www.wxwidgets.org/) 3.0 or newer, which
is installed separately (on Debian/Ubuntu: `libwxgtk3.2-dev`). It is licensed under the
**wxWindows Library Licence 3.1**, which is the LGPL plus an exception that lets you
distribute binaries of programs that use the library under terms of your own choice. The
exception is why a permissively licensed program such as renderHAM can be linked against it,
statically or dynamically, without the program itself becoming LGPL or GPL.

The `renderham` Debian package built by `scripts/build_deb.sh` links wxWidgets dynamically
and does not contain any wxWidgets code. If you distribute a binary that contains wxWidgets
(for example a statically linked Windows build), you must also pass on wxWidgets' own licence
text and notices with it.

The command-line tool `renderHAM` and the library `renderhamlib` do not use wxWidgets.

### Binary packages that contain libraries

The `.deb` package does not contain third-party libraries; it depends on the distribution's
packages. The other two Linux packages, built by `scripts/build_appimage.sh` and
`packaging/linux/flatpak/`, do:

- **AppImage** (built on Ubuntu 24.04): contains copies of wxWidgets and of the GTK 3 stack and
  other libraries of that release (GTK, GLib, Pango, Cairo, ATK, HarfBuzz and their
  dependencies). Most of these are licensed under the LGPL-2.1-or-later, others under MIT, BSD
  and similar licences. The copyright files that the distribution provides for them are
  included inside the AppImage under `usr/share/doc/`. The source code is available from the
  Ubuntu 24.04 source packages (`apt source <package>`) and from the upstream projects. An
  AppImage can be unpacked with `--appimage-extract`, so the libraries in it can be replaced
  by modified versions. The AppImage file also embeds the AppImage runtime added by
  `appimagetool` (see <https://github.com/AppImage/type2-runtime> for its licences).
- **Flatpak**: wxWidgets 3.2.12 is compiled from the upstream source tarball named in the
  manifest, and its licence texts are installed under `share/doc/wxWidgets/`. Everything else
  comes from the GNOME runtime, under the licences of the runtime's components.

### Build and development tools (nothing is copied into the program)

- CMake, a C++17 compiler, and the system C++ runtime and threads library.
- GNU gettext (`msgfmt`) turns the German translation into a catalog during the build.
- `scripts/build_appimage.sh` downloads linuxdeploy, its GTK plugin and appimagetool (MIT-licensed
  tools, pinned by SHA-256) to assemble the AppImage; flatpak-builder builds the Flatpak.
- The Python scripts under `scripts/` and `testdata/` use NumPy, Pillow and (for sample
  photos) scikit-image. They are development aids; they are not needed to build or run
  the program.

## Example images

The photos in `docs/images/` come from the sample data of
[scikit-image](https://scikit-image.org/docs/stable/api/skimage.data.html) and are released as
CC0 by their photographers: the cat photo ("chelsea", also visible in the GUI screenshot) by
Stefan van der Walt, the coffee photo by Rachel Michetti (courtesy of Pikolo Espresso Bar).
They are not part of the program and not covered by its MIT License.
