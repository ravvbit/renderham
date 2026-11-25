// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "MainFrame.hpp"
#include "FileFilters.hpp"
#include "GuiIcons.hpp"
#include "IconUtil.hpp"
#include "TransformIcons.hpp"

#include <wx/config.h>
#include <wx/dirdlg.h>
#include <wx/display.h>
#include <wx/settings.h>
#include <wx/dnd.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "SettingsDialog.hpp"
#include "aic/ham_maps.hpp"
#include "AboutDialog.hpp"

namespace {

const int ID_CONVERT = wxID_HIGHEST + 1;
const int ID_ZOOM_IN = wxID_HIGHEST + 2;
const int ID_ZOOM_OUT = wxID_HIGHEST + 3;
const int ID_ZOOM_FIT = wxID_HIGHEST + 4;
const int ID_ZOOM_ACTUAL = wxID_HIGHEST + 5;
const int ID_SETTINGS = wxID_HIGHEST + 6;
// Deliberately NOT using wxID_OPEN/wxID_SAVEAS anywhere (button or menu
// item): on GTK, a wxButton (and possibly wxMenuItem, on some
// wx/GTK/theme combinations) constructed with certain stock IDs can
// render GTK's own built-in stock label - driven by the C library
// locale, independently of wxLocale - instead of the explicit text
// passed to the constructor. Not reproducible in the development
// environment (English showed correctly even after forcing LANG=de_DE.UTF-8,
// and no wxstd.mo was installed to demonstrate the mechanism), but avoiding stock
// IDs for these two actions removes the whole class of risk regardless
// of the exact trigger on any given system/theme.
const int ID_OPEN_BTN = wxID_HIGHEST + 7;
const int ID_SAVE_BTN = wxID_HIGHEST + 8;
const int ID_SAVE_DEFAULTS = wxID_HIGHEST + 9;
const int ID_TARGET_DIR_BTN = wxID_HIGHEST + 10;
const int ID_SAVE_QUICK_BTN = wxID_HIGHEST + 11;
const int ID_ABOUT = wxID_HIGHEST + 12;
const int ID_ADD_KEYFRAME_SHORTCUT = wxID_HIGHEST + 13;
const int ID_QUIT_SHORTCUT = wxID_HIGHEST + 14;
const int ID_CYCLE_VIEW = wxID_HIGHEST + 15;
const int ID_ROTATE_LEFT = wxID_HIGHEST + 16;
const int ID_ROTATE_RIGHT = wxID_HIGHEST + 17;
const int ID_FLIP_HORIZONTAL = wxID_HIGHEST + 18;
const int ID_FLIP_VERTICAL = wxID_HIGHEST + 19;
const int ID_CANCEL_JOB = wxID_HIGHEST + 20;
const int ID_JOB_TIMER = wxID_HIGHEST + 21;
const int ID_RECENT_BTN = wxID_HIGHEST + 22;   // the button, and Ctrl+Shift+O
const int ID_CLEAR_RECENT = wxID_HIGHEST + 23;
const int ID_TOGGLE_ORIGINAL = wxID_HIGHEST + 24;
const int ID_AUTO_PREVIEW_TIMER = wxID_HIGHEST + 25;
// Pause after the last change of a setting before auto preview converts again: long enough to
// finish a slider drag or a few key presses, short enough to feel like it follows. A newly
// opened or turned picture does not need to wait for anything.
const int kAutoPreviewPauseMs = 700;
const int kAutoPreviewAfterLoadMs = 150;
// The entries of the Recent menu take this id and the following ones (wxFileHistory numbers them).
const int ID_RECENT_FIRST = wxID_HIGHEST + 30;
const int kMaxRecentFiles = 9;  // the menu numbers them 1-9, so a digit key picks one

// aic::Image (float RGB) -> wxImage (byte RGB). wxImage takes ownership
// of a malloc'd buffer when static_data=false (its default), matching
// wxWidgets' own C allocator convention for owned pixel data.
wxImage ToWxImage(const aic::Image& img) {
    int w = img.width(), h = img.height();
    auto* buf = static_cast<unsigned char*>(std::malloc(static_cast<size_t>(w) * h * 3));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const aic::RGBf& c = img.at(x, y);
            size_t idx = (static_cast<size_t>(y) * w + x) * 3;
            buf[idx + 0] = static_cast<unsigned char>(std::clamp(c.r, 0.f, 1.f) * 255.f + 0.5f);
            buf[idx + 1] = static_cast<unsigned char>(std::clamp(c.g, 0.f, 1.f) * 255.f + 0.5f);
            buf[idx + 2] = static_cast<unsigned char>(std::clamp(c.b, 0.f, 1.f) * 255.f + 0.5f);
        }
    }
    return wxImage(w, h, buf, false);
}

// "&1 photo.jpg  -  /home/olli/Pictures": file name first (what one scans for), then the folder,
// shortened in the middle when it is long. The digit is the menu mnemonic, so pressing 1-9 picks
// an entry. '&' in a name must be doubled or the menu would take it for a mnemonic.
wxString RecentMenuLabel(size_t index, const wxString& path) {
    auto escaped = [](wxString text) {
        text.Replace("&", "&&");
        return text;
    };
    const wxFileName name(path);
    wxString folder = name.GetPath();
    constexpr size_t kMaxFolderChars = 48;
    if (folder.length() > kMaxFolderChars)
        folder = folder.Left(20) + wxString::FromUTF8("\xE2\x80\xA6") + folder.Right(27);
    wxString label = wxString::Format("&%d %s", static_cast<int>(index) + 1, escaped(name.GetFullName()));
    if (!folder.empty()) label += "  -  " + escaped(folder);
    return label;
}

// Accepts a single dropped file and forwards its path; used for both the
// frame itself and the two preview panes so a drop lands correctly
// wherever the user releases it.
class FileDropTarget : public wxFileDropTarget {
public:
    explicit FileDropTarget(std::function<void(const wxString&)> on_file) : on_file_(std::move(on_file)) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override {
        if (!filenames.IsEmpty() && on_file_) on_file_(filenames[0]);
        return true;
    }

private:
    std::function<void(const wxString&)> on_file_;
};

}  // namespace

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, "renderHAMgui", wxDefaultPosition, wxSize(820, 200)) {
    Init();
}

MainFrame::MainFrame(const wxString& initial_image_path)
    : wxFrame(nullptr, wxID_ANY, "renderHAMgui", wxDefaultPosition, wxSize(820, 200)) {
    Init();
    LoadImage(initial_image_path);
}

wxSizer* MainFrame::BuildPreviewHeader(const wxString& title, ImageView* view, wxStaticText** out_zoom_label) {
    auto* header = new wxBoxSizer(wxHORIZONTAL);
    auto* title_text = new wxStaticText(this, wxID_ANY, title);
    // Without this, "Source" vs. the longer "Amiga Preview (HAM6)"
    // give the two header rows different natural minimum widths, which
    // can visibly skew the 50/50 split between the two ImageViews by a
    // couple of pixels once the row is tight enough for it to matter
    // (it wasn't with the old, narrower header - see the "equal
    // preview sizes" fix elsewhere in this file for why this matters).
    // The stretch behavior (proportion 1 below) is what actually makes
    // it fill the row; this just stops its own text length from being
    // the deciding factor for how much space that leaves the zoom box.
    title_text->SetMinSize(wxSize(60, -1));
    header->Add(title_text, 1, wxALIGN_CENTER_VERTICAL);

    // Half-width +/- buttons: 85x34 is this button's natural (best)
    // size on this GTK theme/font - halving just the width keeps them
    // tappable while visibly narrower than the text buttons next to them.
    const wxSize kNarrowBtn(42, -1);
    auto* zoom_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Zoom"));
    // Symbols with tooltips instead of "100%", "Fit", "-", "+": four equally narrow buttons
    // that say the same in less room (the percentage next to them shows the current zoom).
    const wxColour icon_color = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
    auto make_zoom_btn = [&](const unsigned char* icon, const wxString& tip) {
        auto* b = new wxBitmapButton(zoom_box->GetStaticBox(), wxID_ANY, MakeMaskIcon(icon, kGuiIconSize, icon_color),
                                     wxDefaultPosition, kNarrowBtn);
        SetMaskIcon(b, icon, kGuiIconSize);
        b->SetToolTip(tip);
        return b;
    };
    auto* zoom_actual_btn = make_zoom_btn(kIconZoomActual, _("Show at 100% (one picture pixel per screen pixel)"));
    auto* zoom_fit_btn = make_zoom_btn(kIconZoomFit, _("Fit the whole picture into the window"));
    auto* zoom_out_btn = make_zoom_btn(kIconZoomOut, _("Zoom out"));
    auto* zoom_in_btn = make_zoom_btn(kIconZoomIn, _("Zoom in"));
    auto* zoom_label = new wxStaticText(zoom_box->GetStaticBox(), wxID_ANY, "100%", wxDefaultPosition, wxSize(45, -1),
                                         wxALIGN_RIGHT);
    zoom_box->Add(zoom_actual_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    zoom_box->Add(zoom_fit_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    zoom_box->Add(zoom_out_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    zoom_box->Add(zoom_in_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    zoom_box->Add(zoom_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
    header->Add(zoom_box, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

    // The percentage follows the view itself (see OnViewChanged), whoever zoomed it: these buttons,
    // the wheel, the keyboard, or the other view when the two are linked.
    zoom_out_btn->Bind(wxEVT_BUTTON, [view](wxCommandEvent&) { view->ZoomOut(); });
    zoom_fit_btn->Bind(wxEVT_BUTTON, [view](wxCommandEvent&) { view->ZoomToFit(); });
    zoom_actual_btn->Bind(wxEVT_BUTTON, [view](wxCommandEvent&) { view->ZoomActual(); });
    zoom_in_btn->Bind(wxEVT_BUTTON, [view](wxCommandEvent&) { view->ZoomIn(); });

    if (out_zoom_label) *out_zoom_label = zoom_label;
    return header;
}

void MainFrame::Init() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    // Toolbar row.
    auto* toolbar_row = new wxBoxSizer(wxHORIZONTAL);
    auto* open_btn = new wxButton(this, ID_OPEN_BTN, _("Open..."));
    open_btn->SetToolTip(_("Open an image (Ctrl+O)"));
    recent_btn_ = new wxButton(this, ID_RECENT_BTN, _("Recent"));
    recent_btn_->SetToolTip(_("Open one of the images opened before (Ctrl+Shift+O)"));
    // Ellipsized in the middle (the end of a file name matters: its extension), so a long name never
    // pushes the buttons next to it out of the window; the tooltip holds the full path.
    input_label_ = new wxStaticText(this, wxID_ANY, _("(no file loaded - or drag an image here)"), wxDefaultPosition,
                                     wxDefaultSize, wxST_ELLIPSIZE_MIDDLE);
    input_label_->SetMinSize(wxSize(100, -1));
    target_dir_btn_ = new wxButton(this, ID_TARGET_DIR_BTN, _("Target directory..."));
    save_quick_btn_ = new wxButton(this, ID_SAVE_QUICK_BTN, _("Save"));
    save_quick_btn_->SetToolTip(_("Save to the target directory (Ctrl+S)"));
    save_as_btn_ = new wxButton(this, ID_SAVE_BTN, _("Save As..."));
    save_as_btn_->SetToolTip(_("Save with a file picker (Ctrl+Shift+S)"));
    toolbar_row->Add(open_btn, 0, wxALL, 4);
    toolbar_row->Add(recent_btn_, 0, wxTOP | wxBOTTOM | wxRIGHT, 4);
    toolbar_row->Add(input_label_, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar_row->Add(target_dir_btn_, 0, wxALL, 4);
    toolbar_row->Add(save_quick_btn_, 0, wxALL, 4);
    toolbar_row->Add(save_as_btn_, 0, wxALL, 4);
    toolbar_row_ = toolbar_row;
    root->Add(toolbar_row, 0, wxEXPAND);

    // Preview row - both columns share BuildPreviewHeader (identical
    // layout => identical ImageView size) and get proportion 1 +
    // wxEXPAND, so the whole row grows when the frame is resized.
    auto* preview_row = new wxBoxSizer(wxHORIZONTAL);

    source_view_ = new ImageView(this);
    source_view_->SetZoomEnabled(true);
    source_view_->SetMinSize(wxSize(150, 100));

    preview_view_ = new ImageView(this);
    preview_view_->SetZoomEnabled(true);
    preview_view_->SetMinSize(wxSize(150, 100));

    auto* src_col = new wxBoxSizer(wxVERTICAL);
    src_col->Add(BuildPreviewHeader(_("Source"), source_view_, &source_zoom_label_), 0, wxEXPAND | wxBOTTOM, 2);

    auto* prev_col = new wxBoxSizer(wxVERTICAL);
    prev_col->Add(BuildPreviewHeader(_("Amiga Preview (HAM6)"), preview_view_, &preview_zoom_label_), 0,
                  wxEXPAND | wxBOTTOM, 2);

    // Only meaningful for the decoded HAM6 result, not the source photo:
    // an interlaced Amiga screen mode has double the vertical pixel
    // count, but occupies the SAME physical height on a real CRT as a
    // non-interlaced mode - the two fields are shown alternately, not
    // stacked. Checked, the preview approximates that by scaling the
    // decoded frame to 50% height (see UpdatePreviewDisplay) instead of
    // showing it at native pixel size, which would look implausibly
    // tall/stretched compared to a real screen.
    //
    // Reserves the identical row height on the source side too (via the
    // real checkbox's own measured best size) purely so both ImageViews
    // stay pixel-equal in size - see the "equal preview sizes" fix
    // elsewhere in this file for why that matters.
    interlace_scale_check_ = new wxCheckBox(this, wxID_ANY, _("Simulate interlace (half height)"));

    // Which image the preview panel shows: the decoded HAM6 result, a map of
    // the HAM operation behind every pixel, or an error map (see
    // aic/ham_maps.hpp). Shares the checkbox's row so the header does not
    // get taller; F6 cycles through the four.
    preview_mode_ = new wxChoice(this, wxID_ANY);
    preview_mode_->Append(_("Preview"));
    preview_mode_->Append(_("HAM map"));
    preview_mode_->Append(_("Error map"));
    preview_mode_->Append(_("Original"));
    preview_mode_->SetSelection(0);
    preview_mode_->SetToolTip(
        _("What the preview shows: the decoded HAM6 result, a map of the HAM operation used for each pixel "
          "(black = Select, red/green/blue = Modify R/G/B), an error map (brightness = distance from the "
          "source image, white = 0.10 OKLab or more), or the original: the picture as the encoder received "
          "it (cropped, scaled, color-corrected), at the size of the preview - the \"before\" to the HAM6 "
          "\"after\", in the same place at the same zoom. F6 cycles through the views, F7 switches between "
          "the original and what was shown before. The error map and the original are only available for "
          "the single-frame preview, not while an animation plays."));

    int interlace_row_height =
        std::max(interlace_scale_check_->GetBestSize().GetHeight(), preview_mode_->GetBestSize().GetHeight());
    auto* option_row = new wxBoxSizer(wxHORIZONTAL);
    option_row->Add(interlace_scale_check_, 0, wxALIGN_CENTER_VERTICAL);
    option_row->AddStretchSpacer(1);
    option_row->Add(preview_mode_, 0, wxALIGN_CENTER_VERTICAL);
    prev_col->Add(option_row, 0, wxEXPAND | wxBOTTOM, 2);
    // Rotate/mirror the source picture. Sits in the row that otherwise only
    // reserves the height of the interlace checkbox on the preview side, so
    // both ImageViews keep the same size.
    {
        auto* transform_row = new wxBoxSizer(wxHORIZONTAL);
        const wxSize square(std::max(interlace_row_height, 28), interlace_row_height);
        struct Spec { int id; const unsigned char* icon; wxString tip; };
        const Spec specs[] = {
            {ID_ROTATE_LEFT, kIconRotateLeft, _("Rotate the source 90 degrees to the left (Ctrl+L)")},
            {ID_ROTATE_RIGHT, kIconRotateRight, _("Rotate the source 90 degrees to the right (Ctrl+R)")},
            {ID_FLIP_HORIZONTAL, kIconFlipHorizontal, _("Mirror the source left to right (Ctrl+Shift+H)")},
            {ID_FLIP_VERTICAL, kIconFlipVertical, _("Mirror the source top to bottom (Ctrl+Shift+V)")},
        };
        const wxColour icon_color = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
        for (const Spec& sp : specs) {
            auto* btn = new wxBitmapButton(this, sp.id, MakeMaskIcon(sp.icon, kTransformIconSize, icon_color), wxDefaultPosition, square);
            SetMaskIcon(btn, sp.icon, kTransformIconSize);  // + the grayed look for "no image loaded yet"
            btn->SetToolTip(sp.tip);
            btn->Enable(false);  // until an image is loaded
            transform_row->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
            transform_buttons_.push_back(btn);
        }
        // Right-aligned in the same row, which has room for it: the two views show the same part
        // of the picture when this is on (see OnViewChanged).
        link_views_check_ = new wxCheckBox(this, wxID_ANY, _("Link zoom and pan"));
        link_views_check_->SetToolTip(
            _("Zoom and pan the source and the preview together, so both always show the same part of the "
              "picture - the crop box and the scaling are taken into account, so a detail is in the same "
              "place in both views."));
        link_views_check_->SetValue(wxConfig::Get()->ReadBool("View/LinkZoomPan", true));
        transform_row->AddStretchSpacer(1);
        transform_row->Add(link_views_check_, 0, wxALIGN_CENTER_VERTICAL);
        src_col->Add(transform_row, 0, wxEXPAND | wxBOTTOM, 2);
    }

    src_col->Add(source_view_, 1, wxEXPAND);
    prev_col->Add(preview_view_, 1, wxEXPAND);

    // Equal-width columns with a plain spacer between them (not a
    // one-sided border on just one column - that would consume space
    // asymmetrically and leave the two ImageViews a few pixels
    // different in width despite both having proportion 1).
    preview_row->Add(src_col, 1, wxEXPAND);
    preview_row->AddSpacer(4);
    preview_row->Add(prev_col, 1, wxEXPAND);
    root->Add(preview_row, 1, wxEXPAND | wxALL, 4);

    // Parameters - fixed natural height, stretches to the frame's width.
    params_ = new ParameterPanel(this);
    params_->LoadDefaults();
    // A scrolled window reports only a small best size of its own; the minimum
    // height is what makes the sizer give it room. Starts at the natural height
    // (everything visible); UpdateParamsPanelHeight() lowers it on small screens.
    params_->SetMinSize(wxSize(wxDefaultCoord, params_->NaturalHeight()));
    root->Add(params_, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    // Status bar + action buttons.
    auto* status_row = new wxBoxSizer(wxHORIZONTAL);
    auto* save_defaults_btn = new wxButton(this, ID_SAVE_DEFAULTS, _("Save current settings"));
    auto* settings_btn = new wxButton(this, ID_SETTINGS, _("Settings..."));
    // Ellipsized, so a long message (a path, the refusal while an export runs) never paints
    // over the progress bar next to it; the minimum size keeps it from demanding the full text.
    status_label_ = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    status_label_->SetMinSize(wxSize(100, -1));
    status_label_->SetToolTip(
        _("After a conversion: error = mean OKLab distance per pixel (lower is better); blurred = the same "
          "after blurring both pictures a little (sigma 1.5 px), the way eye and monitor mix neighboring "
          "pixels - this one rewards dithering; SSIM = structural similarity of the lightness, 1 = "
          "identical (it sees dither noise as foreign structure, so dithering lowers it). The three do "
          "not add up to one score: compare conversions by the figure that matters for your picture."));
    // Progress of a running background job, and the way to stop it. Only visible while
    // a job runs (see BeginJob/EndJobUi).
    job_gauge_ = new wxGauge(this, wxID_ANY, 1000, wxDefaultPosition, wxSize(150, 16), wxGA_HORIZONTAL | wxGA_SMOOTH);
    job_cancel_btn_ = new wxButton(this, ID_CANCEL_JOB, _("Cancel"));
    job_cancel_btn_->SetToolTip(_("Stop the running conversion (Esc)"));
    SetMaskIcon(job_cancel_btn_, kIconCancel, kGuiIconSize, 16);
    job_gauge_->Hide();
    job_cancel_btn_->Hide();
    auto* about_btn = new wxButton(this, ID_ABOUT, _("About"));
    status_row->Add(save_defaults_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    status_row->Add(settings_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    status_row->Add(status_label_, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    status_row->Add(job_gauge_, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    status_row->Add(job_cancel_btn_, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    status_row->Add(about_btn, 0, wxALL, 4);
    status_row_ = status_row;
    root->Add(status_row, 0, wxEXPAND);

    SetSizer(root);
    root->SetSizeHints(this);
    // The sizer's own natural minimum (computed just above, ~1114x768)
    // is really more of a "comfortable" size than a hard requirement -
    // every control still renders correctly, just more tightly packed,
    // well below that. Overriding down to 700x500 was verified by
    // forcing the window to exactly that size and checking that the
    // rightmost control in the parameter panel (the compression
    // checkbox) still ends up fully inside the window bounds, not
    // clipped.
    SetMinSize(wxSize(700, 500));

    // Restore the last window geometry if there is one (saved when the window
    // closes and by "Save current settings"); otherwise fall back to a
    // computed default size, centered on screen. Either way the size is
    // clamped to the display's work area: the natural height of the layout
    // (about 1020 px) does not fit a 1366x768 laptop screen, which used to push
    // the title bar and menu off the top of the screen.
    wxConfigBase* cfg = wxConfig::Get();
    long win_w, win_h, win_x, win_y;
    bool have_geometry = cfg->Read("Defaults/WindowWidth", &win_w) && cfg->Read("Defaults/WindowHeight", &win_h) &&
                         cfg->Read("Defaults/WindowX", &win_x) && cfg->Read("Defaults/WindowY", &win_y);
    // Frame decorations are not known before the window is shown; leave room
    // for a title bar and a bit of border.
    constexpr int kDecorationSlackW = 20;
    constexpr int kDecorationSlackH = 60;
    int saved_display = wxNOT_FOUND;
    if (have_geometry && win_w > 0 && win_h > 0) {
        // Only trust the saved position if its title-bar area is still on a
        // connected display (a monitor may have been unplugged since).
        saved_display = wxDisplay::GetFromPoint(wxPoint(static_cast<int>(win_x) + 40, static_cast<int>(win_y) + 10));
    }
    if (saved_display != wxNOT_FOUND) {
        wxRect area = wxDisplay(static_cast<unsigned>(saved_display)).GetClientArea();
        int w = std::min(static_cast<int>(win_w), area.GetWidth());
        int h = std::min(static_cast<int>(win_h), area.GetHeight());
        int x = std::clamp(static_cast<int>(win_x), area.GetLeft(), std::max(area.GetLeft(), area.GetRight() - w + 1));
        int y = std::clamp(static_cast<int>(win_y), area.GetTop(), std::max(area.GetTop(), area.GetBottom() - h + 1));
        SetSize(x, y, w, h);
        if (cfg->ReadBool("Defaults/WindowMaximized", false)) Maximize();
    } else {
        wxRect area = wxDisplay().GetClientArea();  // primary display
        wxSize best = GetBestSize();  // includes the panel's natural height, see the SetMinSize below
        int w = std::min(std::max(best.GetWidth(), 900), area.GetWidth() - kDecorationSlackW);
        int h = std::min(std::max(best.GetHeight(), 820), area.GetHeight() - kDecorationSlackH);
        SetSize(wxSize(std::max(w, 700), std::max(h, 500)));
        CentreOnScreen();
    }
    UpdateParamsPanelHeight();
    Bind(wxEVT_SIZE, &MainFrame::OnSize, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnCloseWindow, this);

    // Persisted immediately on selection (see OnTargetDirectory), same
    // as the language setting - not tied to "Save current settings".
    {
        wxColour bg(cfg->Read("TransparentBackground", "#000000"));
        if (bg.IsOk()) alpha_background_ = aic::RGBf{bg.Red() / 255.f, bg.Green() / 255.f, bg.Blue() / 255.f};
    }
    target_dir_ = cfg->Read("TargetDirectory", wxEmptyString);
    UpdateTargetDirTooltip();
    // Only the storage (order, duplicates, size limit) comes from wxFileHistory; the menu is built in
    // ShowRecentMenu, because the labels wxFileHistory makes show either the bare name or the whole path.
    recent_files_ = std::make_unique<wxFileHistory>(kMaxRecentFiles, ID_RECENT_FIRST);
    LoadRecentFiles();

    open_btn->Bind(wxEVT_BUTTON, &MainFrame::OnOpen, this);
    // Button and Ctrl+Shift+O share the id (like Open); the entries (and "Clear list") are menu events.
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowRecentMenu(); }, ID_RECENT_BTN);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { ShowRecentMenu(); }, ID_RECENT_BTN);
    Bind(wxEVT_MENU, &MainFrame::OnRecentFile, this, ID_RECENT_FIRST, ID_RECENT_FIRST + kMaxRecentFiles - 1);
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            while (recent_files_->GetCount() > 0) recent_files_->RemoveFileFromHistory(0);
            SaveRecentFiles();
            SetStatus(_("List of recent images cleared"));
        },
        ID_CLEAR_RECENT);
    target_dir_btn_->Bind(wxEVT_BUTTON, &MainFrame::OnTargetDirectory, this);
    save_quick_btn_->Bind(wxEVT_BUTTON, &MainFrame::OnSaveQuick, this);
    save_as_btn_->Bind(wxEVT_BUTTON, &MainFrame::OnSaveAs, this);
    settings_btn->Bind(wxEVT_BUTTON, &MainFrame::OnSettings, this);
    save_defaults_btn->Bind(wxEVT_BUTTON, &MainFrame::OnSaveAsDefaults, this);
    interlace_scale_check_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { UpdatePreviewDisplay(); });
    preview_mode_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { UpdatePreviewDisplay(); });
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) {
            preview_mode_->SetSelection((preview_mode_->GetSelection() + 1) % static_cast<int>(preview_mode_->GetCount()));
            UpdatePreviewDisplay();
        },
        ID_CYCLE_VIEW);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { ToggleOriginalView(); }, ID_TOGGLE_ORIGINAL);
    about_btn->Bind(wxEVT_BUTTON, &MainFrame::OnAbout, this);
    // Buttons and keyboard shortcuts share the IDs, so one handler each.
    auto bind_transform = [this](int id, aic::ImageTransform delta, wxString text) {
        auto handler = [this, delta, text](wxCommandEvent&) { ApplySourceTransform(delta, text); };
        Bind(wxEVT_BUTTON, handler, id);
        Bind(wxEVT_MENU, handler, id);
    };
    bind_transform(ID_ROTATE_LEFT, aic::ImageTransform::rotate_cw(-1), _("Source rotated to the left"));
    bind_transform(ID_ROTATE_RIGHT, aic::ImageTransform::rotate_cw(1), _("Source rotated to the right"));
    bind_transform(ID_FLIP_HORIZONTAL, aic::ImageTransform::flip_horizontal(), _("Source mirrored left to right"));
    bind_transform(ID_FLIP_VERTICAL, aic::ImageTransform::flip_vertical(), _("Source mirrored top to bottom"));
    // ID_CONVERT/F5 now triggers whatever the (former Convert, now
    // merged into Play preview) button currently does - see
    // OnPlayPreviewToggled.
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnPlayPreviewToggled(); }, ID_CONVERT);
    // The remaining shortcuts route to exactly the same handlers their
    // buttons already use (see the accel_entries block above for why
    // these particular keys were chosen).
    Bind(wxEVT_MENU, &MainFrame::OnOpen, this, ID_OPEN_BTN);
    Bind(wxEVT_MENU, &MainFrame::OnSaveQuick, this, ID_SAVE_QUICK_BTN);
    Bind(wxEVT_MENU, &MainFrame::OnSaveAs, this, ID_SAVE_BTN);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { params_->TriggerAddKeyframe(); }, ID_ADD_KEYFRAME_SHORTCUT);
    Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, ID_QUIT_SHORTCUT);
    // Cancel button and Esc share the ID, like the other button/shortcut pairs.
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CancelRunningJob(); }, ID_CANCEL_JOB);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { CancelRunningJob(); }, ID_CANCEL_JOB);
    job_timer_.SetOwner(this, ID_JOB_TIMER);
    Bind(wxEVT_TIMER, &MainFrame::OnJobTimer, this, ID_JOB_TIMER);
    auto_preview_timer_.SetOwner(this, ID_AUTO_PREVIEW_TIMER);
    Bind(wxEVT_TIMER, &MainFrame::OnAutoPreviewTimer, this, ID_AUTO_PREVIEW_TIMER);

    // The keys zoom the preview; with the views linked the source goes along.
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { preview_view_->ZoomIn(); }, ID_ZOOM_IN);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { preview_view_->ZoomOut(); }, ID_ZOOM_OUT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { preview_view_->ZoomToFit(); }, ID_ZOOM_FIT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { preview_view_->ZoomActual(); }, ID_ZOOM_ACTUAL);

    // Chosen deliberately, not just "whatever's free": Ctrl+O/S/Shift+S
    // match the near-universal open/save/save-as convention every
    // desktop app shares, so they need no explanation. F5 (Convert /
    // Play preview, depending on Animation) was already established.
    // Ctrl+K adds a keyboard path for the single action repeated most
    // during animation setup (capturing one keyframe after another) -
    // K for "Keyframe", unclaimed by anything else here. Ctrl+Q for
    // Quit is the same universal convention as Ctrl+O/S, even though
    // the window's own close button/Alt+F4 already cover it.
    wxAcceleratorEntry accel_entries[18];
    accel_entries[0].Set(wxACCEL_NORMAL, WXK_F5, ID_CONVERT);
    accel_entries[1].Set(wxACCEL_CTRL, static_cast<int>('='), ID_ZOOM_IN);
    accel_entries[2].Set(wxACCEL_CTRL, static_cast<int>('-'), ID_ZOOM_OUT);
    accel_entries[3].Set(wxACCEL_CTRL, static_cast<int>('9'), ID_ZOOM_FIT);
    accel_entries[4].Set(wxACCEL_CTRL, static_cast<int>('0'), ID_ZOOM_ACTUAL);
    accel_entries[5].Set(wxACCEL_CTRL, static_cast<int>('O'), ID_OPEN_BTN);
    accel_entries[6].Set(wxACCEL_CTRL, static_cast<int>('S'), ID_SAVE_QUICK_BTN);
    accel_entries[7].Set(wxACCEL_CTRL | wxACCEL_SHIFT, static_cast<int>('S'), ID_SAVE_BTN);
    accel_entries[8].Set(wxACCEL_CTRL, static_cast<int>('K'), ID_ADD_KEYFRAME_SHORTCUT);
    accel_entries[9].Set(wxACCEL_CTRL, static_cast<int>('Q'), ID_QUIT_SHORTCUT);
    accel_entries[10].Set(wxACCEL_NORMAL, WXK_F6, ID_CYCLE_VIEW);
    accel_entries[11].Set(wxACCEL_CTRL, static_cast<int>('L'), ID_ROTATE_LEFT);
    accel_entries[12].Set(wxACCEL_CTRL, static_cast<int>('R'), ID_ROTATE_RIGHT);
    accel_entries[13].Set(wxACCEL_CTRL | wxACCEL_SHIFT, static_cast<int>('H'), ID_FLIP_HORIZONTAL);
    accel_entries[14].Set(wxACCEL_CTRL | wxACCEL_SHIFT, static_cast<int>('V'), ID_FLIP_VERTICAL);
    accel_entries[15].Set(wxACCEL_NORMAL, WXK_ESCAPE, ID_CANCEL_JOB);
    accel_entries[16].Set(wxACCEL_CTRL | wxACCEL_SHIFT, static_cast<int>('O'), ID_RECENT_BTN);
    accel_entries[17].Set(wxACCEL_NORMAL, WXK_F7, ID_TOGGLE_ORIGINAL);
    SetAcceleratorTable(wxAcceleratorTable(18, accel_entries));

    // Bidirectional crop <-> rubber-band sync (see the two methods'
    // comments) and drag-and-drop file loading, covering the frame
    // itself plus both preview panes so a drop lands correctly wherever
    // the user releases it.
    params_->SetCropChangedCallback([this] {
        SyncCropOverlayFromParams();
        // The keyframe just selected (or the crop just edited) may no
        // longer match whatever preview_frames_ currently holds.
        StopAnimationPreview();
    });
    params_->SetAnimationToggledCallback([this](bool on) {
        UpdateSaveButtonLabels(on);
        StopAnimationPreview();
    });
    params_->SetPlayPreviewToggledCallback([this] { OnPlayPreviewToggled(); });
    params_->SetOptionsChangedCallback([this] { ScheduleAutoPreview(kAutoPreviewPauseMs); });
    params_->SetAnimationCompositionChangedCallback([this] { StopAnimationPreview(); });
    // A new speed takes effect at once, also in a preview that is already playing.
    params_->SetAnimationSpeedChangedCallback([this] {
        if (preview_playing_) preview_timer_.Start(1000 / params_->GetAnimationFps());
    });
    preview_timer_.Bind(wxEVT_TIMER, &MainFrame::OnPreviewTimer, this);
    source_view_->SetCropEditedCallback([this](wxRect r) { OnCropOverlayEdited(r); });

    auto load_cb = [this](const wxString& path) { LoadImage(path); };
    SetDropTarget(new FileDropTarget(load_cb));
    source_view_->SetDropTarget(new FileDropTarget(load_cb));
    preview_view_->SetDropTarget(new FileDropTarget(load_cb));

    // Linked zoom and pan (see OnViewChanged). Last, so nothing above reports a view change into
    // a frame that is not complete yet.
    link_leader_ = source_view_;
    source_view_->SetViewChangedCallback([this](bool by_user) { OnViewChanged(source_view_, by_user); });
    preview_view_->SetViewChangedCallback([this](bool by_user) { OnViewChanged(preview_view_, by_user); });
    link_views_check_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        wxConfig::Get()->Write("View/LinkZoomPan", link_views_check_->GetValue());
        wxConfig::Get()->Flush();
        if (link_views_check_->GetValue()) SyncLinkedView(link_leader_);
    });
}

aic::ConversionOptions MainFrame::MakeOptions() const {
    aic::ConversionOptions opts = params_->GetOptions();
    opts.alpha_background = alpha_background_;
    opts.source_transform = source_transform_;
    return opts;
}

wxColour MainFrame::AlphaBackgroundColour() const {
    auto to8 = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.f, 1.f) * 255.f + 0.5f); };
    return wxColour(to8(alpha_background_.r), to8(alpha_background_.g), to8(alpha_background_.b));
}

void MainFrame::ApplySourceTransform(const aic::ImageTransform& delta, const wxString& status_text) {
    if (!source_image_) return;
    if (!PrepareForSourceChange()) return;
    const int old_w = source_image_->width(), old_h = source_image_->height();
    source_image_ = delta.apply(*source_image_);
    source_transform_ = source_transform_.then(delta);

    // Crop box and keyframes turn with the picture, so what the user framed
    // stays framed.
    params_->ApplySourceTransform(delta, old_w, old_h);

    // The preview shows the old orientation; clear it like a new image would.
    last_preview_.reset();
    preview_mapping_.reset();
    preview_view_->SetImage(wxImage());
    source_view_->SetImage(ToWxImage(*source_image_));
    UpdateInputLabel();
    SyncCropOverlayFromParams();
    SetStatus(wxString::Format("%s - %dx%d", status_text, source_image_->width(), source_image_->height()));
    Layout();
    ScheduleAutoPreview(kAutoPreviewAfterLoadMs);
}

void MainFrame::LoadImage(const wxString& path) {
    if (!PrepareForSourceChange()) return;
    std::string err;
    aic::LoadOptions load_opts;
    load_opts.alpha_background = alpha_background_;
    auto loaded = aic::Image::load(path.ToStdString(), &err, load_opts);
    if (!loaded) {
        wxMessageBox(wxString::Format(_("Failed to load: %s"), err), _("Error"), wxOK | wxICON_ERROR, this);
        ForgetFile(path);  // no use offering a file again that cannot be read
        return;
    }

    input_path_ = path;
    RememberFile(path);
    source_image_ = std::move(*loaded);
    source_transform_ = aic::ImageTransform{};  // a newly opened picture starts upright
    for (wxButton* b : transform_buttons_) b->Enable(true);
    last_preview_.reset();
    preview_mapping_.reset();
    UpdateInputLabel();
    params_->SetSourceSize(source_image_->width(), source_image_->height());
    source_view_->SetImage(ToWxImage(*source_image_));
    preview_view_->SetImage(wxImage());
    SyncCropOverlayFromParams();
    SetStatus(wxString::Format(_("Source: %dx%d"), source_image_->width(), source_image_->height()));
    Layout();
    ScheduleAutoPreview(kAutoPreviewAfterLoadMs);
}

void MainFrame::OnOpen(wxCommandEvent&) {
    if (RefuseWhileExporting()) return;  // before the dialog, not after the user picked a file
    // Start where the last picture came from: the open one, else the most recent one of the list.
    wxString start_dir;
    if (!input_path_.empty()) start_dir = wxFileName(input_path_).GetPath();
    else if (recent_files_->GetCount() > 0) start_dir = wxFileName(recent_files_->GetHistoryFile(0)).GetPath();
    if (!start_dir.empty() && !wxFileName::DirExists(start_dir)) start_dir.clear();
    wxFileDialog dlg(this, _("Open Image"), start_dir, "", gui::ImageFileWildcard(), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    LoadImage(dlg.GetPath());
}

void MainFrame::UpdateInputLabel() {
    if (!source_image_ || input_path_.empty()) return;
    input_label_->SetLabel(wxString::Format(_("%s  (%dx%d)"), wxFileName(input_path_).GetFullName(),
                                             source_image_->width(), source_image_->height()));
    input_label_->SetToolTip(input_path_);
}

void MainFrame::LoadRecentFiles() {
    wxConfigBase* cfg = wxConfig::Get();
    const wxString previous = cfg->GetPath();
    cfg->SetPath("/RecentFiles");
    recent_files_->Load(*cfg);
    cfg->SetPath(previous);
}

void MainFrame::SaveRecentFiles() {
    wxConfigBase* cfg = wxConfig::Get();
    const wxString previous = cfg->GetPath();
    cfg->SetPath("/RecentFiles");
    recent_files_->Save(*cfg);
    cfg->SetPath(previous);
    cfg->Flush();
}

void MainFrame::RememberFile(const wxString& path) {
    wxFileName name(path);
    name.MakeAbsolute();  // "renderHAMgui photo.jpg" must still find the file from another folder
    recent_files_->AddFileToHistory(name.GetFullPath());
    SaveRecentFiles();
}

void MainFrame::ForgetFile(const wxString& path) {
    wxFileName name(path);
    name.MakeAbsolute();
    for (size_t i = recent_files_->GetCount(); i-- > 0;) {
        if (recent_files_->GetHistoryFile(i) == name.GetFullPath()) recent_files_->RemoveFileFromHistory(i);
    }
    SaveRecentFiles();
}

void MainFrame::ShowRecentMenu() {
    if (RefuseWhileExporting()) return;
    // Entries whose file is gone are dropped now rather than offered and then refused.
    bool pruned = false;
    for (size_t i = recent_files_->GetCount(); i-- > 0;) {
        if (!wxFileName::FileExists(recent_files_->GetHistoryFile(i))) {
            recent_files_->RemoveFileFromHistory(i);
            pruned = true;
        }
    }
    if (pruned) SaveRecentFiles();

    wxMenu menu;
    if (recent_files_->GetCount() == 0) {
        menu.Append(wxID_ANY, _("(no recent images)"))->Enable(false);
    } else {
        for (size_t i = 0; i < recent_files_->GetCount(); ++i)
            menu.Append(ID_RECENT_FIRST + static_cast<int>(i), RecentMenuLabel(i, recent_files_->GetHistoryFile(i)));
        menu.AppendSeparator();
        menu.Append(ID_CLEAR_RECENT, _("Clear list"));
    }
    PopupMenu(&menu, recent_btn_->GetPosition() + wxPoint(0, recent_btn_->GetSize().GetHeight()));
}

void MainFrame::OnRecentFile(wxCommandEvent& event) {
    const size_t index = static_cast<size_t>(event.GetId() - ID_RECENT_FIRST);
    if (index >= recent_files_->GetCount()) return;
    const wxString path = recent_files_->GetHistoryFile(index);
    if (!wxFileName::FileExists(path)) {
        wxMessageBox(wxString::Format(_("The file no longer exists: %s"), path), _("Error"), wxOK | wxICON_ERROR, this);
        ForgetFile(path);
        return;
    }
    LoadImage(path);
}

void MainFrame::UpdateSaveButtonLabels(bool animation_enabled) {
    save_quick_btn_->SetLabel(animation_enabled ? _("Export Animation") : _("Save"));
    save_as_btn_->SetLabel(animation_enabled ? _("Export Animation As...") : _("Save As..."));
    // The new label may be wider than the old one - re-layout so the
    // button actually grows to fit it instead of clipping the text.
    save_quick_btn_->GetParent()->Layout();
}

std::vector<wxRect> MainFrame::BuildInterpolatedFrameRects() const {
    const auto& keyframes = params_->GetCropKeyframes();
    if (keyframes.size() < 2) return {};
    int frames_per_segment = params_->GetFramesPerSegment();

    // Linearly interpolate x/y/w/h across each consecutive keyframe
    // pair - "frames_per_segment" frames per segment (t=0 at the
    // segment's start keyframe, up to but not including t=1), plus one
    // final frame that lands exactly on the last keyframe (not
    // approximated by interpolation, since the very last segment's
    // loop deliberately stops short of t=1).
    std::vector<wxRect> frame_rects;
    for (size_t seg = 0; seg + 1 < keyframes.size(); ++seg) {
        const wxRect& a = keyframes[seg];
        const wxRect& b = keyframes[seg + 1];
        for (int f = 0; f < frames_per_segment; ++f) {
            double t = static_cast<double>(f) / frames_per_segment;
            wxRect r;
            r.x = static_cast<int>(std::lround(a.x + (b.x - a.x) * t));
            r.y = static_cast<int>(std::lround(a.y + (b.y - a.y) * t));
            r.width = static_cast<int>(std::lround(a.width + (b.width - a.width) * t));
            r.height = static_cast<int>(std::lround(a.height + (b.height - a.height) * t));
            frame_rects.push_back(r);
        }
    }
    frame_rects.push_back(keyframes.back());
    return frame_rects;
}

namespace {

// The crop box of each animation frame, as the library wants it (pixels).
std::vector<aic::CropSpec> ToCropSpecs(const std::vector<wxRect>& rects) {
    std::vector<aic::CropSpec> crops;
    crops.reserve(rects.size());
    for (const wxRect& r : rects) {
        aic::CropSpec spec;
        spec.width = {static_cast<float>(r.width), false};
        spec.height = {static_cast<float>(r.height), false};
        spec.x = {static_cast<float>(r.x), false};
        spec.y = {static_cast<float>(r.y), false};
        crops.push_back(spec);
    }
    return crops;
}

// What an animation export's worker thread leaves behind for the GUI thread.
// Everything in it is written on the worker and read after the job is done,
// except display_pending which both sides use (it is atomic).
struct ExportState {
    std::vector<aic::HAM6Frame> anim_frames;  // FORM ANIM: every frame is needed at once, see below
    std::vector<std::string> frame_paths;     // numbered files: where each frame goes
    bool anim_lace = false;
    aic::SequenceResult sequence;
    int delivered = 0;                        // frames handed out so far (on_frame calls are serialised)
    std::atomic<bool> display_pending{false};  // a live-preview update is queued for the GUI thread
    // A failure of our own writing (as opposed to a frame that failed to render):
    bool write_failed = false;
    size_t write_failed_frame = 0;
    std::string write_error;
};

// Same for the frames of the animation preview.
struct PreviewBuildState {
    std::vector<aic::PreviewResult> frames;
    aic::SequenceResult sequence;
    int delivered = 0;
};

}  // namespace

// Writes the interpolated frame sequence to disk. The frames are rendered by
// aic::render_sequence on several threads; this function only decides what
// happens to each finished frame:
//   numbered files: each frame is written the moment it is done;
//   FORM ANIM:      frames are collected and written in ONE call at the end, since
//                   the delta encoding for frame N reads frame N-2's bytes (see
//                   anim_writer.cpp).
// All frames after the first share the first frame's actual resulting palette
// (whether that came from a pre-existing Lock Palette or a fresh quantization -
// either way, reusing frame 0's result for every later frame is what keeps the
// whole sequence on one consistent palette). The Amiga Preview shows the most
// recently finished frame as live progress.
void MainFrame::RunAnimationExport(const wxString& base_path) {
    StopAnimationPreview();
    if (params_->GetCropKeyframes().size() < 2) {
        wxMessageBox(_("Animation needs at least 2 keyframes - use \"Add keyframe\" in the Rendering box."),
                     _("Notice"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (!BeginJob(JobKind::AnimationExport, _("Generating animation..."))) return;

    const std::vector<wxRect> frame_rects = BuildInterpolatedFrameRects();
    const size_t n = frame_rects.size();
    const wxFileName fn(base_path);
    const wxString dir = fn.GetPath();
    const wxString stem = fn.GetName();
    const bool anim_file_output = params_->IsAnimFileOutput();

    auto st = std::make_shared<ExportState>();
    if (anim_file_output) {
        st->anim_frames.resize(n);
    } else {
        for (size_t i = 0; i < n; ++i) {
            wxFileName out_file(dir, wxString::Format("%s_%04d.iff", stem, static_cast<int>(i) + 1));
            st->frame_paths.push_back(out_file.GetFullPath().ToStdString());
        }
    }
    const wxString anim_path = wxFileName(dir, stem + ".anim").GetFullPath();

    // Copies of everything the worker needs: it must not look at MainFrame's state, which
    // the user may change while it runs.
    const std::string input = input_path_.ToStdString();
    const aic::ConversionOptions opts = MakeOptions();
    const std::vector<aic::CropSpec> crops = ToCropSpecs(frame_rects);
    const int reltime_jiffies = aic::anim_jiffies_for_fps(params_->GetAnimationFps());

    StartJob(
        [this, st, input, opts, crops, anim_file_output, anim_path, n, reltime_jiffies](BackgroundJob::Context& ctx) {
            aic::SequenceOptions so;
            so.cancel = &ctx.cancel;
            so.on_progress = [this](float f) { job_progress_.store(f, std::memory_order_relaxed); };
            so.on_frame = [&, this](size_t i, aic::PreviewResult& pv) -> std::string {
                ++st->delivered;
                // Live progress: show a finished frame in the Amiga Preview, through the
                // same rendering path a normal conversion uses (interlace squash included).
                // Frames can finish faster than the GUI can draw them, so only one update is
                // queued at a time; the others are simply skipped.
                if (!st->display_pending.exchange(true)) {
                    auto shown = std::make_shared<aic::PreviewResult>(pv);
                    const int count = st->delivered;
                    ctx.post([this, st, shown, count, n] {
                        st->display_pending = false;
                        last_preview_ = std::move(*shown);
                        preview_mapping_.reset();  // each frame has its own crop box: no fixed mapping
                        UpdatePreviewDisplay();
                        SetStatus(wxString::Format(_("Generating animation: frame %d/%d..."), count,
                                                    static_cast<int>(n)));
                    });
                }

                if (anim_file_output) {
                    st->anim_frames[i] = std::move(pv.frame);
                    st->anim_lace = pv.lace;
                    return {};
                }
                aic::IlbmWriteOptions write_opts;
                write_opts.compress = opts.compress;
                write_opts.lace = pv.lace;
                std::string write_err;
                if (!aic::write_ham6_ilbm(pv.frame, st->frame_paths[i], write_opts, &write_err)) {
                    st->write_failed = true;
                    st->write_failed_frame = i;
                    st->write_error = write_err;
                    return write_err.empty() ? std::string("write failed") : write_err;
                }
                return {};
            };

            st->sequence = aic::render_sequence(pipeline_, input, opts, crops, so);

            if (st->sequence.success && anim_file_output) {
                ctx.post([this] { SetStatus(_("Writing animation file...")); });
                aic::AnimWriteOptions anim_opts;
                anim_opts.compress = opts.compress;
                anim_opts.lace = st->anim_lace;
                anim_opts.reltime_jiffies = reltime_jiffies;
                std::string write_err;
                if (!aic::write_ham6_anim(st->anim_frames, anim_path.ToStdString(), anim_opts, &write_err)) {
                    st->write_failed = true;
                    st->write_failed_frame = n;  // not a frame
                    st->write_error = write_err;
                }
            }
        },
        [this, st, n, dir, stem, anim_file_output] {
            const aic::SequenceResult& sr = st->sequence;
            if (st->write_failed) {
                if (st->write_failed_frame >= n) {
                    wxMessageBox(wxString::Format(_("Failed to write animation file: %s"), st->write_error),
                                 _("Error"), wxOK | wxICON_ERROR, this);
                } else {
                    wxMessageBox(wxString::Format(_("Failed to write frame %d: %s"),
                                                  static_cast<int>(st->write_failed_frame) + 1, st->write_error),
                                 _("Error"), wxOK | wxICON_ERROR, this);
                }
                SetStatus(_("Export failed."));
                return;
            }
            if (sr.cancelled) {
                SetStatus(_("Export cancelled."));
                return;
            }
            if (!sr.success) {
                wxMessageBox(wxString::Format(_("Animation frame %d failed: %s"),
                                              static_cast<int>(sr.failed_index) + 1, sr.error_message),
                             _("Error"), wxOK | wxICON_ERROR, this);
                SetStatus(_("Export failed."));
                return;
            }
            if (sr.palette) params_->NotifyPaletteGenerated(*sr.palette);
            if (anim_file_output) {
                SetStatus(wxString::Format(_("Animation exported: %d frames to %s.anim"), static_cast<int>(n), stem));
            } else {
                SetStatus(wxString::Format(_("Animation exported: %d frames to %s"), static_cast<int>(n), dir));
            }
        });
}

void MainFrame::OnPlayPreviewToggled() {
    // The frames of the preview are still being rendered: this click (the button reads
    // "Stop preview" meanwhile) or F5 stops that.
    if (job_.IsRunning() && job_kind_ == JobKind::AnimationPreview) {
        StopAnimationPreview();
        SetStatus(_("Cancelled."));
        return;
    }
    if (!params_->IsAnimationEnabled()) {
        // Acts as the former standalone Convert button in this mode -
        // see ParameterPanel::UpdateKeyframeControlsEnabled for the
        // matching "Show HAM preview" label/always-enabled state.
        StopAnimationPreview();  // defensive; shouldn't be active while animation is off anyway
        RunConversion();
        return;
    }
    if (preview_playing_) {
        StopAnimationPreview();
    } else {
        StartAnimationPreview();
    }
}

void MainFrame::StartAnimationPreview() {
    if (params_->GetCropKeyframes().size() < 2) {
        wxMessageBox(_("Animation needs at least 2 keyframes - use \"Add keyframe\" in the Rendering box."),
                     _("Notice"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (!BeginJob(JobKind::AnimationPreview, _("Building preview..."))) return;
    // The button reads "Stop preview" while the frames are rendered, too: clicking it
    // then cancels the rendering (see OnPlayPreviewToggled).
    params_->SetPreviewPlaying(true);
    preview_frames_.clear();

    // Same "frame 0's actual resulting palette, then locked for every later frame" as
    // RunAnimationExport (both go through aic::render_sequence), so the preview is
    // representative of what an actual export would look like - not just a rough
    // approximation of it.
    const std::vector<wxRect> frame_rects = BuildInterpolatedFrameRects();
    const size_t n = frame_rects.size();
    auto st = std::make_shared<PreviewBuildState>();
    st->frames.resize(n);
    const std::string input = input_path_.ToStdString();
    const aic::ConversionOptions opts = MakeOptions();
    const std::vector<aic::CropSpec> crops = ToCropSpecs(frame_rects);

    StartJob(
        [this, st, input, opts, crops, n](BackgroundJob::Context& ctx) {
            aic::SequenceOptions so;
            so.cancel = &ctx.cancel;
            so.on_progress = [this](float f) { job_progress_.store(f, std::memory_order_relaxed); };
            so.on_frame = [&, this](size_t i, aic::PreviewResult& pv) -> std::string {
                st->frames[i] = std::move(pv);
                const int count = ++st->delivered;
                ctx.post([this, count, n] {
                    SetStatus(wxString::Format(_("Building preview: frame %d/%d..."), count, static_cast<int>(n)));
                });
                return {};
            };
            st->sequence = aic::render_sequence(pipeline_, input, opts, crops, so);
        },
        [this, st] {
            const aic::SequenceResult& sr = st->sequence;
            if (sr.cancelled) {
                SetStatus(_("Cancelled."));
                return;
            }
            if (!sr.success) {
                wxMessageBox(wxString::Format(_("Animation frame %d failed: %s"),
                                              static_cast<int>(sr.failed_index) + 1, sr.error_message),
                             _("Error"), wxOK | wxICON_ERROR, this);
                SetStatus(_("Preview failed."));
                return;
            }
            if (sr.palette) params_->NotifyPaletteGenerated(*sr.palette);
            preview_frames_ = std::move(st->frames);

            preview_frame_index_ = 0;
            preview_direction_ = 1;
            preview_playing_ = true;
            params_->SetPreviewPlaying(true);
            last_preview_ = preview_frames_[0];
            preview_mapping_.reset();
            UpdatePreviewDisplay();
            // The tick follows the Playback speed setting - the same one an exported
            // Anim-File gets - so the preview is as fast as the result will be. There is
            // no per-tick pipeline work to keep up with: every frame was already fully
            // decoded up front.
            preview_timer_.Start(1000 / params_->GetAnimationFps());
            SetStatus(_("Playing animation preview..."));
        });
}

void MainFrame::StopAnimationPreview() {
    // A preview that is still being rendered is stopped too: whatever made the caller
    // stop (a changed keyframe, another picture) means its result would not match anymore.
    if (job_.IsRunning() && job_kind_ == JobKind::AnimationPreview) {
        job_.CancelAndWait();
        EndJobUi();
    }
    if (!preview_playing_) return;
    preview_timer_.Stop();
    preview_playing_ = false;
    params_->SetPreviewPlaying(false);
}

void MainFrame::OnPreviewTimer(wxTimerEvent&) {
    if (preview_frames_.empty()) {
        StopAnimationPreview();
        return;
    }
    bool loop = params_->IsPreviewLoopEnabled();
    bool pingpong = params_->IsPreviewPingPongEnabled();
    size_t last = preview_frames_.size() - 1;

    if (pingpong) {
        // Bounce between the two ends: reverses direction exactly at
        // each end rather than stepping past it, so a turnaround frame
        // (index 0 or last) is never rendered twice back-to-back - the
        // direction flips first, then the index moves away from it on
        // the very same tick.
        if ((preview_direction_ > 0 && preview_frame_index_ == last) ||
            (preview_direction_ < 0 && preview_frame_index_ == 0)) {
            if (!loop && preview_frame_index_ == 0 && preview_direction_ < 0) {
                StopAnimationPreview();
                return;
            }
            preview_direction_ = -preview_direction_;
        }
        preview_frame_index_ = static_cast<size_t>(static_cast<int>(preview_frame_index_) + preview_direction_);
    } else {
        if (preview_frame_index_ == last) {
            if (!loop) {
                StopAnimationPreview();
                return;
            }
            preview_frame_index_ = 0;
        } else {
            ++preview_frame_index_;
        }
    }

    last_preview_ = preview_frames_[preview_frame_index_];
    preview_mapping_.reset();
    UpdatePreviewDisplay();
}

void MainFrame::OnSaveAs(wxCommandEvent&) {
    if (input_path_.IsEmpty()) {
        wxMessageBox(_("Please open an image first."), _("Notice"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxFileDialog dlg(this, _("Save IFF As"), target_dir_, "", _("Amiga IFF ILBM (*.iff)|*.iff"),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    if (params_->IsAnimationEnabled()) {
        RunAnimationExport(dlg.GetPath());
        return;
    }

    SaveSingle(dlg.GetPath());
}

void MainFrame::OnTargetDirectory(wxCommandEvent&) {
    wxDirDialog dlg(this, _("Select Target Directory"), target_dir_, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    target_dir_ = dlg.GetPath();
    wxConfig::Get()->Write("TargetDirectory", target_dir_);
    wxConfig::Get()->Flush();
    UpdateTargetDirTooltip();
    SetStatus(wxString::Format(_("Target directory set: %s"), target_dir_));
}

void MainFrame::OnSaveQuick(wxCommandEvent&) {
    if (input_path_.IsEmpty()) {
        wxMessageBox(_("Please open an image first."), _("Notice"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (target_dir_.IsEmpty()) {
        wxMessageBox(_("Please select a target directory first."), _("Notice"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    // Original filename, extension stripped, ".iff" appended - no
    // dialog, no overwrite prompt (that's the point of "Save" vs.
    // "Save As...": a quick, repeatable, silent overwrite of the same
    // target once the directory is set).
    wxFileName target_file(target_dir_, wxFileName(input_path_).GetName(), "iff");

    if (params_->IsAnimationEnabled()) {
        RunAnimationExport(target_file.GetFullPath());
        return;
    }

    SaveSingle(target_file.GetFullPath());
}

void MainFrame::UpdateTargetDirTooltip() {
    target_dir_btn_->SetToolTip(target_dir_.IsEmpty() ? _("No target directory selected yet.") : target_dir_);
}

void MainFrame::SaveWindowGeometry() {
    if (IsIconized()) return;  // the reported rectangle of a minimized window is meaningless
    wxConfigBase* cfg = wxConfig::Get();
    cfg->Write("Defaults/WindowMaximized", IsMaximized());
    if (!IsMaximized()) {  // keep the last normal size so un-maximizing after a restart still works
        wxRect geo = GetRect();
        cfg->Write("Defaults/WindowWidth", static_cast<long>(geo.GetWidth()));
        cfg->Write("Defaults/WindowHeight", static_cast<long>(geo.GetHeight()));
        cfg->Write("Defaults/WindowX", static_cast<long>(geo.GetX()));
        cfg->Write("Defaults/WindowY", static_cast<long>(geo.GetY()));
    }
    cfg->Flush();
}

void MainFrame::OnCloseWindow(wxCloseEvent& event) {
    // A running export would be left half-written: ask before throwing it away.
    if (job_.IsRunning() && IsExportJob(job_kind_) && event.CanVeto()) {
        if (wxMessageBox(_("An export is still running. Cancel it and quit?"), _("Quit"), wxYES_NO | wxICON_QUESTION,
                         this) != wxYES) {
            event.Veto();
            return;
        }
    }
    // Stop the worker (it polls the cancel flag between scanlines) and drop what it
    // had queued, before the window it would report to goes away.
    job_.CancelAndWait();
    job_timer_.Stop();
    preview_timer_.Stop();
    SaveWindowGeometry();
    event.Skip();  // continue with the normal close
}

void MainFrame::OnSize(wxSizeEvent& event) {
    UpdateParamsPanelHeight();  // before Skip(): the default handler then lays out with the new height
    event.Skip();
}

void MainFrame::UpdateParamsPanelHeight() {
    if (!params_ || !toolbar_row_ || !status_row_) return;
    constexpr int kMinPreviewHeight = 260;  // two preview panes that are still worth looking at
    constexpr int kMinParamsHeight = 120;
    constexpr int kPreviewRowMargins = 8;   // wxALL 4 around the preview row

    const int natural = params_->NaturalHeight();
    const int reserved = toolbar_row_->CalcMin().GetHeight() + status_row_->CalcMin().GetHeight() +
                         kPreviewRowMargins + kMinPreviewHeight;
    const int wanted = std::clamp(GetClientSize().GetHeight() - reserved, kMinParamsHeight, std::max(natural, kMinParamsHeight));
    const int target = std::min(wanted, natural);
    if (params_->GetMinSize().GetHeight() != target) params_->SetMinSize(wxSize(wxDefaultCoord, target));
}

void MainFrame::OnSaveAsDefaults(wxCommandEvent&) {
    params_->SaveAsDefaults();
    SaveWindowGeometry();
    SetStatus(_("Current settings saved as defaults."));
}

void MainFrame::OnSettings(wxCommandEvent&) {
    wxString current_lang = wxConfig::Get()->Read("Language", "en");
    SettingsDialog dlg(this, current_lang, AlphaBackgroundColour());
    if (dlg.ShowModal() != wxID_OK) return;

    // Background for transparent images: takes effect immediately by reloading
    // the current image (the composite is made at load time).
    wxColour new_bg = dlg.GetTransparentBackground();
    if (new_bg.IsOk() && new_bg != AlphaBackgroundColour()) {
        alpha_background_ = aic::RGBf{new_bg.Red() / 255.f, new_bg.Green() / 255.f, new_bg.Blue() / 255.f};
        wxConfig::Get()->Write("TransparentBackground", new_bg.GetAsString(wxC2S_HTML_SYNTAX));
        wxConfig::Get()->Flush();
        if (!input_path_.IsEmpty()) LoadImage(input_path_);
    }

    wxString new_lang = dlg.GetSelectedLanguageCode();
    if (new_lang == current_lang) return;

    wxConfig::Get()->Write("Language", new_lang);
    wxConfig::Get()->Flush();

    // wxWidgets doesn't retranslate already-created widgets when the
    // active wxLocale changes, so a live switch would mean manually
    // updating every label in the app. Restarting is simpler and far
    // less error-prone - and since this app already accepts --lang and
    // an image path on the command line, the restart can carry the
    // currently open image straight through, so it doesn't feel like
    // starting over.
    wxString exe = wxStandardPaths::Get().GetExecutablePath();
    wxString cmd = "\"" + exe + "\" --lang " + new_lang;
    if (!input_path_.IsEmpty()) cmd += " \"" + input_path_ + "\"";

    long pid = wxExecute(cmd, wxEXEC_ASYNC);
    if (pid == 0) {
        wxMessageBox(_("Could not restart automatically. Please restart the application manually "
                        "for the new language to take effect."),
                     _("Notice"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent&) {
    AboutDialog dlg(this);
    dlg.ShowModal();
}

void MainFrame::SyncCropOverlayFromParams() {
    if (syncing_crop_) return;
    syncing_crop_ = true;
    source_view_->SetCropOverlay(params_->GetCropPixelRect());
    source_view_->SetCropAspectRatioLock(params_->IsCropRatioLocked(), params_->GetCropAspectRatio());
    syncing_crop_ = false;
}

void MainFrame::OnCropOverlayEdited(wxRect rect_px) {
    if (syncing_crop_) return;
    syncing_crop_ = true;
    params_->SetCropPixelRect(rect_px);
    // Without this, a rubber-band drag only updated the displayed
    // spinner values - it never wrote back into a currently selected
    // keyframe (see ParameterPanel::NotifyCropChanged), unlike every
    // other way of editing the crop (spinners, ratio-lock snapping,
    // keyframe selection itself). Re-selecting that keyframe later
    // would then silently revert to its last SAVED value, discarding
    // whatever the rubber-band had shown. Safe from the recursion this
    // guard exists for: NotifyCropChanged's own crop_changed_cb_ calls
    // back into SyncCropOverlayFromParams, which the syncing_crop_
    // flag (already true here) makes an immediate no-op.
    params_->NotifyCropChanged();
    syncing_crop_ = false;
}

// Single-image conversion for the preview. The pipeline call is the complete,
// self-contained unit of work: it gets copies of the options and the path, runs
// on the worker thread and reports its progress through ConversionOptions::progress.
// A conversion takes from a fraction of a second (greedy, 320x256) to several
// seconds (lookahead, interlaced sizes), which is why the window must not block
// on it.
void MainFrame::RunConversion() {
    if (input_path_.IsEmpty()) {
        wxMessageBox(_("Please open an image first."), _("Notice"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    auto_preview_timer_.Stop();  // whatever was pending is being done now
    if (!BeginJob(JobKind::Preview, _("Converting..."))) return;

    aic::ConversionOptions opts = MakeOptions();
    opts.keep_canvas = true;  // for the error-map view; animation frames deliberately do not keep theirs
    opts.quality_metrics = true;  // blurred error and SSIM for the status line (~50 ms at 320x256)
    const std::string input = input_path_.ToStdString();
    auto result = std::make_shared<aic::PreviewResult>();
    // Where the source lands on this result's canvas - from the options this run uses, which is
    // what the picture will show even if the controls change before it is done.
    const std::optional<aic::CanvasMapping> mapping =
        source_image_ ? aic::canvas_mapping(source_image_->width(), source_image_->height(), opts) : std::nullopt;

    StartJob(
        [this, opts, input, result](BackgroundJob::Context& ctx) mutable {
            opts.progress = [this, &ctx](float f) {
                job_progress_.store(f, std::memory_order_relaxed);
                return !ctx.cancel.load(std::memory_order_relaxed);
            };
            *result = pipeline_.preview(input, opts);
        },
        [this, result, mapping] {
            if (result->cancelled) {
                SetStatus(_("Cancelled."));
                return;
            }
            if (!result->success) {
                wxMessageBox(wxString::Format(_("Conversion failed: %s"), result->error_message), _("Error"),
                             wxOK | wxICON_ERROR, this);
                SetStatus(_("Conversion failed."));
                return;
            }
            // Taking the result in changes controls (the palette swatches); that is no setting of
            // the user's and must not start the next auto preview.
            handling_result_ = true;
            last_preview_ = std::move(*result);
            params_->NotifyPaletteGenerated(last_preview_->frame.palette);
            handling_result_ = false;
            const bool geometry_changed = preview_mapping_ != mapping;
            preview_mapping_ = mapping;
            UpdatePreviewDisplay();  // keeps zoom and position when the picture has the size of the last one
            // A new crop, size or scaling puts the picture elsewhere on the canvas: the preview
            // takes its place from the source again. Otherwise the two still fit.
            if (geometry_changed) SyncLinkedView(source_view_);
            const aic::QualityMetrics& qm = last_preview_->metrics;
            SetStatus(wxString::Format(_("%dx%d  error=%.4f  blurred=%.4f  SSIM=%.3f  LACE=%s"),
                                        last_preview_->frame.width, last_preview_->frame.height,
                                        last_preview_->mean_oklab_error, qm.blurred_error, qm.ssim,
                                        last_preview_->lace ? _("on") : _("off")));
        });
}

void MainFrame::SaveSingle(const wxString& output_path) {
    if (!BeginJob(JobKind::Save, _("Saving..."))) return;

    aic::ConversionOptions opts = MakeOptions();  // not const: the worker adds its progress callback to its copy
    const std::string input = input_path_.ToStdString();
    const std::string output = output_path.ToStdString();
    auto result = std::make_shared<aic::ConversionResult>();

    StartJob(
        [this, opts, input, output, result](BackgroundJob::Context& ctx) mutable {
            opts.progress = [this, &ctx](float f) {
                job_progress_.store(f, std::memory_order_relaxed);
                return !ctx.cancel.load(std::memory_order_relaxed);
            };
            *result = pipeline_.convert(input, output, opts);
        },
        [this, result, output_path] {
            if (result->cancelled) {
                SetStatus(_("Saving cancelled."));
                return;
            }
            if (!result->success) {
                wxMessageBox(wxString::Format(_("Failed to save: %s"), result->error_message), _("Error"),
                             wxOK | wxICON_ERROR, this);
                SetStatus(_("Saving failed."));
                return;
            }
            SetStatus(wxString::Format(_("Saved: %s (error=%.4f, LACE=%s)"), wxFileName(output_path).GetFullName(),
                                        result->mean_oklab_error, result->lace ? _("on") : _("off")));
        });
}

// --- Background job plumbing ------------------------------------------------

bool MainFrame::RefuseWhileExporting() {
    if (job_.IsRunning() && IsExportJob(job_kind_)) {
        SetStatus(_("Export running - Esc cancels it."));
        return true;
    }
    return false;
}

bool MainFrame::PrepareForSourceChange() {
    if (RefuseWhileExporting()) return false;
    if (job_.IsRunning()) {  // a preview of the old picture: no longer wanted
        job_.CancelAndWait();
        EndJobUi();
    }
    StopAnimationPreview();
    return true;
}

bool MainFrame::BeginJob(JobKind kind, const wxString& status_text) {
    if (RefuseWhileExporting()) return false;
    if (job_.IsRunning()) {  // the newest preview request wins
        job_.CancelAndWait();
        EndJobUi();
    }
    job_kind_ = kind;
    job_progress_.store(0.f);
    job_gauge_->SetValue(0);
    job_gauge_->Show();
    job_cancel_btn_->Show();
    Layout();
    job_timer_.Start(50);
    SetStatus(status_text);
    return true;
}

void MainFrame::StartJob(BackgroundJob::Work work, std::function<void()> done) {
    job_.Start(std::move(work), [this, done = std::move(done)] {
        EndJobUi();
        done();
    });
}

void MainFrame::EndJobUi() {
    job_timer_.Stop();
    job_gauge_->Hide();
    job_cancel_btn_->Hide();
    Layout();
    // The "Stop preview" label of a preview that was being rendered (see StartAnimationPreview).
    if (job_kind_ == JobKind::AnimationPreview && !preview_playing_) params_->SetPreviewPlaying(false);
    job_kind_ = JobKind::None;
}

void MainFrame::CancelRunningJob() {
    if (!job_.IsRunning()) return;
    job_.RequestCancel();
    SetStatus(_("Cancelling..."));
}

void MainFrame::OnJobTimer(wxTimerEvent&) {
    job_gauge_->SetValue(std::clamp(static_cast<int>(job_progress_.load(std::memory_order_relaxed) * 1000.f), 0, 1000));
}

void MainFrame::UpdatePreviewDisplay() {
    if (!last_preview_) return;
    // Preview / HAM map / error map. An empty map (error map for a frame
    // that did not keep its canvas, i.e. animation playback) falls back to
    // the normal decoded image rather than showing nothing.
    aic::Image map;
    switch (preview_mode_->GetSelection()) {
        case 1: map = aic::make_ham_map(last_preview_->frame); break;
        case 2: map = aic::make_error_map(last_preview_->canvas, last_preview_->decoded); break;
        case 3: map = last_preview_->canvas; break;  // what the encoder was given (empty during animation playback)
        default: break;
    }
    wxImage img = ToWxImage(map.empty() ? last_preview_->decoded : map);
    // Only squash when the decoded frame actually IS interlaced - a
    // checked-but-inapplicable box (e.g. still showing a non-interlaced
    // result from before LACE was turned off) is a harmless no-op here
    // rather than something that needs its own enabled/disabled state.
    //
    // This used to pre-shrink the image by discarding every other row
    // (a direct memcpy, deliberately not wxImage::Scale() - see git
    // history for why that blurred). That was wrong in a different way:
    // it threw away half of the actual decoded pixel data, AND still
    // rendered square pixels once zoomed in, since ImageView zooms
    // isotropically - a real interlaced scanline is only half as TALL
    // as it is wide, not literally absent every other line. Squashing
    // the DISPLAY instead (ImageView::SetVerticalSquash) keeps every
    // decoded pixel inspectable at full data fidelity while genuinely
    // rendering it half as tall as wide at any zoom level.
    bool squash = interlace_scale_check_->GetValue() && last_preview_->lace;
    preview_view_->SetVerticalSquash(squash ? 0.5 : 1.0);
    // The same size as the picture shown before keeps its zoom and position: a new result, another
    // map of the same frame, or the original lands exactly where the eye was.
    preview_view_->SetImage(img, true);
}

void MainFrame::ScheduleAutoPreview(int delay_ms) {
    if (handling_result_) return;
    if (!params_->IsAutoPreviewEnabled() || params_->IsAnimationEnabled() || input_path_.IsEmpty()) {
        auto_preview_timer_.Stop();  // switched off (or no longer applicable) while a conversion was pending
        return;
    }
    auto_preview_timer_.StartOnce(delay_ms);  // restarts a running pause: it is the pause since the LAST change
}

void MainFrame::OnAutoPreviewTimer(wxTimerEvent&) {
    if (!params_->IsAutoPreviewEnabled() || params_->IsAnimationEnabled() || input_path_.IsEmpty()) return;
    if (job_.IsRunning() && IsExportJob(job_kind_)) {  // never in the way of a save: try again later, silently
        auto_preview_timer_.StartOnce(1000);
        return;
    }
    RunConversion();
}

void MainFrame::UpdateZoomLabels() {
    if (source_zoom_label_) source_zoom_label_->SetLabel(wxString::Format("%d%%", source_view_->GetZoomPercent()));
    if (preview_zoom_label_) preview_zoom_label_->SetLabel(wxString::Format("%d%%", preview_view_->GetZoomPercent()));
}

void MainFrame::OnViewChanged(ImageView* origin, bool by_user) {
    UpdateZoomLabels();
    if (!by_user || syncing_views_) return;
    link_leader_ = origin;
    if (link_views_check_->GetValue()) SyncLinkedView(origin);
}

void MainFrame::SyncLinkedView(ImageView* leader) {
    if (!link_views_check_->GetValue() || !preview_mapping_ || !leader) return;
    if (!source_view_->HasImage() || !preview_view_->HasImage()) return;
    const bool from_source = leader == source_view_;
    ImageView* follower = from_source ? preview_view_ : source_view_;

    const ImageView::View lead = leader->GetView();
    ImageView::View view;
    view.fit = lead.fit;  // fit follows fit: "show everything" means the whole picture in each view
    if (!lead.fit) {
        const aic::CanvasMapping& m = *preview_mapping_;
        const aic::CanvasPoint center = from_source ? m.to_canvas({lead.center_x, lead.center_y})
                                                    : m.to_source({lead.center_x, lead.center_y});
        view.center_x = center.x;
        view.center_y = center.y;
        view.zoom = from_source ? m.zoom_to_canvas(lead.zoom) : m.zoom_to_source(lead.zoom);
    }
    syncing_views_ = true;
    follower->SetView(view);
    syncing_views_ = false;
    UpdateZoomLabels();
}

void MainFrame::ToggleOriginalView() {
    const int original = static_cast<int>(preview_mode_->GetCount()) - 1;
    if (preview_mode_->GetSelection() == original) {
        preview_mode_->SetSelection(mode_before_original_);
    } else {
        mode_before_original_ = preview_mode_->GetSelection();
        preview_mode_->SetSelection(original);
    }
    UpdatePreviewDisplay();
}

void MainFrame::SetStatus(const wxString& text) { status_label_->SetLabel(text); }
