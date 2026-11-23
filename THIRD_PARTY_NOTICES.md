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

### Build and development tools (nothing is copied into the program)

- CMake, a C++17 compiler, and the system C++ runtime and threads library.
- GNU gettext (`msgfmt`) turns the German translation into a catalog during the build.
- The Python scripts under `scripts/` and `testdata/` use NumPy, Pillow and (for sample
  photos) scikit-image. They are development aids; they are not needed to build or run
  the program.
