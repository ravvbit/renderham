// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// ParameterPanel.hpp
//
// All aic::ConversionOptions controls, grouped to match the design
// mockup: crop, target resolution/fit, color processing, output. Built
// as a single self-contained wxPanel (not entangled with "the one open
// file") so a future batch-mode file list can reuse it unchanged as a
// shared settings template.
//
// Text entry is avoided wherever a picker/slider/spinner reasonably
// covers the range - see the individual control comments below for the
// reasoning per field.

#include <wx/clrpicker.h>
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "aic/aic.hpp"

class ParameterPanel : public wxScrolledWindow {
public:
    explicit ParameterPanel(wxWindow* parent);
    ~ParameterPanel() override;  // out of line: ChangeWatcher is incomplete here

    // Height the panel needs to show every control without scrolling. The
    // frame uses it to cap the panel on small screens, where the panel then
    // scrolls instead of pushing the window off the screen.
    int NaturalHeight() const;

    // Builds a full ConversionOptions from the current control states.
    aic::ConversionOptions GetOptions() const;

    // Lets crop spinner ranges/labels make sense once an image is
    // loaded (pixel-mode max = source dimension).
    void SetSourceSize(int width, int height);

    // The source image was rotated/mirrored by `delta` (old_w x old_h before):
    // moves the crop box and every animation keyframe along with the picture
    // and switches to the new source size. Does not change which keyframe is
    // selected or whether crop is enabled.
    void ApplySourceTransform(const aic::ImageTransform& delta, int old_w, int old_h);

    // Crop rectangle in image PIXEL coordinates, resolving percent
    // values against the current source size - nullopt if crop is
    // disabled or no source size is known yet. Used to draw/sync the
    // rubber-band overlay on the source ImageView.
    std::optional<wxRect> GetCropPixelRect() const;

    // Updates the four crop spinners (converting to the currently
    // selected unit) from a pixel-space rect and enables crop if it
    // wasn't already. Does NOT itself write back into a selected
    // keyframe or notify listeners - callers that represent a genuine
    // user edit (not just mirroring another widget) must follow up
    // with NotifyCropChanged themselves; see MainFrame::OnCropOverlayEdited.
    void SetCropPixelRect(const wxRect& rect_px);

    // Writes the current crop rect back into the selected keyframe (if
    // any) and notifies the crop-changed callback. Called internally
    // by every genuine crop edit (spinners, ratio-lock snapping,
    // keyframe selection); external callers that update the crop via
    // SetCropPixelRect on the user's behalf - namely the rubber-band
    // drag path in MainFrame - must call this afterward too, or that
    // edit is visible in the spinners but never actually saved.
    void NotifyCropChanged();

    // Invoked whenever the user changes anything that affects the crop
    // rectangle (checkbox, unit toggle, any of the four spinners) via
    // the panel's own controls - i.e. the spinners-edited-it-so-update-
    // the-rubber-band direction.
    void SetCropChangedCallback(std::function<void()> cb) { crop_changed_cb_ = std::move(cb); }
    // Fires whenever "Enable animation" is toggled, so MainFrame can
    // keep the Save/Save As button labels honest about what clicking
    // them currently does (see MainFrame::UpdateSaveButtonLabels).
    void SetAnimationToggledCallback(std::function<void(bool)> cb) { animation_toggled_cb_ = std::move(cb); }

    // "Auto preview" toggle: the frame converts again by itself after a pause once a setting
    // changed. Saved with the other settings by "Save current settings".
    bool IsAutoPreviewEnabled() const { return auto_preview_->GetValue(); }
    // Invoked after anything that changes what a single-image preview would look like: a control
    // the user changed, a crop edit (spinners or rubber band), what the panel itself sets (quality
    // profile, "reset" of the color correction, a loaded palette), and the auto-preview checkbox
    // itself. Not for animation settings, not for the palette a conversion reports back. It says
    // nothing about WHAT changed and may come several times for one change; the frame restarts
    // its pause timer each time.
    void SetOptionsChangedCallback(std::function<void()> cb) { options_changed_cb_ = std::move(cb); }

    // Whether crop is enabled AND the aspect-ratio lock is checked -
    // this is what ImageView's rubber-band drag consults to decide
    // whether it needs to preserve the ratio while resizing.
    bool IsCropRatioLocked() const;

    // Current locked ratio (width / height) from the selected preset.
    // Only meaningful when IsCropRatioLocked() is true.
    double GetCropAspectRatio() const;

    // Reads every control this class owns EXCEPT crop (crop is always
    // per-image, never a saved default) from wxConfig and applies it.
    // Missing keys (first run, or a key added by a later version) just
    // leave that control at its hardcoded constructor default - no
    // separate "has defaults" flag needed.
    void LoadDefaults();

    // Writes the current state of every control this class owns EXCEPT
    // crop into wxConfig, under the "Defaults/" group.
    void SaveAsDefaults() const;

    // Called by MainFrame after every successful conversion. Becomes
    // the reference palette "Lock Palette" uses if the user hasn't
    // explicitly loaded a different one more recently - see
    // BuildPaletteBox for the full reasoning. Also enables the
    // "Lock current palette" checkbox and "Save palette..." button the first
    // time this is ever called (before that, there's nothing to lock
    // to or save).
    void NotifyPaletteGenerated(const aic::Palette& palette);

    // Crop rectangles the user has explicitly captured via "Add
    // Keyframe", in source-pixel space, in the order added. Empty
    // unless crop is enabled and at least one keyframe exists. Not
    // used yet within this class itself - MainFrame will read this to
    // drive interpolated animation-frame generation in a later step.
    const std::vector<wxRect>& GetCropKeyframes() const { return crop_keyframes_; }

    // Governs whether Save/Save As generate a single image (false) or
    // an interpolated frame sequence (true) - see MainFrame's
    // RunAnimationExport, which is the actual consumer of this and
    // GetFramesPerSegment/GetCropKeyframes.
    bool IsAnimationEnabled() const { return animation_enable_->GetValue(); }
    int GetFramesPerSegment() const { return frames_per_segment_->GetValue(); }
    // false = one numbered IFF file per frame (existing behavior),
    // true = a single delta-compressed IFF FORM ANIM file.
    bool IsAnimFileOutput() const { return output_mode_->GetSelection() == 1; }
    // Playback speed in frames per second: one setting for the in-app preview and for
    // the ANIM file (see aic::anim_jiffies_for_fps), so the preview shows the speed the
    // exported file will play at. Only speeds that divide 60 are offered - those are
    // the ones an ANIM (counted in 1/60 s) can express exactly.
    int GetAnimationFps() const;
    // Called when the speed choice changes (MainFrame retimes a running preview).
    void SetAnimationSpeedChangedCallback(std::function<void()> cb) { animation_speed_changed_cb_ = std::move(cb); }

    // Read live on every playback tick (see MainFrame::OnPreviewTimer),
    // so toggling either while a preview is already playing takes
    // effect immediately rather than requiring a stop/restart.
    bool IsPreviewLoopEnabled() const { return preview_loop_->GetValue(); }
    bool IsPreviewPingPongEnabled() const { return preview_pingpong_->GetValue(); }
    // Updates the Play/Stop button's own label - called by MainFrame
    // whenever ITS OWN playback state changes (starting, stopping, or
    // reaching the end of a non-looping sequence), since MainFrame owns
    // the timer and therefore the actual "is it playing" state.
    void SetPreviewPlaying(bool playing);
    // For the Ctrl+K keyboard shortcut (see MainFrame) - identical to
    // clicking "Add keyframe" itself, including silently doing nothing
    // if crop isn't enabled or no source image is loaded (the same
    // condition that keeps the button itself disabled).
    void TriggerAddKeyframe() { AddKeyframeFromCurrentCrop(); }
    // Fires when the Play/Stop button is clicked - carries no
    // direction of its own; MainFrame decides start vs. stop from its
    // own state and reports the result back via SetPreviewPlaying.
    void SetPlayPreviewToggledCallback(std::function<void()> cb) { play_preview_toggled_cb_ = std::move(cb); }
    // Fires whenever anything that changes which frames an animation
    // preview/export would produce happens outside of a plain crop
    // edit (which already goes through SetCropChangedCallback) - Add/
    // Remove Keyframe and the frames-per-segment spinner. MainFrame
    // uses this to stop a running preview before it plays back frames
    // that no longer match the current keyframe list.
    void SetAnimationCompositionChangedCallback(std::function<void()> cb) {
        animation_composition_changed_cb_ = std::move(cb);
    }

private:
    struct ResolutionPreset {
        wxString label;
        int width, height;  // width/height == 0 marks the "custom" entry
    };
    struct AspectRatioPreset {
        wxString label;
        int width, height;
    };

    void BuildCropBox(wxSizer* parent_sizer);
    void BuildAnimationBox(wxSizer* parent_sizer);
    void BuildResolutionBox(wxSizer* parent_sizer);
    void BuildColorBox(wxSizer* parent_sizer);
    void BuildColorCorrectionBox(wxSizer* parent_sizer);
    void BuildPaletteBox(wxSizer* parent_sizer);
    void BuildOutputBox(wxSizer* parent_sizer);

    void OnCropEnableToggled(wxCommandEvent&);
    void OnCropUnitChanged(wxCommandEvent&);
    void OnCropSpinChanged(wxSpinDoubleEvent&);
    void OnCropRatioLockToggled(wxCommandEvent&);
    void OnCropRatioPresetChanged(wxCommandEvent&);
    void OnAddKeyframe(wxCommandEvent&);
    // Shared by OnAddKeyframe and the auto-seeded first keyframe when
    // Animation gets enabled with no keyframes yet (see
    // OnAnimationEnableToggled).
    void AddKeyframeFromCurrentCrop();
    void OnRemoveKeyframe(wxCommandEvent&);
    void OnKeyframeSelected(wxCommandEvent&);
    void OnAnimationEnableToggled(wxCommandEvent&);
    void OnPlayPreviewClicked(wxCommandEvent&);
    void OnResolutionPresetChanged(wxCommandEvent&);
    void OnFitModeChanged(wxCommandEvent&);
    void OnPaletteIterationsChanged(wxCommandEvent&);
    // "Quality profile": picking one writes its quantizer, refinement passes and lookahead into the
    // three controls; changing any of those three by hand makes the choice follow (it shows the
    // profile the three add up to, or "Custom").
    void OnProfileChosen(wxCommandEvent&);
    void ApplyQualityProfile(const aic::QualityProfileInfo& profile);
    void SyncProfileChoice();
    void OnQualitySettingChanged(wxCommandEvent&);
    void OnChromaWeightChanged(wxCommandEvent&);
    void OnDitherStrengthChanged(wxCommandEvent&);
    void OnDitherChoiceChanged(wxCommandEvent&);
    // "Dither strength" has nothing to scale for the "none" ditherer.
    void UpdateDitherStrengthEnabled();
    void OnPixelAspectChanged(wxCommandEvent&);
    void OnBrightnessChanged(wxCommandEvent&);
    void OnContrastChanged(wxCommandEvent&);
    void OnGammaChanged(wxCommandEvent&);
    void OnSaturationChanged(wxCommandEvent&);
    void OnResetColorCorrection(wxCommandEvent&);
    void OnLoadPalette(wxCommandEvent&);
    void OnSavePalette(wxCommandEvent&);

    void UpdateCropControlsEnabled();
    void UpdateCropRatioControlsEnabled();
    // Keyframe/animation controls need BOTH crop enabled (there has to
    // be a crop rect to capture) AND the Rendering box's own "Enable
    // Animation" checkbox on.
    void UpdateKeyframeControlsEnabled();
    // "Play preview" while Animation is enabled (plays the keyframe
    // sequence back); "Show HAM preview" while it's off, where the same
    // button instead takes over the single-frame conversion job the
    // standalone Convert button used to do. Called from both
    // UpdateKeyframeControlsEnabled and SetPreviewPlaying so the label
    // stays correct regardless of which one last changed.
    void UpdatePlayPreviewButtonLabel();
    void UpdateCropSpinRanges();
    void UpdateCustomResolutionEnabled();
    // pixel_aspect_ only affects the Fit/Fill resize calculation - the
    // resize code never even reads it for Stretch (see the README's
    // "--pixel-aspect" section for why) - so it's disabled whenever
    // Stretch is selected, re-evaluated on every fit-mode change and
    // once at startup/after LoadDefaults restores a saved selection.
    void UpdatePixelAspectEnabled();
    // See NotifyPaletteGenerated - keeps Lock Palette/Save Palette
    // enabled state and Lock Palette's own value in sync with whether
    // a reference palette currently exists at all.
    void UpdatePaletteControlsEnabled();
    // Shared by NotifyPaletteGenerated and OnLoadPalette: writes a
    // palette's 16 colors into the matching pickers so the current
    // palette is always directly visible/editable there.
    void PopulateColorPickers(const aic::Palette& palette);
    // Recomputes the other crop dimension (in whichever unit - px or %
    // - is currently selected) from the given one plus the locked
    // ratio, converting through pixel space so it stays correct
    // regardless of unit (width% and height% are relative to different
    // source dimensions, so the ratio only makes sense in pixels).
    void ApplyRatioFromWidth();
    void ApplyRatioFromHeight();

    int src_width_ = 0, src_height_ = 0;
    std::vector<ResolutionPreset> presets_;
    std::vector<AspectRatioPreset> ratio_presets_;
    std::function<void()> crop_changed_cb_;
    std::function<void(bool)> animation_toggled_cb_;
    std::function<void()> play_preview_toggled_cb_;
    std::function<void()> animation_composition_changed_cb_;
    std::function<void()> animation_speed_changed_cb_;
    std::function<void()> options_changed_cb_;
    bool crop_last_unit_percent_ = false;

    // Sees every command event of the application (wxEventFilter) and reports the value changes
    // of controls inside this panel to NotifyOptionsChanged - one hook instead of one per control,
    // which also covers controls added later. Programmatic changes (SetValue and friends) send no
    // such events, so the places that set controls themselves call NotifyOptionsChanged directly.
    class ChangeWatcher;
    std::unique_ptr<ChangeWatcher> change_watcher_;
    void NotifyOptionsChanged();
    bool AffectsSinglePreview(const wxWindow* control) const;  // false: animation or export settings
    wxBitmapToggleButton* auto_preview_ = nullptr;

    wxCheckBox* crop_enable_ = nullptr;
    wxSpinCtrlDouble *crop_w_ = nullptr, *crop_h_ = nullptr, *crop_x_ = nullptr, *crop_y_ = nullptr;
    wxRadioButton *crop_unit_px_ = nullptr, *crop_unit_pct_ = nullptr;
    wxCheckBox* crop_ratio_lock_ = nullptr;
    wxChoice* crop_ratio_choice_ = nullptr;

    // Preparation for the interpolated-animation feature: each
    // keyframe is just a captured crop rectangle in source-pixel
    // space, in the order "Add keyframe" was clicked. Interpolating
    // between consecutive entries (and actually generating frames) is
    // deliberately not part of this class yet - see GetCropKeyframes.
    std::vector<wxRect> crop_keyframes_;
    wxChoice* keyframe_choice_ = nullptr;
    wxButton* add_keyframe_btn_ = nullptr;
    wxButton* remove_keyframe_btn_ = nullptr;
    wxCheckBox* animation_enable_ = nullptr;
    wxSpinCtrl* frames_per_segment_ = nullptr;
    wxChoice* anim_speed_ = nullptr;
    wxRadioBox* output_mode_ = nullptr;
    wxButton* play_preview_btn_ = nullptr;
    wxBitmapToggleButton* preview_loop_ = nullptr;
    wxBitmapToggleButton* preview_pingpong_ = nullptr;

    wxChoice* resolution_preset_ = nullptr;
    wxSpinCtrl *custom_width_ = nullptr, *custom_height_ = nullptr;

    wxRadioBox* fit_mode_ = nullptr;
    wxStaticText* pixel_aspect_caption_ = nullptr;
    wxSlider* pixel_aspect_ = nullptr;  // integer 0..200, represents 0.00..2.00
    wxStaticText* pixel_aspect_label_ = nullptr;

    wxChoice* dither_choice_ = nullptr;
    wxChoice* profile_choice_ = nullptr;  // the profiles in aic::quality_profiles() order, then "Custom"
    wxChoice* quantizer_choice_ = nullptr;
    wxChoice* importance_choice_ = nullptr;

    wxSlider* palette_iterations_ = nullptr;
    wxStaticText* palette_iterations_label_ = nullptr;
    wxSlider* chroma_weight_ = nullptr;  // integer 0..200, represents 0.00..2.00
    wxStaticText* chroma_weight_label_ = nullptr;
    wxSlider* dither_strength_ = nullptr;  // integer 0..100 (percent)
    wxStaticText* dither_strength_label_ = nullptr;
    wxCheckBox* line_start_modify_ = nullptr;
    wxChoice* lookahead_choice_ = nullptr;  // selection 0..3 = lookahead depth 1..4

    wxCheckBox* compress_ = nullptr;
    wxRadioBox* lace_mode_ = nullptr;  // Automatic / On / Off

    // All four: integer sliders representing a float range, same
    // pattern as chroma_weight_/pixel_aspect_ above. Applied only to
    // the output (see ColorCorrectionOptions) - never touches the
    // source image shown elsewhere in the GUI.
    wxSlider* brightness_ = nullptr;  // integer -50..50, represents -0.50..0.50
    wxStaticText* brightness_label_ = nullptr;
    wxSlider* contrast_ = nullptr;  // integer 0..200, represents 0.00..2.00
    wxStaticText* contrast_label_ = nullptr;
    wxSlider* gamma_ = nullptr;  // integer 20..300, represents 0.20..3.00
    wxStaticText* gamma_label_ = nullptr;
    wxSlider* saturation_ = nullptr;  // integer 0..200, represents 0.00..2.00
    wxStaticText* saturation_label_ = nullptr;

    // See NotifyPaletteGenerated for how reference_palette_ gets set:
    // either an explicit load, or the result of the most recent
    // conversion, whichever happened most recently.
    std::optional<aic::Palette> reference_palette_;
    wxButton* load_palette_btn_ = nullptr;
    wxButton* save_palette_btn_ = nullptr;
    wxCheckBox* lock_palette_ = nullptr;
    // Only color 0 gets its own lock - it doubles as the Amiga's
    // border/background color, everything else needs no such special
    // case (see GetOptions). All 16 pickers stay editable regardless
    // of any lock state; index 0's picker also feeds crop_fill_color.
    wxCheckBox* lock_color0_ = nullptr;
    std::array<wxColourPickerCtrl*, 16> color_picker_{};
};
