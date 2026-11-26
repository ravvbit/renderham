// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// Button icons from 8-bit alpha masks (TransformIcons.hpp, GuiIcons.hpp).
//
// The masks carry only the shape; the color is the theme's button-text color at the time the
// icon is made, so the icons read on light and dark themes alike (a fixed-color bitmap would
// vanish on one of them). When the theme changes while the program runs, RefreshMaskIcons() makes
// the icons again. Disabled buttons get their grayed look from a second bitmap (SetMaskIcon).

#include <wx/anybutton.h>
#include <wx/bitmap.h>
#include <wx/clntdata.h>
#include <wx/colour.h>
#include <wx/image.h>
#include <wx/settings.h>
#include <wx/window.h>

// `alpha` is size x size bytes. out_size 0 keeps the size; another value rescales smoothly, for
// the smaller icon next to a button's text.
inline wxBitmap MakeMaskIcon(const unsigned char* alpha, int size, const wxColour& color, int out_size = 0) {
    wxImage img(size, size);
    img.InitAlpha();
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            img.SetRGB(x, y, color.Red(), color.Green(), color.Blue());
            img.SetAlpha(x, y, alpha[y * size + x]);
        }
    }
    if (out_size > 0 && out_size != size) img.Rescale(out_size, out_size, wxIMAGE_QUALITY_HIGH);
    return wxBitmap(img);
}

// Gives a button its icon in the theme's text color and, for the disabled state, in the theme's
// gray text color. wxWidgets/GTK does not gray an icon of its own accord, so without the second
// bitmap a disabled symbol button looks exactly like an enabled one.
inline void ApplyMaskIcon(wxAnyButton* button, const unsigned char* alpha, int size, int out_size) {
    button->SetBitmap(MakeMaskIcon(alpha, size, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT), out_size));
    button->SetBitmapDisabled(MakeMaskIcon(alpha, size, wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT), out_size));
}

// What a button's icon is made from, kept with the button (as its client object, which the button
// deletes) so that RefreshMaskIcons() can make it again.
class MaskIconSource : public wxClientData {
public:
    MaskIconSource(const unsigned char* alpha, int size, int out_size)
        : alpha(alpha), size(size), out_size(out_size) {}
    const unsigned char* alpha;
    int size;
    int out_size;
};

inline void SetMaskIcon(wxAnyButton* button, const unsigned char* alpha, int size, int out_size = 0) {
    ApplyMaskIcon(button, alpha, size, out_size);
    button->SetClientObject(new MaskIconSource(alpha, size, out_size));
}

// Makes the icons of all buttons below `root` again, in the colors the theme has now (the theme
// changed from light to dark or back while the program was running).
inline void RefreshMaskIcons(wxWindow* root) {
    for (wxWindow* child : root->GetChildren()) {
        if (auto* button = dynamic_cast<wxAnyButton*>(child)) {
            if (auto* source = dynamic_cast<MaskIconSource*>(button->GetClientObject())) {
                ApplyMaskIcon(button, source->alpha, source->size, source->out_size);
            }
        }
        RefreshMaskIcons(child);
    }
}
