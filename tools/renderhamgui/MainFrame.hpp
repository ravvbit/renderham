// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// MainFrame.hpp
//
// Top-level window: file open/save, source/preview comparison (both as
// equally-sized, independently zoomable ImageViews), the ParameterPanel,
// and a status bar. Conversions, saves and animation renders run on a
// background thread (BackgroundJob) with a progress bar and a Cancel button
// (Esc), so the window stays usable while the encoder works - see the "Background
// jobs" block below for the rules.

#include <wx/wx.h>
#include <wx/filehistory.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "BackgroundJob.hpp"
#include "ImageView.hpp"
#include "ParameterPanel.hpp"
#include "aic/aic.hpp"

class MainFrame : public wxFrame {
public:
    MainFrame();
    explicit MainFrame(const wxString& initial_image_path);

private:
    aic::ImageTransform source_transform_;       // everything the user did to the loaded picture so far
    std::vector<wxButton*> transform_buttons_;   // enabled once an image is loaded
    aic::RGBf alpha_background_{0.f, 0.f, 0.f};  // see Settings: background for transparent images
    wxSizer* toolbar_row_ = nullptr;
    wxSizer* status_row_ = nullptr;
    void Init();
    void LoadImage(const wxString& path);
    void OnOpen(wxCommandEvent&);
    // "Recent" list (the last few images that were opened, most recent first). It lives in the
    // configuration under /RecentFiles and is pruned of files that no longer exist whenever the
    // menu is about to be shown, not at startup (a vanished network share must not delay it).
    void LoadRecentFiles();
    void SaveRecentFiles();
    void RememberFile(const wxString& path);  // moves/puts `path` to the top
    void ForgetFile(const wxString& path);    // drops `path` (it vanished or cannot be read)
    void ShowRecentMenu();                    // popup below the "Recent" button (also Ctrl+Shift+O)
    void OnRecentFile(wxCommandEvent& event);
    void UpdateInputLabel();                  // file name, size and full path (as tooltip) of the loaded picture
    void OnAbout(wxCommandEvent&);
    void OnSaveAs(wxCommandEvent&);
    void OnTargetDirectory(wxCommandEvent&);
    void OnSaveQuick(wxCommandEvent&);
    void OnSettings(wxCommandEvent&);
    void OnSaveAsDefaults(wxCommandEvent&);
    void OnSize(wxSizeEvent& event);
    void OnCloseWindow(wxCloseEvent& event);
    // Gives the scrollable parameter panel as much height as it needs, but
    // no more than what is left after the toolbar, a usable pair of preview
    // panes and the status row - so the window fits on small screens.
    void UpdateParamsPanelHeight();
    void SaveWindowGeometry();

    // params_->GetOptions() plus everything the frame itself owns (how the
    // source is loaded). Every conversion goes through this.
    aic::ConversionOptions MakeOptions() const;
    wxColour AlphaBackgroundColour() const;

    // Rotates/mirrors the source image by `delta` (relative to what is shown).
    void ApplySourceTransform(const aic::ImageTransform& delta, const wxString& status_text);

    // Single-image conversion for the preview (F5 / "Show HAM preview"). Runs in
    // the background; the result lands in last_preview_ when it is done.
    void RunConversion();
    // Single-image save (Ctrl+S / Ctrl+Shift+S), also in the background.
    void SaveSingle(const wxString& output_path);
    void UpdatePreviewDisplay();  // (re)renders last_preview_ into preview_view_, applying interlace scaling if requested
    // Linearly interpolates params_->GetCropKeyframes() (>= 2 required)
    // across params_->GetFramesPerSegment() frames per segment - shared
    // by RunAnimationExport (writes to disk) and StartAnimationPreview
    // (stays in memory) so both always agree on exactly which frames an
    // animation consists of.
    std::vector<wxRect> BuildInterpolatedFrameRects() const;
    // Interpolates params_->GetCropKeyframes() (>= 2 required) across
    // params_->GetFramesPerSegment() frames per segment, writes a
    // numbered IFF sequence ("<stem>_0001.iff", ...) next to base_path,
    // and updates the Amiga Preview live as each frame completes. All
    // frames after the first share the first frame's actual resulting
    // palette (whether that came from a pre-existing Lock Palette or a
    // fresh quantization - either way, reusing frame 0's result for
    // every later frame is what keeps the whole sequence on one
    // consistent palette). Called by OnSaveAs/OnSaveQuick instead of a
    // normal single-file save whenever animation is enabled.
    void RunAnimationExport(const wxString& base_path);
    // Starts the Play preview: renders the same interpolated, single-palette
    // frame sequence as RunAnimationExport, but keeps every frame's fully decoded
    // result in memory (preview_frames_) instead of writing anything to disk. The
    // rendering is a background job; when it has finished, playback starts
    // (OnPreviewTimer). Shows a message box on the same failure conditions
    // RunAnimationExport would hit.
    void StartAnimationPreview();
    // Stops playback, and cancels the rendering of the preview frames if that is
    // still going on. Safe to call at any time.
    void StopAnimationPreview();
    void OnPlayPreviewToggled();  // ParameterPanel's Play/Stop button was clicked
    void OnPreviewTimer(wxTimerEvent&);
    // Keeps the "Save"/"Save As..." toolbar buttons honest about what
    // clicking them will actually do: with Animation enabled, both
    // buttons trigger a full multi-frame export instead of a
    // single-image save (see RunAnimationExport) - relabeling them
    // makes that switch visible at the point of action, rather than
    // only discoverable by having noticed a checkbox in a side panel.
    void UpdateSaveButtonLabels(bool animation_enabled);
    void SyncCropOverlayFromParams();  // spinners/checkbox -> rubber-band
    void OnCropOverlayEdited(wxRect rect_px);  // rubber-band drag -> spinners
    void SetStatus(const wxString& text);
    void UpdateTargetDirTooltip();  // reflects target_dir_ (or its absence) on the button

    // Builds the "<title>  [-][Passend][100%][+]  <zoom%>" row shared by
    // both preview columns, wired to the given view, so source and
    // result end up with pixel-identical layouts (and therefore equal
    // ImageView sizes - the whole point of sharing this).
    wxSizer* BuildPreviewHeader(const wxString& title, ImageView* view, wxStaticText** out_zoom_label);

    wxString input_path_;
    wxString target_dir_;  // persisted immediately on change, like the language setting
    std::optional<aic::Image> source_image_;
    std::optional<aic::PreviewResult> last_preview_;

    wxStaticText* input_label_ = nullptr;
    wxButton* recent_btn_ = nullptr;
    std::unique_ptr<wxFileHistory> recent_files_;
    wxButton* target_dir_btn_ = nullptr;
    wxButton* save_quick_btn_ = nullptr;
    wxButton* save_as_btn_ = nullptr;
    ImageView* source_view_ = nullptr;
    ImageView* preview_view_ = nullptr;
    wxStaticText* preview_zoom_label_ = nullptr;
    wxCheckBox* interlace_scale_check_ = nullptr;
    wxChoice* preview_mode_ = nullptr;  // Preview / HAM map / Error map
    wxStaticText* status_label_ = nullptr;
    ParameterPanel* params_ = nullptr;

    aic::ConversionPipeline pipeline_;

    // --- Background jobs ---------------------------------------------------
    // One job at a time. What a new request does while one is running:
    //   - a preview (single image or animation preview) is replaced: the newest
    //     request wins, the old one is cancelled (F5 again after changing a slider);
    //   - a save or an export is never interrupted by a request: it is refused with
    //     a note in the status line, and only the Cancel button / Esc stops it
    //     (closing the window asks first). That includes opening another image,
    //     which would silently change what the running export reads.
    // The worker thread touches no wx object: it gets copies of everything it needs and
    // reports back through BackgroundJob::Context::post.
    enum class JobKind { None, Preview, Save, AnimationExport, AnimationPreview };
    static bool IsExportJob(JobKind kind) { return kind == JobKind::Save || kind == JobKind::AnimationExport; }
    // Returns false (with a note in the status line) if an export is running;
    // otherwise cancels a running preview, shows the progress bar and returns true.
    bool BeginJob(JobKind kind, const wxString& status_text);
    // Runs `work` on the worker thread; `done` runs on the GUI thread afterwards (after the
    // progress bar was taken down again). Only after a successful BeginJob().
    void StartJob(BackgroundJob::Work work, std::function<void()> done);
    void EndJobUi();                  // hides progress bar and Cancel button, forgets job_kind_
    void CancelRunningJob();          // Cancel button / Esc: asks the worker to stop
    bool RefuseWhileExporting();      // status note + true if an export is running
    // Before the source picture changes (open, rotate, mirror): refuses while exporting,
    // otherwise cancels any running preview and stops playback. False = do not go on.
    bool PrepareForSourceChange();
    void OnJobTimer(wxTimerEvent&);   // moves job_progress_ to the progress bar

    BackgroundJob job_{this};
    JobKind job_kind_ = JobKind::None;
    std::atomic<float> job_progress_{0.f};  // 0..1, written by the worker, read by job_timer_
    wxGauge* job_gauge_ = nullptr;
    wxButton* job_cancel_btn_ = nullptr;
    wxTimer job_timer_;

    // Real-time animation preview (see StartAnimationPreview/
    // OnPreviewTimer): every frame the current keyframe sequence
    // interpolates to, fully decoded and kept in memory - nothing here
    // is ever written to disk. preview_direction_ is +1 or -1 and only
    // matters while IsPreviewPingPongEnabled() is on; it flips at
    // either end of preview_frames_ instead of wrapping back to 0.
    std::vector<aic::PreviewResult> preview_frames_;
    wxTimer preview_timer_;
    size_t preview_frame_index_ = 0;
    int preview_direction_ = 1;
    bool preview_playing_ = false;

    // Re-entrancy guard for the bidirectional crop sync: SetCropPixelRect
    // (called from OnCropOverlayEdited) updates spinners without firing
    // ParameterPanel's own change callback, so this shouldn't be needed
    // in practice, but it costs nothing to be explicit and safe here too.
    bool syncing_crop_ = false;
};
