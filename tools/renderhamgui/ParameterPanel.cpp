// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "ParameterPanel.hpp"

#include "FileFilters.hpp"
#include "GuiIcons.hpp"
#include "IconUtil.hpp"

#include <wx/config.h>
#include <wx/filefn.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/statline.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace {

wxArrayString RegistryNames(const std::vector<std::string>& names) {
    wxArrayString out;
    for (const auto& n : names) out.Add(wxString(n));
    return out;
}

wxString FormatFloat(float v) { return wxString::Format("%.2f", v); }

// Space around each row inside a group box, and between the group boxes. The window's height is
// the height of the two columns of group boxes plus the two preview panes, and on a 1080p screen
// every row of the columns is paid for by the preview panes, so the gaps are small.
constexpr int kPad = 2;
constexpr int kBoxGap = 6;
constexpr int kSwatchSize = 22;  // a palette swatch: 16 of them have to fit one row of the column

// The name of a quality profile as shown in the choice. The library's names are English only; each
// is spelled out here so the translation tooling sees it.
wxString QualityProfileLabel(aic::QualityProfile id) {
    switch (id) {
        case aic::QualityProfile::Fast: return _("Fast");
        case aic::QualityProfile::Standard: return _("Standard");
        case aic::QualityProfile::Best: return _("Best quality");
    }
    return wxString();
}

}  // namespace

namespace {

// Playback speeds offered for animations: the frame rates that divide 60, because an ANIM
// counts frame time in jiffies of 1/60 s (see aic::anim_jiffies_for_fps). Index 1 (30 fps)
// is the default: the traditional ANIM rate, and what exports always used.
constexpr int kAnimFpsChoices[] = {60, 30, 20, 15, 12, 10, 6, 5, 3, 2, 1};
constexpr int kDefaultAnimFpsIndex = 1;

}  // namespace

class ParameterPanel::ChangeWatcher : public wxEventFilter {
public:
    explicit ChangeWatcher(ParameterPanel* panel) : panel_(panel) { wxEvtHandler::AddFilter(this); }
    ~ChangeWatcher() override { wxEvtHandler::RemoveFilter(this); }

    int FilterEvent(wxEvent& event) override {
        if (!event.IsCommandEvent()) return Event_Skip;
        // The events a control sends when its value was changed by the user. Buttons are not among
        // them: what a button does to the settings reports itself (see NotifyOptionsChanged).
        const wxEventType type = event.GetEventType();
        if (type != wxEVT_CHECKBOX && type != wxEVT_RADIOBUTTON && type != wxEVT_RADIOBOX && type != wxEVT_CHOICE &&
            type != wxEVT_COMBOBOX && type != wxEVT_SLIDER && type != wxEVT_SPINCTRL &&
            type != wxEVT_SPINCTRLDOUBLE && type != wxEVT_COLOURPICKER_CHANGED)
            return Event_Skip;
        const auto* source = wxDynamicCast(event.GetEventObject(), wxWindow);
        if (source && panel_->IsDescendant(const_cast<wxWindow*>(source)) && panel_->AffectsSinglePreview(source))
            panel_->NotifyOptionsChanged();
        return Event_Skip;  // never in the way of the normal handling
    }

private:
    ParameterPanel* panel_;
};

ParameterPanel::~ParameterPanel() = default;

void ParameterPanel::NotifyOptionsChanged() {
    if (options_changed_cb_) options_changed_cb_();
}

bool ParameterPanel::AffectsSinglePreview(const wxWindow* control) const {
    // Animation and export settings change nothing about a single preview; neither does the unit
    // of the crop numbers (the crop itself reports through NotifyCropChanged).
    const wxWindow* const unrelated[] = {animation_enable_, keyframe_choice_, frames_per_segment_, anim_speed_,
                                         output_mode_,      crop_unit_px_,    crop_unit_pct_,      compress_};
    return std::find(std::begin(unrelated), std::end(unrelated), control) == std::end(unrelated);
}

ParameterPanel::ParameterPanel(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxHSCROLL | wxTAB_TRAVERSAL) {
    presets_ = {
        {"320 x 256 (PAL)", 320, 256},
        {_("352 x 280 (PAL, overscan)"), 352, 280},
        {"320 x 200 (NTSC)", 320, 200},
        {_("352 x 240 (NTSC, overscan)"), 352, 240},
        {_("320 x 512 (PAL, interlaced)"), 320, 512},
        {_("352 x 560 (PAL, interlaced, overscan)"), 352, 560},
        {_("320 x 400 (NTSC, interlaced)"), 320, 400},
        {_("352 x 480 (NTSC, interlaced, overscan)"), 352, 480},
        {_("Custom..."), 0, 0},
    };
    // Overscan dimensions are commonly-cited approximations, not a
    // single hardware-mandated exact value - same caveat as
    // pixel_aspect_ratio (see the README's "--pixel-aspect" section):
    // the actual visible overscan area depends on the specific
    // monitor/scandoubler and how far into the border a given display
    // pushes. 352 wide is a widely-used "safe overscan" figure for
    // both PAL and NTSC; 280/240 (and their doubled interlace
    // counterparts 560/480) are commonly-cited full-frame heights.
    // Interlace ratios were tried and removed again - see git history -
    // they turned out not to be a sensible crop concept after all
    // (mixing a display-physical correction into what's otherwise a
    // plain source-pixel selection tool was more confusing than useful).
    // NTSC/PAL match the pixel dimensions of the same-named resolution
    // preset directly (320:200, 320:256); 1:1/4:3/16:9 are the standard
    // photo/video framings.
    ratio_presets_ = {
        {_("NTSC (8:5)"), 320, 200},
        {_("PAL (5:4)"), 320, 256},
        {"1:1", 1, 1},
        {"4:3", 4, 3},
        {"16:9", 16, 9},
    };

    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* columns = new wxBoxSizer(wxHORIZONTAL);
    auto* left = new wxBoxSizer(wxVERTICAL);
    auto* right = new wxBoxSizer(wxVERTICAL);

    BuildCropBox(left);
    BuildAnimationBox(left);
    BuildResolutionBox(left);
    BuildColorCorrectionBox(right);
    BuildColorBox(right);
    BuildPaletteBox(right);
    BuildOutputBox(right);

    columns->Add(left, 1, wxEXPAND | wxRIGHT, 8);
    columns->Add(right, 1, wxEXPAND);
    outer->Add(columns, 1, wxEXPAND | wxALL, 8);
    // Scrolls when the frame gives it less than NaturalHeight() (small
    // screens); at full height there is no scrollbar and nothing changes.
    SetScrollRate(10, 10);
    SetSizer(outer);
    FitInside();

    UpdateCropControlsEnabled();
    UpdateCustomResolutionEnabled();
    change_watcher_ = std::make_unique<ChangeWatcher>(this);
}

int ParameterPanel::NaturalHeight() const {
    return GetSizer() ? GetSizer()->CalcMin().GetHeight() : 0;
}

int ParameterPanel::NaturalWidth() const {
    return GetSizer() ? GetSizer()->CalcMin().GetWidth() : 0;
}

void ParameterPanel::BuildCropBox(wxSizer* parent_sizer) {
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("Crop (before scaling)"));

    // The switch and the unit it works in share a row.
    auto* head_row = new wxBoxSizer(wxHORIZONTAL);
    crop_enable_ = new wxCheckBox(box->GetStaticBox(), wxID_ANY, _("Enable crop"));
    crop_unit_px_ = new wxRadioButton(box->GetStaticBox(), wxID_ANY, _("Pixels"), wxDefaultPosition, wxDefaultSize,
                                       wxRB_GROUP);
    crop_unit_pct_ = new wxRadioButton(box->GetStaticBox(), wxID_ANY, _("Percent"));
    head_row->Add(crop_enable_, 0, wxALIGN_CENTER_VERTICAL);
    head_row->AddStretchSpacer(1);
    head_row->Add(crop_unit_px_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    head_row->Add(crop_unit_pct_, 0, wxALIGN_CENTER_VERTICAL);
    box->Add(head_row, 0, wxEXPAND | wxALL, kPad);

    // Width and Height in one row, X and Y in the next, each label to the left of its spinner.
    auto* grid = new wxFlexGridSizer(4, 4, 6);
    grid->AddGrowableCol(1, 1);
    grid->AddGrowableCol(3, 1);
    auto add_spin = [&](const wxString& caption, wxSpinCtrlDouble*& spin, double value) {
        grid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, caption), 0, wxALIGN_CENTER_VERTICAL);
        spin = new wxSpinCtrlDouble(box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 0, 100, value, 1);
        grid->Add(spin, 1, wxEXPAND);
    };
    add_spin(_("Width"), crop_w_, 100);
    add_spin(_("Height"), crop_h_, 100);
    add_spin("X", crop_x_, 0);
    add_spin("Y", crop_y_, 0);
    box->Add(grid, 0, wxEXPAND | wxALL, kPad);

    auto* ratio_row = new wxBoxSizer(wxHORIZONTAL);
    crop_ratio_lock_ = new wxCheckBox(box->GetStaticBox(), wxID_ANY, _("Lock aspect ratio"));
    ratio_row->Add(crop_ratio_lock_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    wxArrayString ratio_labels;
    for (const auto& p : ratio_presets_) ratio_labels.Add(p.label);
    crop_ratio_choice_ = new wxChoice(box->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, ratio_labels);
    crop_ratio_choice_->SetSelection(0);
    ratio_row->Add(crop_ratio_choice_, 1);
    box->Add(ratio_row, 0, wxEXPAND | wxALL, kPad);

    parent_sizer->Add(box, 0, wxEXPAND | wxBOTTOM, kBoxGap);

    crop_enable_->Bind(wxEVT_CHECKBOX, &ParameterPanel::OnCropEnableToggled, this);
    crop_unit_px_->Bind(wxEVT_RADIOBUTTON, &ParameterPanel::OnCropUnitChanged, this);
    crop_unit_pct_->Bind(wxEVT_RADIOBUTTON, &ParameterPanel::OnCropUnitChanged, this);
    for (auto* spin : {crop_w_, crop_h_, crop_x_, crop_y_}) {
        spin->Bind(wxEVT_SPINCTRLDOUBLE, &ParameterPanel::OnCropSpinChanged, this);
    }
    crop_ratio_lock_->Bind(wxEVT_CHECKBOX, &ParameterPanel::OnCropRatioLockToggled, this);
    crop_ratio_choice_->Bind(wxEVT_CHOICE, &ParameterPanel::OnCropRatioPresetChanged, this);
}

void ParameterPanel::BuildAnimationBox(wxSizer* parent_sizer) {
    // Everything about producing the picture(s): the preview button, which renders a plain
    // single image as well as an animation, and the Ken Burns-style pan/zoom animation
    // (see the project's README/history for the fuller design discussion): each keyframe is
    // just a captured crop rectangle; MainFrame's RunAnimationExport interpolates between
    // consecutive ones and writes a numbered IFF sequence or an ANIM when this box's "Enable
    // animation" checkbox is on. Hence "Rendering", not "Animation": the box was called
    // "Animation" while it only held animation settings, and the heading was wrong for a
    // single image once "Show HAM preview" lived in it.
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("Rendering"));

    // The switch and the keyframe list share a row: the list is greyed out without the switch anyway.
    animation_enable_ = new wxCheckBox(box->GetStaticBox(), wxID_ANY, _("Enable animation"));
    animation_enable_->SetToolTip(_("Requires \"Enable crop\" above to also be checked"));

    auto* keyframe_row = new wxBoxSizer(wxHORIZONTAL);
    keyframe_row->Add(animation_enable_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    keyframe_choice_ = new wxChoice(box->GetStaticBox(), wxID_ANY);
    // Symbols (a keyframe diamond with plus / minus) with tooltips: the row keeps its room for
    // the keyframe list, and the diamond is what animation tools use for a keyframe anyway.
    const wxColour icon_color = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
    add_keyframe_btn_ = new wxBitmapButton(box->GetStaticBox(), wxID_ANY,
                                           MakeMaskIcon(kIconKeyframeAdd, kGuiIconSize, icon_color));
    SetMaskIcon(add_keyframe_btn_, kIconKeyframeAdd, kGuiIconSize);
    add_keyframe_btn_->SetToolTip(_("Add keyframe: capture the current crop as a new keyframe (Ctrl+K)"));
    remove_keyframe_btn_ = new wxBitmapButton(box->GetStaticBox(), wxID_ANY,
                                              MakeMaskIcon(kIconKeyframeRemove, kGuiIconSize, icon_color));
    SetMaskIcon(remove_keyframe_btn_, kIconKeyframeRemove, kGuiIconSize);
    remove_keyframe_btn_->SetToolTip(_("Remove the selected keyframe"));
    keyframe_row->Add(keyframe_choice_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    keyframe_row->Add(add_keyframe_btn_, 0, wxRIGHT, 4);
    keyframe_row->Add(remove_keyframe_btn_, 0);
    box->Add(keyframe_row, 0, wxEXPAND | wxALL, kPad);

    auto* frames_row = new wxBoxSizer(wxHORIZONTAL);
    frames_row->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Frames between keyframes")), 0,
                     wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    frames_per_segment_ = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                                          wxDefaultSize, wxSP_ARROW_KEYS, 1, 999, 10);
    frames_row->Add(frames_per_segment_, 0);
    box->Add(frames_row, 0, wxALL, kPad);

    auto* speed_row = new wxBoxSizer(wxHORIZONTAL);
    speed_row->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Playback speed")), 0,
                    wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    anim_speed_ = new wxChoice(box->GetStaticBox(), wxID_ANY);
    for (int fps : kAnimFpsChoices) anim_speed_->Append(wxString::Format(_("%d fps"), fps));
    anim_speed_->SetSelection(kDefaultAnimFpsIndex);
    anim_speed_->SetToolTip(
        _("How fast the animation plays: in the preview here, and in an exported Anim-File (stored as one "
          "frame every 60/fps jiffies of 1/60 s). Numbered single images have no speed. Only speeds that "
          "divide 60 are offered, since an ANIM counts in 1/60 s."));
    speed_row->Add(anim_speed_, 0, wxALIGN_CENTER_VERTICAL);
    box->Add(speed_row, 0, wxALL, kPad);

    // Single images = today's numbered-IFF-sequence behavior; Anim-File
    // = a single delta-compressed FORM ANIM (see anim_writer.hpp/cpp -
    // ANIM-5, the most widely supported real-Amiga-playable mode).
    const wxString output_choices[2] = {_("Single images"), _("Anim-File")};
    // Side by side (one column per choice), not stacked: one row instead of two.
    output_mode_ = new wxRadioBox(box->GetStaticBox(), wxID_ANY, _("Export format"), wxDefaultPosition,
                                   wxDefaultSize, 2, output_choices, 2, wxRA_SPECIFY_COLS);
    box->Add(output_mode_, 0, wxEXPAND | wxALL, kPad);

    // In-memory real-time preview: plays the same interpolated frame
    // sequence RunAnimationExport would write to disk, but only in the
    // Amiga Preview panel - see MainFrame::StartAnimationPreview/
    // OnPreviewTimer. Loop and Back and forth (ping-pong) combine
    // freely; both are read live on every tick, so toggling either
    // while already playing takes effect immediately. One plain row at the
    // bottom of "Rendering": the button and the toggles read as a group by themselves, and a frame
    // with a caption of its own cost another half row of height.
    auto* preview_box = new wxBoxSizer(wxHORIZONTAL);
    play_preview_btn_ = new wxButton(box->GetStaticBox(), wxID_ANY, _("Show HAM preview"));
    // Toggle buttons with symbols, not checkboxes with words: both are read live on every tick
    // and combine freely, and the symbols (repeat, arrow both ways) are the familiar ones.
    preview_loop_ = new wxBitmapToggleButton(box->GetStaticBox(), wxID_ANY,
                                             MakeMaskIcon(kIconLoop, kGuiIconSize, icon_color));
    SetMaskIcon(preview_loop_, kIconLoop, kGuiIconSize);
    preview_loop_->SetToolTip(_("Loop: start again from the beginning when the preview reaches the end"));
    preview_pingpong_ = new wxBitmapToggleButton(box->GetStaticBox(), wxID_ANY,
                                                 MakeMaskIcon(kIconPingPong, kGuiIconSize, icon_color));
    SetMaskIcon(preview_pingpong_, kIconPingPong, kGuiIconSize);
    preview_pingpong_->SetToolTip(_("Back and forth: play forwards, then backwards, and so on"));
    // Next to the button it belongs to: the single-image preview is what it repeats. A toggle
    // button with a symbol like the two next to it - a checkbox with words made the box wider
    // than its column in German, which squeezed the right-hand column out of the window.
    auto_preview_ = new wxBitmapToggleButton(box->GetStaticBox(), wxID_ANY,
                                             MakeMaskIcon(kIconAutoPreview, kGuiIconSize, icon_color));
    SetMaskIcon(auto_preview_, kIconAutoPreview, kGuiIconSize);
    auto_preview_->SetToolTip(
        _("Auto preview: convert again by itself whenever a setting changes, after a short pause without "
          "further changes (the newest request wins; a conversion that is still running is replaced). "
          "Not available for animations."));
    preview_box->Add(play_preview_btn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    preview_box->Add(auto_preview_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    preview_box->Add(preview_loop_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    preview_box->Add(preview_pingpong_, 0, wxALIGN_CENTER_VERTICAL);
    box->Add(preview_box, 0, wxEXPAND | wxALL, kPad);

    parent_sizer->Add(box, 0, wxEXPAND | wxBOTTOM, kBoxGap);

    animation_enable_->Bind(wxEVT_CHECKBOX, &ParameterPanel::OnAnimationEnableToggled, this);
    add_keyframe_btn_->Bind(wxEVT_BUTTON, &ParameterPanel::OnAddKeyframe, this);
    remove_keyframe_btn_->Bind(wxEVT_BUTTON, &ParameterPanel::OnRemoveKeyframe, this);
    keyframe_choice_->Bind(wxEVT_CHOICE, &ParameterPanel::OnKeyframeSelected, this);
    play_preview_btn_->Bind(wxEVT_BUTTON, &ParameterPanel::OnPlayPreviewClicked, this);
    // Switching it on starts a preview, switching it off cancels a pending one: the frame decides.
    auto_preview_->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { NotifyOptionsChanged(); });
    frames_per_segment_->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&) {
        if (animation_composition_changed_cb_) animation_composition_changed_cb_();
    });
    anim_speed_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (animation_speed_changed_cb_) animation_speed_changed_cb_();
    });

    UpdateKeyframeControlsEnabled();
}

void ParameterPanel::BuildResolutionBox(wxSizer* parent_sizer) {
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("Target resolution and mode"));

    wxArrayString preset_labels;
    for (const auto& p : presets_) preset_labels.Add(p.label);
    resolution_preset_ = new wxChoice(box->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, preset_labels);
    resolution_preset_->SetSelection(0);
    box->Add(resolution_preset_, 0, wxEXPAND | wxALL, kPad);

    auto* custom_row = new wxBoxSizer(wxHORIZONTAL);
    custom_width_ = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY, "320", wxDefaultPosition, wxDefaultSize,
                                    wxSP_ARROW_KEYS, 16, 4096, 320);
    custom_height_ = new wxSpinCtrl(box->GetStaticBox(), wxID_ANY, "256", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 16, 4096, 256);
    custom_row->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, _("W:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    custom_row->Add(custom_width_, 0, wxRIGHT, 10);
    custom_row->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, _("H:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    custom_row->Add(custom_height_, 0);
    box->Add(custom_row, 0, wxALL, kPad);

    // The three modes side by side (one column per choice), not stacked.
    const wxString fit_choices[3] = {_("Fit (letterbox)"), _("Fill (crop)"), _("Stretch")};
    fit_mode_ = new wxRadioBox(box->GetStaticBox(), wxID_ANY, _("Scaling mode"), wxDefaultPosition, wxDefaultSize, 3,
                                fit_choices, 3, wxRA_SPECIFY_COLS);
    box->Add(fit_mode_, 0, wxEXPAND | wxALL, kPad);

    // Caption, slider and value in one row.
    auto* pa_row = new wxBoxSizer(wxHORIZONTAL);
    pixel_aspect_caption_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Pixel aspect ratio"));
    pixel_aspect_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 100, 20, 200);
    pixel_aspect_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, "1.00");
    pa_row->Add(pixel_aspect_caption_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    pa_row->Add(pixel_aspect_, 1, wxALIGN_CENTER_VERTICAL);
    pa_row->Add(pixel_aspect_label_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
    box->Add(pa_row, 0, wxEXPAND | wxALL, kPad);

    parent_sizer->Add(box, 0, wxEXPAND);

    resolution_preset_->Bind(wxEVT_CHOICE, &ParameterPanel::OnResolutionPresetChanged, this);
    fit_mode_->Bind(wxEVT_RADIOBOX, &ParameterPanel::OnFitModeChanged, this);
    pixel_aspect_->Bind(wxEVT_SLIDER, &ParameterPanel::OnPixelAspectChanged, this);
    UpdatePixelAspectEnabled();
}

void ParameterPanel::BuildColorBox(wxSizer* parent_sizer) {
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("HAM conversion settings"));

    // Choices and sliders are laid out in side-by-side pairs (same growable-
    // column FlexGridSizer pattern as the Refinement / Chroma row below)
    // instead of one control per row: this box sets the height of the whole
    // right-hand column and with it the window, and stacking every new
    // control grew the window past what a 1080p screen can show.
    // The quality profile takes a single line, label and choice side by side, for the same reason.
    auto* profile_row = new wxBoxSizer(wxHORIZONTAL);
    auto* profile_label = new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Quality profile"));
    profile_choice_ = new wxChoice(box->GetStaticBox(), wxID_ANY);
    for (const aic::QualityProfileInfo& profile : aic::quality_profiles())
        profile_choice_->Append(QualityProfileLabel(profile.id));
    profile_choice_->Append(_("Custom"));
    const wxString profile_tip =
        _("Sets the palette quantizer, the refinement passes and the lookahead below in one go.\n"
          "Fast: one refinement pass. About the same quality as Standard, in roughly a quarter less time.\n"
          "Standard: the default.\n"
          "Best quality: 8 passes and a lookahead of 4 pixels. About 5 percent less error on typical "
          "photographs, but it takes roughly 7 times as long.\n"
          "Changing one of those three settings by hand switches this to Custom. Greyed out while the "
          "palette is locked, as the quantizer and the passes have no effect then.");
    profile_label->SetToolTip(profile_tip);
    profile_choice_->SetToolTip(profile_tip);
    profile_row->Add(profile_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    profile_row->Add(profile_choice_, 1, wxALIGN_CENTER_VERTICAL);
    box->Add(profile_row, 0, wxEXPAND | wxALL, kPad);

    // Dithering and its strength share a row, and so do the quantizer and the color focus: every
    // caption sits left of its control instead of above it. The strength has no caption of its
    // own - it is the slider right of the dithering choice, and its tooltip says what it scales.
    auto* dither_row = new wxBoxSizer(wxHORIZONTAL);
    dither_row->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Dithering")), 0,
                    wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    dither_choice_ = new wxChoice(
        box->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
        RegistryNames(aic::Registry<aic::IErrorDiffuser>::instance().names()));
    dither_choice_->SetStringSelection("floyd-steinberg");
    if (dither_choice_->GetSelection() == wxNOT_FOUND) dither_choice_->SetSelection(0);
    dither_row->Add(dither_choice_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    // Applies to every ditherer except "none" (greyed out for that one,
    // see UpdateDitherStrengthEnabled): error diffusers read it as the
    // share of error propagated, ordered ditherers as pattern amplitude.
    dither_strength_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 100, 0, 100);
    dither_strength_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, wxString::Format("%d%%", 100),
                                              wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    dither_strength_label_->SetMinSize(wxSize(dither_strength_label_->GetTextExtent("100%").GetWidth() + 4, -1));
    const wxString strength_tip =
        _("Dither strength: with the error-diffusion ditherers the share of each pixel's error that is passed on "
          "to its neighbors, with the ordered ones (Bayer, blue noise, check patterns) the strength of the pattern. "
          "Not available without dithering.");
    dither_strength_->SetToolTip(strength_tip);
    dither_strength_label_->SetToolTip(strength_tip);
    dither_row->Add(dither_strength_, 1, wxALIGN_CENTER_VERTICAL);
    dither_row->Add(dither_strength_label_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    box->Add(dither_row, 0, wxEXPAND | wxALL, kPad);

    auto* choice_row = new wxBoxSizer(wxHORIZONTAL);
    choice_row->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Palette quantizer")), 0,
                    wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    quantizer_choice_ = new wxChoice(
        box->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
        RegistryNames(aic::Registry<aic::IPaletteQuantizer>::instance().names()));
    quantizer_choice_->SetStringSelection("kmeans");
    if (quantizer_choice_->GetSelection() == wxNOT_FOUND) quantizer_choice_->SetSelection(0);
    choice_row->Add(quantizer_choice_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    choice_row->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Color focus")), 0,
                    wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    importance_choice_ = new wxChoice(
        box->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
        RegistryNames(aic::Registry<aic::IImportanceMap>::instance().names()));
    importance_choice_->SetStringSelection("uniform");
    if (importance_choice_->GetSelection() == wxNOT_FOUND) importance_choice_->SetSelection(0);
    choice_row->Add(importance_choice_, 1, wxALIGN_CENTER_VERTICAL);
    box->Add(choice_row, 0, wxEXPAND | wxALL, kPad);

    // Refinement and Chroma<->Luminance side by side rather than
    // stacked - same growable-columns FlexGridSizer pattern as the
    // color-correction 2x2 grid above.
    auto* grid = new wxFlexGridSizer(2, 2, 4, 8);
    grid->AddGrowableCol(0, 1);
    grid->AddGrowableCol(1, 1);

    palette_iterations_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY,
                                                  wxString::Format(_("Refinement: %d passes"), 4));
    chroma_weight_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Chroma <-> Luminance: 1.00"));
    grid->Add(palette_iterations_label_, 0, wxALIGN_LEFT);
    grid->Add(chroma_weight_label_, 0, wxALIGN_LEFT);

    palette_iterations_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 4, 0, 16);
    chroma_weight_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 100, 0, 200);
    grid->Add(palette_iterations_, 0, wxEXPAND);
    grid->Add(chroma_weight_, 0, wxEXPAND);

    box->Add(grid, 0, wxEXPAND | wxALL, kPad);

    line_start_modify_ = new wxCheckBox(box->GetStaticBox(), wxID_ANY, _("Allow Modify at line start"));
    line_start_modify_->SetToolTip(
        _("Lets a scanline start with a Modify of the border color (palette color 0), as the ILBM "
          "specification allows. Slightly better quality at the left edge, but decoders that start "
          "every line from black (e.g. netpbm's ilbmtoppm) show those pixels wrongly. No difference "
          "if color 0 is black."));

    // Shares the checkbox's row: a row of its own would make the column that
    // sets the window height taller again (see the layout note in the README).
    lookahead_choice_ = new wxChoice(box->GetStaticBox(), wxID_ANY);
    lookahead_choice_->Append(_("Lookahead: off"));
    for (int n = 2; n <= 4; ++n) lookahead_choice_->Append(wxString::Format(_("Lookahead: %d pixels"), n));
    lookahead_choice_->SetSelection(0);  // off, the library default
    lookahead_choice_->SetToolTip(
        _("Looks several pixels ahead when choosing each HAM operation, so that a color change can start "
          "before an edge instead of after it. Less smearing at edges (error about 3 to 6 percent lower), but the "
          "conversion takes roughly 3 to 10 times longer; 3 pixels is a good compromise. Off by default, "
          "as it makes every preview slower. Animations take proportionally longer, too."));
    auto* option_row = new wxBoxSizer(wxHORIZONTAL);
    option_row->Add(line_start_modify_, 0, wxALIGN_CENTER_VERTICAL);
    option_row->AddStretchSpacer(1);
    option_row->Add(lookahead_choice_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    box->Add(option_row, 0, wxEXPAND | wxALL, kPad);

    parent_sizer->Add(box, 0, wxEXPAND | wxBOTTOM, kBoxGap);

    palette_iterations_->Bind(wxEVT_SLIDER, &ParameterPanel::OnPaletteIterationsChanged, this);
    profile_choice_->Bind(wxEVT_CHOICE, &ParameterPanel::OnProfileChosen, this);
    quantizer_choice_->Bind(wxEVT_CHOICE, &ParameterPanel::OnQualitySettingChanged, this);
    lookahead_choice_->Bind(wxEVT_CHOICE, &ParameterPanel::OnQualitySettingChanged, this);
    SyncProfileChoice();
    chroma_weight_->Bind(wxEVT_SLIDER, &ParameterPanel::OnChromaWeightChanged, this);
    dither_choice_->Bind(wxEVT_CHOICE, &ParameterPanel::OnDitherChoiceChanged, this);
    dither_strength_->Bind(wxEVT_SLIDER, &ParameterPanel::OnDitherStrengthChanged, this);
    UpdateDitherStrengthEnabled();
}

void ParameterPanel::BuildColorCorrectionBox(wxSizer* parent_sizer) {
    // Deliberately its own box, separate from "HAM conversion settings"
    // right below it in the same column: that one holds algorithm
    // CHOICES (which ditherer, which quantizer); this one holds tonal
    // ADJUSTMENTS applied to the canvas before any of that runs - a
    // different kind of control, and grouping them together read as
    // more confusing than helpful while building this.
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("Target color correction"));

    // 2x2: Brightness/Contrast on top, Gamma/Saturation below - each
    // cell is its own label-above-slider pair, same growable-columns
    // approach as the refinement/chroma row below. A third, narrow column holds the reset button
    // beside the first slider row, so it does not need a row of its own.
    auto* grid = new wxFlexGridSizer(4, 3, 4, 8);
    grid->AddGrowableCol(0, 1);
    grid->AddGrowableCol(1, 1);

    brightness_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Brightness: 0.00"));
    contrast_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Contrast: 1.00"));
    grid->Add(brightness_label_, 0, wxALIGN_LEFT);
    grid->Add(contrast_label_, 0, wxALIGN_LEFT);
    grid->AddSpacer(0);

    brightness_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 0, -50, 50);
    contrast_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 100, 0, 200);
    grid->Add(brightness_, 0, wxEXPAND);
    grid->Add(contrast_, 0, wxEXPAND);

    auto* reset_btn = new wxBitmapButton(
        box->GetStaticBox(), wxID_ANY,
        MakeMaskIcon(kIconReset, kGuiIconSize, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT)));
    SetMaskIcon(reset_btn, kIconReset, kGuiIconSize);
    reset_btn->SetToolTip(_("Reset brightness, contrast, gamma and saturation to their neutral values"));
    grid->Add(reset_btn, 0, wxALIGN_CENTER_VERTICAL);

    gamma_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Gamma: 1.00"));
    saturation_label_ = new wxStaticText(box->GetStaticBox(), wxID_ANY, _("Saturation: 1.00"));
    grid->Add(gamma_label_, 0, wxALIGN_LEFT);
    grid->Add(saturation_label_, 0, wxALIGN_LEFT);
    grid->AddSpacer(0);

    gamma_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 100, 20, 300);
    saturation_ = new wxSlider(box->GetStaticBox(), wxID_ANY, 100, 0, 200);
    grid->Add(gamma_, 0, wxEXPAND);
    grid->Add(saturation_, 0, wxEXPAND);
    grid->AddSpacer(0);

    box->Add(grid, 0, wxEXPAND | wxALL, kPad);

    parent_sizer->Add(box, 0, wxEXPAND | wxBOTTOM, kBoxGap);

    brightness_->Bind(wxEVT_SLIDER, &ParameterPanel::OnBrightnessChanged, this);
    contrast_->Bind(wxEVT_SLIDER, &ParameterPanel::OnContrastChanged, this);
    gamma_->Bind(wxEVT_SLIDER, &ParameterPanel::OnGammaChanged, this);
    saturation_->Bind(wxEVT_SLIDER, &ParameterPanel::OnSaturationChanged, this);
    reset_btn->Bind(wxEVT_BUTTON, &ParameterPanel::OnResetColorCorrection, this);
}

void ParameterPanel::BuildPaletteBox(wxSizer* parent_sizer) {
    // Preparation for a future animation feature: reusing one palette
    // across several conversions avoids the color drift a freshly
    // re-optimized-per-image palette would otherwise cause between
    // frames that are mostly the same content (see the project's
    // README/history for the fuller ANIM-format discussion this grew
    // out of).
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("Palette"));

    auto* io_row = new wxBoxSizer(wxHORIZONTAL);
    load_palette_btn_ = new wxButton(box->GetStaticBox(), wxID_ANY, _("Load palette..."));
    save_palette_btn_ = new wxButton(box->GetStaticBox(), wxID_ANY, _("Save palette..."));
    io_row->Add(load_palette_btn_, 1, wxRIGHT, 4);
    io_row->Add(save_palette_btn_, 1);
    box->Add(io_row, 0, wxEXPAND | wxALL, kPad);

    // Disabled until NotifyPaletteGenerated or a successful load gives
    // it something to actually lock onto - see UpdatePaletteControlsEnabled.
    lock_palette_ = new wxCheckBox(box->GetStaticBox(), wxID_ANY, _("Lock current palette"));
    lock_palette_->Enable(false);

    // All 16 palette entries as always-editable swatches, kept in sync
    // with whatever the current palette actually is (see
    // NotifyPaletteGenerated/OnLoadPalette) - small, roughly square pickers in one row.
    // Editing any of them only feeds back into the
    // NEXT conversion depending on the lock checkboxes below: under
    // "Lock Palette" every edit here matters (the whole grid is used
    // as-is); under "Lock color 0" only picker 0's edits matter;
    // otherwise they're purely informational until overwritten by the
    // next generated/loaded palette.
    auto* color_grid = new wxGridSizer(1, 16, 0, 2);
    for (int i = 0; i < 16; ++i) {
        color_picker_[static_cast<size_t>(i)] =
            new wxColourPickerCtrl(box->GetStaticBox(), wxID_ANY, *wxBLACK, wxDefaultPosition, wxDefaultSize,
                                    wxCLRP_DEFAULT_STYLE & ~wxCLRP_SHOW_LABEL);
        color_picker_[static_cast<size_t>(i)]->SetMinSize(wxSize(kSwatchSize, kSwatchSize));
        color_picker_[static_cast<size_t>(i)]->SetToolTip(wxString::Format(_("Color %d"), i));
        color_grid->Add(color_picker_[static_cast<size_t>(i)], 0, wxALIGN_CENTER_HORIZONTAL);
    }
    box->Add(color_grid, 0, wxEXPAND | wxALL, kPad);

    // Only color 0 gets its own lock - see the header comment on lock_color0_ for why. The two
    // locks share a row below the swatches they are about.
    lock_color0_ = new wxCheckBox(box->GetStaticBox(), wxID_ANY, _("Lock color 0 (Amiga background)"));
    auto* lock_row = new wxBoxSizer(wxHORIZONTAL);
    lock_row->Add(lock_palette_, 0, wxALIGN_CENTER_VERTICAL);
    lock_row->AddStretchSpacer(1);
    lock_row->Add(lock_color0_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
    box->Add(lock_row, 0, wxEXPAND | wxALL, kPad);

    parent_sizer->Add(box, 0, wxEXPAND | wxBOTTOM, kBoxGap);

    load_palette_btn_->Bind(wxEVT_BUTTON, &ParameterPanel::OnLoadPalette, this);
    save_palette_btn_->Bind(wxEVT_BUTTON, &ParameterPanel::OnSavePalette, this);
    lock_palette_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { UpdatePaletteControlsEnabled(); });

    UpdatePaletteControlsEnabled();
}

void ParameterPanel::BuildOutputBox(wxSizer* parent_sizer) {
    auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("Output"));

    compress_ = new wxCheckBox(box->GetStaticBox(), wxID_ANY, _("ByteRun1 compression"));
    compress_->SetValue(true);
    box->Add(compress_, 0, wxALL, kPad);

    const wxString lace_choices[3] = {_("Automatic"), _("On"), _("Off")};
    lace_mode_ = new wxRadioBox(box->GetStaticBox(), wxID_ANY, _("Interlace flag"), wxDefaultPosition,
                                 wxDefaultSize, 3, lace_choices, 3, wxRA_SPECIFY_COLS);
    box->Add(lace_mode_, 0, wxEXPAND | wxALL, kPad);

    parent_sizer->Add(box, 0, wxEXPAND);
}

void ParameterPanel::OnCropEnableToggled(wxCommandEvent&) {
    UpdateCropControlsEnabled();
    NotifyCropChanged();
}

void ParameterPanel::OnCropUnitChanged(wxCommandEvent&) {
    // Preserve the actual pixel rectangle across the unit switch -
    // otherwise flipping px<->% would silently reinterpret the same
    // numbers as a different rectangle (and visibly jump the rubber-band).
    bool now_pct = crop_unit_pct_->GetValue();
    if (now_pct != crop_last_unit_percent_ && src_width_ > 0 && src_height_ > 0) {
        auto convert = [&](wxSpinCtrlDouble* ctrl, int dim) {
            double v = ctrl->GetValue();
            double px = crop_last_unit_percent_ ? (v / 100.0) * dim : v;
            ctrl->SetValue(now_pct ? (100.0 * px / dim) : px);
        };
        convert(crop_w_, src_width_);
        convert(crop_x_, src_width_);
        convert(crop_h_, src_height_);
        convert(crop_y_, src_height_);
    }
    crop_last_unit_percent_ = now_pct;
    UpdateCropSpinRanges();
    NotifyCropChanged();
}

void ParameterPanel::OnCropSpinChanged(wxSpinDoubleEvent& event) {
    if (IsCropRatioLocked()) {
        if (event.GetId() == crop_w_->GetId()) {
            ApplyRatioFromWidth();
        } else if (event.GetId() == crop_h_->GetId()) {
            ApplyRatioFromHeight();
        }
    }
    NotifyCropChanged();
}

void ParameterPanel::OnCropRatioLockToggled(wxCommandEvent&) {
    UpdateCropRatioControlsEnabled();
    if (crop_ratio_lock_->GetValue()) ApplyRatioFromWidth();
    NotifyCropChanged();
}

void ParameterPanel::OnCropRatioPresetChanged(wxCommandEvent&) {
    if (crop_ratio_lock_->GetValue()) ApplyRatioFromWidth();
    NotifyCropChanged();
}

void ParameterPanel::NotifyCropChanged() {
    // If a keyframe is currently selected, any edit to the crop rect
    // (spinners, rubber-band drag, ratio-lock snap - everything funnels
    // through here) updates that keyframe's stored rectangle, not just
    // the "live" crop state - this is what makes a selected keyframe
    // individually editable.
    int sel = keyframe_choice_->GetSelection();
    if (sel != wxNOT_FOUND && static_cast<size_t>(sel) < crop_keyframes_.size()) {
        auto rect = GetCropPixelRect();
        if (rect) crop_keyframes_[static_cast<size_t>(sel)] = *rect;
    }
    if (crop_changed_cb_) crop_changed_cb_();
    NotifyOptionsChanged();
}

void ParameterPanel::OnResolutionPresetChanged(wxCommandEvent&) { UpdateCustomResolutionEnabled(); }

void ParameterPanel::OnPaletteIterationsChanged(wxCommandEvent&) {
    palette_iterations_label_->SetLabel(
        wxString::Format(_("Refinement: %d passes"), palette_iterations_->GetValue()));
    SyncProfileChoice();
}

void ParameterPanel::OnQualitySettingChanged(wxCommandEvent&) { SyncProfileChoice(); }

void ParameterPanel::OnProfileChosen(wxCommandEvent&) {
    const auto& profiles = aic::quality_profiles();
    const int selection = profile_choice_->GetSelection();
    if (selection >= 0 && static_cast<size_t>(selection) < profiles.size())
        ApplyQualityProfile(profiles[static_cast<size_t>(selection)]);
    // "Custom" changes nothing; either way the choice shows what the three controls now add up to.
    SyncProfileChoice();
}

void ParameterPanel::ApplyQualityProfile(const aic::QualityProfileInfo& profile) {
    aic::ConversionOptions options;
    aic::apply_quality_profile(profile, &options);

    // A quantizer this build does not have (cannot happen with the built-in profiles) leaves the choice alone.
    quantizer_choice_->SetStringSelection(wxString::FromUTF8(options.quantizer_name.c_str()));

    const int passes = std::clamp(options.palette_refinement_iterations, palette_iterations_->GetMin(),
                                  palette_iterations_->GetMax());
    palette_iterations_->SetValue(passes);
    palette_iterations_label_->SetLabel(wxString::Format(_("Refinement: %d passes"), passes));

    // The choice lists "off" and then 2, 3 and 4 pixels; a lookahead of 0 or 1 means off.
    const int lookahead = std::clamp(options.lookahead, 1, static_cast<int>(lookahead_choice_->GetCount()));
    lookahead_choice_->SetSelection(lookahead - 1);
}

void ParameterPanel::SyncProfileChoice() {
    aic::ConversionOptions options;  // everything a profile does not touch stays at the defaults
    options.quantizer_name = quantizer_choice_->GetStringSelection().ToStdString();
    options.palette_refinement_iterations = palette_iterations_->GetValue();
    options.lookahead = lookahead_choice_->GetSelection() + 1;

    const std::optional<aic::QualityProfile> match = aic::matching_quality_profile(options);
    const auto& profiles = aic::quality_profiles();
    int index = static_cast<int>(profiles.size());  // "Custom"
    if (match) {
        for (size_t i = 0; i < profiles.size(); ++i)
            if (profiles[i].id == *match) index = static_cast<int>(i);
    }
    profile_choice_->SetSelection(index);
}

void ParameterPanel::OnChromaWeightChanged(wxCommandEvent&) {
    float v = chroma_weight_->GetValue() / 100.f;
    chroma_weight_label_->SetLabel(wxString::Format(_("Chroma <-> Luminance: %s"), FormatFloat(v)));
}

void ParameterPanel::OnDitherStrengthChanged(wxCommandEvent&) {
    dither_strength_label_->SetLabel(wxString::Format("%d%%", dither_strength_->GetValue()));
}

void ParameterPanel::OnDitherChoiceChanged(wxCommandEvent&) { UpdateDitherStrengthEnabled(); }

void ParameterPanel::UpdateDitherStrengthEnabled() {
    bool has_effect = dither_choice_->GetStringSelection() != "none";
    dither_strength_->Enable(has_effect);
    dither_strength_label_->Enable(has_effect);
}

void ParameterPanel::OnPixelAspectChanged(wxCommandEvent&) {
    float v = pixel_aspect_->GetValue() / 100.f;
    pixel_aspect_label_->SetLabel(FormatFloat(v));
}

void ParameterPanel::OnFitModeChanged(wxCommandEvent&) { UpdatePixelAspectEnabled(); }

void ParameterPanel::UpdatePixelAspectEnabled() {
    // Selection 2 is Stretch (see the fit_choices array in
    // BuildResolutionBox) - the only mode resize_image() never reads
    // pixel_aspect_ratio for at all.
    bool enabled = fit_mode_->GetSelection() != 2;
    pixel_aspect_caption_->Enable(enabled);
    pixel_aspect_->Enable(enabled);
    pixel_aspect_label_->Enable(enabled);
}

void ParameterPanel::OnBrightnessChanged(wxCommandEvent&) {
    float v = brightness_->GetValue() / 100.f;
    brightness_label_->SetLabel(wxString::Format(_("Brightness: %s"), FormatFloat(v)));
}

void ParameterPanel::OnContrastChanged(wxCommandEvent&) {
    float v = contrast_->GetValue() / 100.f;
    contrast_label_->SetLabel(wxString::Format(_("Contrast: %s"), FormatFloat(v)));
}

void ParameterPanel::OnGammaChanged(wxCommandEvent&) {
    float v = gamma_->GetValue() / 100.f;
    gamma_label_->SetLabel(wxString::Format(_("Gamma: %s"), FormatFloat(v)));
}

void ParameterPanel::OnSaturationChanged(wxCommandEvent&) {
    float v = saturation_->GetValue() / 100.f;
    saturation_label_->SetLabel(wxString::Format(_("Saturation: %s"), FormatFloat(v)));
}

void ParameterPanel::OnResetColorCorrection(wxCommandEvent&) {
    brightness_->SetValue(0);
    contrast_->SetValue(100);
    gamma_->SetValue(100);
    saturation_->SetValue(100);
    // SetValue() doesn't fire wxEVT_SLIDER, so the labels need updating
    // directly - same reasoning as everywhere else in this file that
    // sets a slider programmatically (e.g. LoadDefaults).
    brightness_label_->SetLabel(wxString::Format(_("Brightness: %s"), FormatFloat(0.f)));
    contrast_label_->SetLabel(wxString::Format(_("Contrast: %s"), FormatFloat(1.f)));
    gamma_label_->SetLabel(wxString::Format(_("Gamma: %s"), FormatFloat(1.f)));
    saturation_label_->SetLabel(wxString::Format(_("Saturation: %s"), FormatFloat(1.f)));
    NotifyOptionsChanged();  // the sliders moved without an event
}

void ParameterPanel::OnLoadPalette(wxCommandEvent&) {
    wxFileDialog dlg(this, _("Load Palette"), "", "", gui::PaletteLoadWildcard(), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    aic::Palette loaded;
    std::string error;
    if (!aic::load_palette(dlg.GetPath().utf8_string(), &loaded, &error)) {
        wxMessageBox(wxString::Format(_("Failed to load palette: %s"), wxString::FromUTF8(error)), _("Error"),
                     wxOK | wxICON_ERROR, this);
        return;
    }
    reference_palette_ = loaded;
    PopulateColorPickers(loaded);
    UpdatePaletteControlsEnabled();
    NotifyOptionsChanged();
}

void ParameterPanel::OnSavePalette(wxCommandEvent&) {
    if (!reference_palette_) return;  // button is disabled whenever this would be empty
    // One filter per format. The extension of the name typed decides how the file is written (as on
    // the command line); a name without one gets the extension of the selected filter.
    wxFileDialog dlg(this, _("Save Palette As"), "", "", gui::PaletteSaveWildcard(),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    const wxString path = gui::PaletteSavePath(dlg.GetPath(), dlg.GetFilterIndex());
    // The dialog only asked about the name as typed; with an extension added, the file may be one that exists.
    if (path != dlg.GetPath() && wxFileExists(path) &&
        wxMessageBox(wxString::Format(_("%s already exists. Replace it?"), path), _("Save Palette As"),
                     wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
        return;

    // Built from the pickers' current colors, not the stored snapshot -
    // saves whatever is actually shown/edited right now, consistent
    // with how "Lock Palette" also reads the live picker state.
    auto to4bit = [](int v8) {
        int v4 = static_cast<int>(std::lround(std::clamp(v8, 0, 255) / 17.0));
        return static_cast<uint8_t>(std::clamp(v4, 0, 15));
    };
    aic::Palette current;
    current.colors.resize(16);
    for (size_t i = 0; i < 16; ++i) {
        wxColour c = color_picker_[i]->GetColour();
        current.colors[i] = aic::AmigaColor{to4bit(c.Red()), to4bit(c.Green()), to4bit(c.Blue())};
    }

    std::string error;
    if (!aic::save_palette(current, path.utf8_string(), &error)) {
        wxMessageBox(wxString::Format(_("Failed to save palette: %s"), wxString::FromUTF8(error)), _("Error"),
                     wxOK | wxICON_ERROR, this);
    }
}

void ParameterPanel::UpdatePaletteControlsEnabled() {
    bool available = reference_palette_.has_value();
    lock_palette_->Enable(available);
    save_palette_btn_->Enable(available);
    if (!available) lock_palette_->SetValue(false);  // nothing left to stay locked to

    // A locked palette is used as-is: quantization AND refinement are
    // both skipped entirely for it (see HAM6Encoder::encode), so
    // Palette quantizer / Color focus / Refinement / Chroma<->Luminance
    // have no effect while it's active. Greying them out makes that
    // silent override visible instead of leaving fully-interactive
    // controls that quietly do nothing. Dithering is untouched here -
    // it still applies to whichever palette (locked or not) ends up in
    // effect.
    bool locked = lock_palette_->GetValue();
    profile_choice_->Enable(!locked);
    quantizer_choice_->Enable(!locked);
    importance_choice_->Enable(!locked);
    palette_iterations_->Enable(!locked);
    chroma_weight_->Enable(!locked);
}

void ParameterPanel::PopulateColorPickers(const aic::Palette& palette) {
    auto rep = [](uint8_t v4) { return static_cast<uint8_t>((v4 << 4) | v4); };
    for (size_t i = 0; i < color_picker_.size() && i < palette.colors.size(); ++i) {
        const auto& c = palette.colors[i];
        color_picker_[i]->SetColour(wxColour(rep(c.r4), rep(c.g4), rep(c.b4)));
    }
}

void ParameterPanel::NotifyPaletteGenerated(const aic::Palette& palette) {
    reference_palette_ = palette;
    PopulateColorPickers(palette);
    UpdatePaletteControlsEnabled();
}

void ParameterPanel::UpdateCropControlsEnabled() {
    bool on = crop_enable_->GetValue();
    for (auto* c : {crop_w_, crop_h_, crop_x_, crop_y_}) c->Enable(on);
    crop_unit_px_->Enable(on);
    crop_unit_pct_->Enable(on);
    UpdateCropRatioControlsEnabled();
    UpdateKeyframeControlsEnabled();
}

void ParameterPanel::UpdateCropRatioControlsEnabled() {
    bool on = crop_enable_->GetValue();
    crop_ratio_lock_->Enable(on);
    crop_ratio_choice_->Enable(on && crop_ratio_lock_->GetValue());
}

void ParameterPanel::UpdateKeyframeControlsEnabled() {
    bool on = crop_enable_->GetValue() && animation_enable_->GetValue();
    add_keyframe_btn_->Enable(on);
    keyframe_choice_->Enable(on && !crop_keyframes_.empty());
    remove_keyframe_btn_->Enable(on && keyframe_choice_->GetSelection() != wxNOT_FOUND);
    frames_per_segment_->Enable(animation_enable_->GetValue());
    anim_speed_->Enable(animation_enable_->GetValue());
    output_mode_->Enable(animation_enable_->GetValue());
    auto_preview_->Enable(!animation_enable_->GetValue());  // repeats the single-image preview
    preview_loop_->Enable(animation_enable_->GetValue());
    preview_pingpong_->Enable(animation_enable_->GetValue());
    // With Animation off, this button takes over the former standalone
    // Convert button's job instead (single-frame HAM preview) - always
    // enabled in that mode, same as Convert always was, rather than
    // gated on having a keyframe sequence to play. With Animation on,
    // it plays that sequence back and so needs one - same minimum as
    // RunAnimationExport.
    if (animation_enable_->GetValue()) {
        play_preview_btn_->Enable(on && crop_keyframes_.size() >= 2);
    } else {
        play_preview_btn_->Enable(true);
    }
    UpdatePlayPreviewButtonLabel();
}

void ParameterPanel::UpdatePlayPreviewButtonLabel() {
    bool anim = animation_enable_->GetValue();
    SetMaskIcon(play_preview_btn_, kIconPlay, kGuiIconSize, 16);
    play_preview_btn_->SetLabel(anim ? _("Play preview") : _("Show HAM preview"));
    play_preview_btn_->SetToolTip(anim ? _("Play the animation preview (F5)")
                                        : _("Convert and show a single-frame preview (F5)"));
}

void ParameterPanel::OnAnimationEnableToggled(wxCommandEvent&) {
    // A brand new animation starting with zero keyframes is an empty,
    // slightly confusing state - auto-seed "Keyframe 1" from whatever
    // the crop is currently set to, so there's always at least a
    // starting point to adjust rather than an empty list plus a
    // separate "now click Add" step.
    if (animation_enable_->GetValue() && crop_keyframes_.empty()) AddKeyframeFromCurrentCrop();
    UpdateKeyframeControlsEnabled();
    if (animation_toggled_cb_) animation_toggled_cb_(animation_enable_->GetValue());
}

void ParameterPanel::OnPlayPreviewClicked(wxCommandEvent&) {
    if (play_preview_toggled_cb_) play_preview_toggled_cb_();
}

void ParameterPanel::SetPreviewPlaying(bool playing) {
    if (playing) {
        SetMaskIcon(play_preview_btn_, kIconStop, kGuiIconSize, 16);
        play_preview_btn_->SetLabel(_("Stop preview"));
        play_preview_btn_->SetToolTip(_("Stop the animation preview (F5)"));
    } else {
        UpdatePlayPreviewButtonLabel();
    }
}

void ParameterPanel::AddKeyframeFromCurrentCrop() {
    auto rect = GetCropPixelRect();
    if (!rect) return;  // crop off, or no source image loaded
    crop_keyframes_.push_back(*rect);
    keyframe_choice_->Append(wxString::Format(_("Keyframe %d"), static_cast<int>(crop_keyframes_.size())));
    keyframe_choice_->SetSelection(static_cast<int>(crop_keyframes_.size()) - 1);
    // Auto-selected on explicit request, so the crop controls
    // (spinners, rubber band) immediately act on the keyframe that was
    // just created - not just visually indicated as "current", but
    // actually live-editable right away, matching every other already-
    // existing keyframe once selected (see NotifyCropChanged).
    //
    // This reintroduces the exact trade-off documented (and previously
    // removed) here before: because a selected keyframe absorbs
    // further crop edits, the SAFE way to build up several keyframes
    // is "Add (captures + selects) -> reposition (updates that SAME,
    // still-selected entry) -> Add again (forks a new entry from the
    // now-repositioned state, selecting THAT one) -> reposition ->
    // Add -> ...". Verified: Add(100) -> width 150 -> Add -> width 200
    // leaves keyframe 1 at 150 and keyframe 2 at 200, correctly
    // distinct. The UNSAFE order - reposition immediately after Add,
    // before adding the next one - silently collapses the earlier
    // entry into the later one, because the reposition step lands on
    // whichever entry is already selected. Verified: Add(100) -> width
    // 150 -> Add again leaves BOTH keyframes at 150, the original 100
    // gone - reproducing the exact regression this box used to avoid
    // by not auto-selecting. Accepted on explicit request; the correct
    // way to add a second, distinct keyframe is to click "Add keyframe"
    // again BEFORE repositioning, not after.
    UpdateKeyframeControlsEnabled();
    if (animation_composition_changed_cb_) animation_composition_changed_cb_();
}

void ParameterPanel::OnAddKeyframe(wxCommandEvent&) { AddKeyframeFromCurrentCrop(); }

void ParameterPanel::OnRemoveKeyframe(wxCommandEvent&) {
    int sel = keyframe_choice_->GetSelection();
    if (sel == wxNOT_FOUND || static_cast<size_t>(sel) >= crop_keyframes_.size()) return;
    crop_keyframes_.erase(crop_keyframes_.begin() + sel);
    // Rebuild labels wholesale - simplest correct way to keep "Keyframe
    // N" matching each entry's new index after the removed one shifts
    // everything after it down by one.
    keyframe_choice_->Clear();
    for (size_t i = 0; i < crop_keyframes_.size(); ++i) {
        keyframe_choice_->Append(wxString::Format(_("Keyframe %d"), static_cast<int>(i) + 1));
    }
    UpdateKeyframeControlsEnabled();
    if (animation_composition_changed_cb_) animation_composition_changed_cb_();
}

void ParameterPanel::OnKeyframeSelected(wxCommandEvent&) {
    int sel = keyframe_choice_->GetSelection();
    if (sel == wxNOT_FOUND || static_cast<size_t>(sel) >= crop_keyframes_.size()) return;
    SetCropPixelRect(crop_keyframes_[static_cast<size_t>(sel)]);
    UpdateKeyframeControlsEnabled();
    // SetCropPixelRect() only sets the spinner values (SetValue() doesn't
    // fire a change event) - without this, the rubber-band overlay on
    // the source image never learns the crop rect just changed.
    NotifyCropChanged();
}

void ParameterPanel::UpdateCropSpinRanges() {
    bool pct = crop_unit_pct_->GetValue();
    double src_w = pct ? 100.0 : static_cast<double>(src_width_ > 0 ? src_width_ : 4096);
    double src_h = pct ? 100.0 : static_cast<double>(src_height_ > 0 ? src_height_ : 4096);
    // Generous headroom (4x the source dimension) rather than capping
    // at the source size itself: the rubber-band (ClampRectToImage) has
    // no such cap, so a box dragged larger than the source would
    // otherwise display (and, worse, get silently written back as)
    // a clamped value the moment SetCropPixelRect() - e.g. when
    // switching between animation keyframes - tries to show it here.
    // W/H and X/Y share the same multiple so a maximally large box can
    // still be moved far enough to fully clear either edge.
    double max_w = src_w * 4.0;
    double max_h = src_h * 4.0;
    crop_w_->SetRange(0, max_w);
    crop_h_->SetRange(0, max_h);
    // X/Y allow negative values too, so the crop box can extend past
    // any edge of the source image - or lie entirely outside it - for
    // an animation keyframe sequence to "wander into"/"wander out of"
    // frame. See crop_image's fill_color for what covers the resulting
    // out-of-source area.
    crop_x_->SetRange(-max_w, max_w);
    crop_y_->SetRange(-max_h, max_h);
}

void ParameterPanel::UpdateCustomResolutionEnabled() {
    int sel = resolution_preset_->GetSelection();
    bool is_custom = (sel != wxNOT_FOUND) && (static_cast<size_t>(sel) == presets_.size() - 1);
    custom_width_->Enable(is_custom);
    custom_height_->Enable(is_custom);
}

void ParameterPanel::SetSourceSize(int width, int height) {
    src_width_ = width;
    src_height_ = height;
    if (crop_w_->GetValue() <= 0.001) crop_w_->SetValue(width);
    if (crop_h_->GetValue() <= 0.001) crop_h_->SetValue(height);
    UpdateCropSpinRanges();
}

void ParameterPanel::ApplySourceTransform(const aic::ImageTransform& delta, int old_w, int old_h) {
    if (delta.is_identity() || old_w <= 0 || old_h <= 0) return;
    const bool pct = crop_unit_pct_->GetValue();
    auto to_px = [&](double v, int dim) { return pct ? (v / 100.0) * dim : v; };
    auto rounded = [](const aic::ImageTransform::Rect& r) {
        return wxRect(static_cast<int>(std::lround(r.x)), static_cast<int>(std::lround(r.y)),
                      static_cast<int>(std::lround(r.w)), static_cast<int>(std::lround(r.h)));
    };

    // The crop controls keep meaningful values even while crop is disabled, so
    // they are mapped either way (enabling crop later then shows the right box).
    const aic::ImageTransform::Rect crop_px = delta.map_rect(
        {to_px(crop_x_->GetValue(), old_w), to_px(crop_y_->GetValue(), old_h), to_px(crop_w_->GetValue(), old_w),
         to_px(crop_h_->GetValue(), old_h)},
        old_w, old_h);
    for (wxRect& k : crop_keyframes_) {
        k = rounded(delta.map_rect({double(k.x), double(k.y), double(k.width), double(k.height)}, old_w, old_h));
    }

    // New size first: it sets the spin ranges the mapped values must fit into.
    src_width_ = delta.result_width(old_w, old_h);
    src_height_ = delta.result_height(old_w, old_h);
    UpdateCropSpinRanges();
    auto to_unit = [&](double px, int dim) { return pct ? (100.0 * px / dim) : px; };
    crop_x_->SetValue(to_unit(crop_px.x, src_width_));
    crop_y_->SetValue(to_unit(crop_px.y, src_height_));
    crop_w_->SetValue(to_unit(crop_px.w, src_width_));
    crop_h_->SetValue(to_unit(crop_px.h, src_height_));
}

bool ParameterPanel::IsCropRatioLocked() const { return crop_enable_->GetValue() && crop_ratio_lock_->GetValue(); }

double ParameterPanel::GetCropAspectRatio() const {
    int sel = crop_ratio_choice_->GetSelection();
    if (sel == wxNOT_FOUND || static_cast<size_t>(sel) >= ratio_presets_.size()) return 1.0;
    const auto& p = ratio_presets_[static_cast<size_t>(sel)];
    if (p.height <= 0) return 1.0;
    return static_cast<double>(p.width) / p.height;
}

void ParameterPanel::ApplyRatioFromWidth() {
    if (src_width_ <= 0 || src_height_ <= 0) return;
    double ratio = GetCropAspectRatio();
    if (ratio <= 0.0) return;
    bool pct = crop_unit_pct_->GetValue();
    double w_px = pct ? (crop_w_->GetValue() / 100.0) * src_width_ : crop_w_->GetValue();
    double h_px = w_px / ratio;
    crop_h_->SetValue(pct ? (100.0 * h_px / src_height_) : h_px);
}

void ParameterPanel::ApplyRatioFromHeight() {
    if (src_width_ <= 0 || src_height_ <= 0) return;
    double ratio = GetCropAspectRatio();
    if (ratio <= 0.0) return;
    bool pct = crop_unit_pct_->GetValue();
    double h_px = pct ? (crop_h_->GetValue() / 100.0) * src_height_ : crop_h_->GetValue();
    double w_px = h_px * ratio;
    crop_w_->SetValue(pct ? (100.0 * w_px / src_width_) : w_px);
}

std::optional<wxRect> ParameterPanel::GetCropPixelRect() const {
    if (!crop_enable_->GetValue() || src_width_ <= 0 || src_height_ <= 0) return std::nullopt;
    bool pct = crop_unit_pct_->GetValue();
    auto resolve = [&](double v, int dim) { return pct ? (v / 100.0) * dim : v; };
    int x = static_cast<int>(std::lround(resolve(crop_x_->GetValue(), src_width_)));
    int y = static_cast<int>(std::lround(resolve(crop_y_->GetValue(), src_height_)));
    int w = static_cast<int>(std::lround(resolve(crop_w_->GetValue(), src_width_)));
    int h = static_cast<int>(std::lround(resolve(crop_h_->GetValue(), src_height_)));
    return wxRect(x, y, std::max(1, w), std::max(1, h));
}

void ParameterPanel::SetCropPixelRect(const wxRect& rect_px) {
    if (src_width_ <= 0 || src_height_ <= 0) return;
    bool pct = crop_unit_pct_->GetValue();
    auto to_unit = [&](int v, int dim) { return pct ? (100.0 * v / dim) : static_cast<double>(v); };
    crop_x_->SetValue(to_unit(rect_px.x, src_width_));
    crop_y_->SetValue(to_unit(rect_px.y, src_height_));
    crop_w_->SetValue(to_unit(rect_px.width, src_width_));
    crop_h_->SetValue(to_unit(rect_px.height, src_height_));
    if (!crop_enable_->GetValue()) {
        crop_enable_->SetValue(true);
        UpdateCropControlsEnabled();
    }
}

aic::ConversionOptions ParameterPanel::GetOptions() const {
    aic::ConversionOptions opts;

    if (crop_enable_->GetValue()) {
        bool pct = crop_unit_pct_->GetValue();
        aic::CropSpec spec;
        spec.width = {static_cast<float>(crop_w_->GetValue()), pct};
        spec.height = {static_cast<float>(crop_h_->GetValue()), pct};
        spec.x = {static_cast<float>(crop_x_->GetValue()), pct};
        spec.y = {static_cast<float>(crop_y_->GetValue()), pct};
        opts.crop = spec;
    }

    int sel = resolution_preset_->GetSelection();
    bool is_custom = (sel == wxNOT_FOUND) || (static_cast<size_t>(sel) == presets_.size() - 1);
    if (is_custom) {
        opts.target_width = custom_width_->GetValue();
        opts.target_height = custom_height_->GetValue();
    } else {
        opts.target_width = presets_[static_cast<size_t>(sel)].width;
        opts.target_height = presets_[static_cast<size_t>(sel)].height;
    }

    switch (fit_mode_->GetSelection()) {
        case 0: opts.fit_mode = aic::FitMode::Fit; break;
        case 1: opts.fit_mode = aic::FitMode::Fill; break;
        default: opts.fit_mode = aic::FitMode::Stretch; break;
    }
    opts.pixel_aspect_ratio = pixel_aspect_->GetValue() / 100.f;
    opts.color_correction.brightness = brightness_->GetValue() / 100.f;
    opts.color_correction.contrast = contrast_->GetValue() / 100.f;
    opts.color_correction.gamma = gamma_->GetValue() / 100.f;
    opts.color_correction.saturation = saturation_->GetValue() / 100.f;

    auto to4bit = [](int v8) {
        int v4 = static_cast<int>(std::lround(std::clamp(v8, 0, 255) / 17.0));
        return static_cast<uint8_t>(std::clamp(v4, 0, 15));
    };
    auto picker_to_amiga = [&](size_t i) {
        wxColour c = color_picker_[i]->GetColour();
        return aic::AmigaColor{to4bit(c.Red()), to4bit(c.Green()), to4bit(c.Blue())};
    };

    if (lock_palette_->GetValue() && reference_palette_) {
        // Built from the 16 pickers' CURRENT colors, not the stored
        // snapshot - so an edit made to any of them while locked (see
        // the box's own label/comment) actually takes effect on the
        // next conversion, matching "retains the current palette
        // completely", not just whatever it was when it first became
        // available. No quantization/refinement runs against this at
        // all (see HAM6Encoder::encode).
        aic::Palette p;
        p.colors.resize(16);
        for (size_t i = 0; i < 16; ++i) p.colors[i] = picker_to_amiga(i);
        opts.locked_palette = p;
    } else if (lock_color0_->GetValue()) {
        // Only takes effect without a full palette lock - forces just
        // color 0 (the Amiga's border/background color) while colors
        // 1-15 are still freshly, optimally quantized/refined.
        opts.fixed_colors[0] = picker_to_amiga(0);
    }
    {
        // Always taken from color 0's picker specifically, independent
        // of "Lock color 0" (that one only gates whether it's forced
        // into a freshly generated palette) - the crop box's
        // out-of-source fill uses whatever color is currently shown
        // there regardless, so it stays visually consistent with the
        // picker, and defaults to black since that's the picker's own
        // default.
        wxColour c = color_picker_[0]->GetColour();
        opts.crop_fill_color = aic::RGBf{c.Red() / 255.f, c.Green() / 255.f, c.Blue() / 255.f};
    }

    opts.ditherer_name = dither_choice_->GetStringSelection().ToStdString();
    opts.quantizer_name = quantizer_choice_->GetStringSelection().ToStdString();
    opts.importance_name = importance_choice_->GetStringSelection().ToStdString();
    opts.palette_refinement_iterations = palette_iterations_->GetValue();
    opts.chroma_weight = chroma_weight_->GetValue() / 100.f;
    opts.dither_strength = dither_strength_->GetValue() / 100.f;
    opts.line_start_modify = line_start_modify_->GetValue();
    opts.lookahead = lookahead_choice_->GetSelection() + 1;

    opts.compress = compress_->GetValue();
    switch (lace_mode_->GetSelection()) {
        case 1: opts.lace = true; break;
        case 2: opts.lace = false; break;
        default: opts.lace = std::nullopt; break;
    }

    return opts;
}

void ParameterPanel::LoadDefaults() {
    wxConfigBase* cfg = wxConfig::Get();
    long v;

    if (cfg->Read("Defaults/ResolutionPreset", &v) && v >= 0 && static_cast<size_t>(v) < presets_.size()) {
        resolution_preset_->SetSelection(static_cast<int>(v));
    }
    if (cfg->Read("Defaults/CustomWidth", &v)) custom_width_->SetValue(static_cast<int>(v));
    if (cfg->Read("Defaults/CustomHeight", &v)) custom_height_->SetValue(static_cast<int>(v));
    UpdateCustomResolutionEnabled();

    if (cfg->Read("Defaults/FitMode", &v) && v >= 0 && v <= 2) fit_mode_->SetSelection(static_cast<int>(v));
    UpdatePixelAspectEnabled();

    wxString s;
    if (cfg->Read("Defaults/Dither", &s) && !s.IsEmpty()) dither_choice_->SetStringSelection(s);
    if (cfg->Read("Defaults/Quantizer", &s) && !s.IsEmpty()) quantizer_choice_->SetStringSelection(s);
    if (cfg->Read("Defaults/Importance", &s) && !s.IsEmpty()) importance_choice_->SetStringSelection(s);

    if (cfg->Read("Defaults/DitherStrength", &v) && v >= 0 && v <= 100) {
        dither_strength_->SetValue(static_cast<int>(v));
        dither_strength_label_->SetLabel(wxString::Format("%d%%", static_cast<int>(v)));
    }
    UpdateDitherStrengthEnabled();

    if (cfg->Read("Defaults/AnimFps", &v)) {  // stored as the fps value itself, not as a list index
        for (size_t i = 0; i < sizeof(kAnimFpsChoices) / sizeof(kAnimFpsChoices[0]); ++i) {
            if (kAnimFpsChoices[i] == v) anim_speed_->SetSelection(static_cast<int>(i));
        }
    }

    bool lsm = false;
    if (cfg->Read("Defaults/LineStartModify", &lsm)) line_start_modify_->SetValue(lsm);
    if (cfg->Read("Defaults/Lookahead", &v) && v >= 0 && v < static_cast<long>(lookahead_choice_->GetCount()))
        lookahead_choice_->SetSelection(static_cast<int>(v));

    if (cfg->Read("Defaults/RefinementPasses", &v)) {
        palette_iterations_->SetValue(static_cast<int>(v));
        palette_iterations_label_->SetLabel(wxString::Format(_("Refinement: %d passes"), static_cast<int>(v)));
    }
    SyncProfileChoice();  // the profile is not stored itself: it follows from the three settings just read
    if (cfg->Read("Defaults/ChromaWeight", &v)) {
        chroma_weight_->SetValue(static_cast<int>(v));
        chroma_weight_label_->SetLabel(
            wxString::Format(_("Chroma <-> Luminance: %s"), FormatFloat(static_cast<int>(v) / 100.f)));
    }
    if (cfg->Read("Defaults/Brightness", &v)) {
        brightness_->SetValue(static_cast<int>(v));
        brightness_label_->SetLabel(wxString::Format(_("Brightness: %s"), FormatFloat(static_cast<int>(v) / 100.f)));
    }
    if (cfg->Read("Defaults/Contrast", &v)) {
        contrast_->SetValue(static_cast<int>(v));
        contrast_label_->SetLabel(wxString::Format(_("Contrast: %s"), FormatFloat(static_cast<int>(v) / 100.f)));
    }
    if (cfg->Read("Defaults/Gamma", &v)) {
        gamma_->SetValue(static_cast<int>(v));
        gamma_label_->SetLabel(wxString::Format(_("Gamma: %s"), FormatFloat(static_cast<int>(v) / 100.f)));
    }
    if (cfg->Read("Defaults/Saturation", &v)) {
        saturation_->SetValue(static_cast<int>(v));
        saturation_label_->SetLabel(wxString::Format(_("Saturation: %s"), FormatFloat(static_cast<int>(v) / 100.f)));
    }

    bool b;
    if (cfg->Read("Defaults/FixColor0Enabled", &b)) lock_color0_->SetValue(b);
    long cr, cg, cb;
    if (cfg->Read("Defaults/FixColor0R", &cr) && cfg->Read("Defaults/FixColor0G", &cg) &&
        cfg->Read("Defaults/FixColor0B", &cb)) {
        color_picker_[0]->SetColour(wxColour(static_cast<int>(cr), static_cast<int>(cg), static_cast<int>(cb)));
    }

    if (cfg->Read("Defaults/Compress", &b)) compress_->SetValue(b);
    if (cfg->Read("Defaults/AutoPreview", &b)) auto_preview_->SetValue(b);
    if (cfg->Read("Defaults/LaceMode", &v) && v >= 0 && v <= 2) lace_mode_->SetSelection(static_cast<int>(v));
}

int ParameterPanel::GetAnimationFps() const {
    const int sel = anim_speed_->GetSelection();
    const int count = static_cast<int>(sizeof(kAnimFpsChoices) / sizeof(kAnimFpsChoices[0]));
    return kAnimFpsChoices[(sel >= 0 && sel < count) ? sel : kDefaultAnimFpsIndex];
}

void ParameterPanel::SaveAsDefaults() const {
    wxConfigBase* cfg = wxConfig::Get();
    cfg->Write("Defaults/ResolutionPreset", static_cast<long>(resolution_preset_->GetSelection()));
    cfg->Write("Defaults/CustomWidth", static_cast<long>(custom_width_->GetValue()));
    cfg->Write("Defaults/CustomHeight", static_cast<long>(custom_height_->GetValue()));
    cfg->Write("Defaults/FitMode", static_cast<long>(fit_mode_->GetSelection()));
    cfg->Write("Defaults/Dither", dither_choice_->GetStringSelection());
    cfg->Write("Defaults/DitherStrength", static_cast<long>(dither_strength_->GetValue()));
    cfg->Write("Defaults/LineStartModify", line_start_modify_->GetValue());
    cfg->Write("Defaults/Lookahead", static_cast<long>(lookahead_choice_->GetSelection()));
    cfg->Write("Defaults/AnimFps", static_cast<long>(GetAnimationFps()));
    cfg->Write("Defaults/Quantizer", quantizer_choice_->GetStringSelection());
    cfg->Write("Defaults/Importance", importance_choice_->GetStringSelection());
    cfg->Write("Defaults/RefinementPasses", static_cast<long>(palette_iterations_->GetValue()));
    cfg->Write("Defaults/ChromaWeight", static_cast<long>(chroma_weight_->GetValue()));
    cfg->Write("Defaults/Brightness", static_cast<long>(brightness_->GetValue()));
    cfg->Write("Defaults/Contrast", static_cast<long>(contrast_->GetValue()));
    cfg->Write("Defaults/Gamma", static_cast<long>(gamma_->GetValue()));
    cfg->Write("Defaults/Saturation", static_cast<long>(saturation_->GetValue()));
    cfg->Write("Defaults/FixColor0Enabled", lock_color0_->GetValue());
    {
        wxColour c = color_picker_[0]->GetColour();
        cfg->Write("Defaults/FixColor0R", static_cast<long>(c.Red()));
        cfg->Write("Defaults/FixColor0G", static_cast<long>(c.Green()));
        cfg->Write("Defaults/FixColor0B", static_cast<long>(c.Blue()));
    }
    cfg->Write("Defaults/Compress", compress_->GetValue());
    cfg->Write("Defaults/AutoPreview", auto_preview_->GetValue());
    cfg->Write("Defaults/LaceMode", static_cast<long>(lace_mode_->GetSelection()));
    cfg->Flush();
}
