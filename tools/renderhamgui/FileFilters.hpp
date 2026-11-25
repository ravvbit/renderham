// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// FileFilters.hpp
//
// The file-type filters of the GUI's file dialogs, built from the library's lists of the formats
// it can read and write (aic::supported_image_formats(), aic::supported_palette_formats()), so
// the dialogs, the desktop entry and the loaders cannot drift apart.
//
// A wxFileDialog wildcard is "Text (*.a;*.b)|patterns|Text 2|patterns 2|...". GTK compares the
// patterns case-sensitively, so there every letter is spelled in both cases ("*.[jJ][pP][gG]"):
// without that a camera's IMG_0001.JPG would not show up. The other platforms' dialogs ignore
// case themselves and do not know the bracket syntax.

#include <wx/string.h>

#include <string>
#include <vector>

#include "aic/aic.hpp"

namespace gui {

// "Images (*.png;*.jpg;...)|<patterns>|All Files|*" - the "Open Image" dialog.
wxString ImageFileWildcard();

// "Palettes (*.iff;*.pal;...)|...|Amiga IFF ILBM (...)|...|JASC...|...|GIMP...|...|All Files|*".
// Which format a file has is found out from its content, so every palette file is offered.
wxString PaletteLoadWildcard();

// One filter per palette format, in the order of aic::supported_palette_formats(), so that
// wxFileDialog::GetFilterIndex() indexes that list.
wxString PaletteSaveWildcard();

// The GUI's (translated) name of a palette format, e.g. "Amiga IFF ILBM".
wxString PaletteFormatName(aic::PaletteFileFormat format);

// The path a "Save palette" dialog should save to: `typed` as it is when it already ends in
// the extension of a palette format (whatever the filter says - the extension decides how
// the file is written, as it does on the command line), otherwise `typed` plus the default
// extension of the format `filter_index` selected. wxGTK does not append one itself.
wxString PaletteSavePath(const wxString& typed, int filter_index);

}  // namespace gui
