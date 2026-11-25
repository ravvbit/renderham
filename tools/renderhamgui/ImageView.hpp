// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// ImageView.hpp
//
// A wxScrolledWindow-based image display that:
//  - auto-fits its content to the current panel size whenever the panel
//    is resized (as long as the user hasn't manually zoomed), so simply
//    growing the main window grows every ImageView inside it;
//  - optionally supports manual zooming (mouse wheel + Ctrl, or the
//    buttons a caller wires up) all the way to per-pixel level, with
//    mouse-drag panning and scrollbars once the content no longer fits;
//  - always renders with nearest-neighbor scaling, so a HAM6 preview's
//    hard pixel edges are shown honestly instead of implying an
//    anti-aliased blur that won't exist on real hardware;
//  - optionally shows an interactively editable crop rubber-band
//    (drag the interior to move, drag an edge/corner handle to resize),
//    independent of whether zooming is enabled;
//  - describes what it shows as an image-space View (zoom plus the image
//    point in the middle of the window), which can be read, set and watched,
//    so that a second ImageView can be made to show the same part of the
//    picture (see MainFrame's "link zoom and pan").

#include <wx/wx.h>
#include <wx/scrolwin.h>

#include <functional>
#include <optional>

class ImageView : public wxScrolledWindow {
public:
    explicit ImageView(wxWindow* parent);

    // Replaces the picture. Starts in fit mode, unless keep_view is set and the new picture has
    // the size of the old one: then zoom and position stay, so a new conversion result or another
    // map of the same frame appears exactly where the last one was looked at.
    void SetImage(wxImage image, bool keep_view = false);
    bool HasImage() const { return image_.IsOk(); }
    void SetZoomEnabled(bool enabled) { zoom_enabled_ = enabled; }

    void ZoomIn();
    void ZoomOut();
    void ZoomToFit();   // resume auto-fit-to-window behavior
    void ZoomActual();  // 100%: one image pixel = one screen pixel

    // Current zoom as a percentage, valid even in fit mode (reflects
    // whatever scale fit mode is currently rendering at).
    int GetZoomPercent() const { return static_cast<int>(effective_zoom_ * 100.0 + 0.5); }
    bool IsFitMode() const { return fit_mode_; }

    // What the window shows, in image terms. zoom = screen pixels per image pixel along x (the
    // vertical squash only changes y); (center_x, center_y) = the point of the picture in the
    // middle of the window, in continuous image coordinates (pixel edges at integers). In fit mode
    // zoom and center describe what fit mode currently renders and `fit` is set.
    struct View {
        bool fit = true;
        double zoom = 1.0;
        double center_x = 0.0, center_y = 0.0;
    };
    View GetView() const;
    // Shows `view`: fit mode if it says so, else that zoom (limited to the supported range) around
    // that center. Does not invoke the view-changed callback - like SetCropOverlay, that one is
    // only for what the user does, which is what keeps two linked views from echoing each other.
    void SetView(const View& view);

    // Invoked after every change of zoom or position: by_user = true when the user zoomed or
    // panned (buttons, wheel, drag, scrollbars), false when the window itself changed what it shows
    // (new picture, resize in fit mode).
    void SetViewChangedCallback(std::function<void(bool by_user)> cb) { on_view_changed_ = std::move(cb); }

    // Crop rubber-band overlay, in IMAGE pixel coordinates. nullopt
    // hides it. Setting it programmatically (e.g. because the crop
    // spinners changed) does NOT invoke on_crop_edited - that callback
    // is only for edits the user makes by dragging the overlay itself,
    // which is what avoids a feedback loop with the caller.
    void SetCropOverlay(std::optional<wxRect> rect_image_space);
    void SetCropEditedCallback(std::function<void(wxRect)> cb) { on_crop_edited_ = std::move(cb); }

    // When enabled, dragging any resize handle on the crop overlay
    // preserves this width/height ratio instead of resizing freely.
    // Purely a rendering/interaction concern here - ParameterPanel owns
    // the actual locked-or-not state and which preset is selected; the
    // caller (MainFrame) just mirrors that over whenever it changes.
    void SetCropAspectRatioLock(bool enabled, double ratio) {
        crop_ratio_locked_ = enabled;
        crop_ratio_ = ratio;
    }

    // Anisotropic display-only compression of the vertical axis - 1.0
    // (default) renders normally, 0.5 renders every row at half its
    // normal height while the horizontal axis is untouched. Used to
    // simulate an interlaced Amiga screen mode, where a scanline is
    // physically half as tall as a non-interlaced one but no narrower:
    // real hardware doesn't discard any of the extra vertical
    // resolution, it just fits twice as many scanlines into the same
    // physical height, and at any zoom level a single simulated pixel
    // should look twice as wide as it is tall. This scales the DISPLAY
    // only, never the underlying image_ data, so every actual decoded
    // pixel stays inspectable when zooming in - unlike an approach
    // that pre-shrinks the image by discarding rows, which would both
    // throw away real data and still render square pixels once zoomed.
    void SetVerticalSquash(double factor);

private:
    enum class Handle { None, Inside, N, S, E, W, NE, NW, SE, SW };

    void OnPaint(wxPaintEvent&);
    void OnSize(wxSizeEvent&);
    void OnMouseWheel(wxMouseEvent&);
    void OnMouseDown(wxMouseEvent&);
    void OnMouseMove(wxMouseEvent&);
    void OnMouseUp(wxMouseEvent&);
    void OnCaptureLost(wxMouseCaptureLostEvent&);

    void OnScrollLine(wxScrollWinEvent&);
    void OnScrollOther(wxScrollWinEvent&);

    // Zooms to new_zoom (manual mode) keeping the picture point under client_anchor (a position in
    // the window) where it is.
    void ZoomAround(double new_zoom, wxPoint client_anchor);
    void ApplyZoom(double new_zoom) { ZoomAround(new_zoom, wxPoint(GetClientSize().x / 2, GetClientSize().y / 2)); }
    // Scrolls so that image point (ix, iy) is at client_pt. Manual mode, after RebuildScaledBitmap.
    void PlaceImagePoint(double ix, double iy, wxPoint client_pt);
    void PanBy(int dx, int dy);  // screen pixels, as far as there is content
    void NotifyViewChanged(bool by_user);
    void RebuildScaledBitmap();  // recomputes scaled_bitmap_ for the current effective_zoom_

    wxPoint ImageToLogical(wxPoint image_pt) const;
    wxPoint LogicalToImage(wxPoint logical_pt) const;
    wxRect ImageRectToLogical(const wxRect& r) const;
    wxRect ClampRectToImage(wxRect r) const;
    Handle HitTestOverlay(wxPoint logical_pt) const;
    wxCursor CursorForHandle(Handle h) const;
    // Ratio-preserving equivalent of the free-form per-handle resize
    // logic in OnMouseMove: anchors the corner opposite the dragged
    // handle, derives the "other" dimension from whichever axis the
    // handle's own drag direction drives (width for E/W, height for
    // N/S, whichever needs the bigger box for corners), then shrinks
    // both dimensions together (never independently - that would
    // change the ratio) if that would exceed the image bounds.
    wxRect ResizeWithLockedRatio(Handle handle, const wxRect& start, wxPoint img_pt) const;
    // The non-ratio-locked case: each handle adjusts only the edge(s)
    // it owns (SetTop/SetLeft/etc. move that edge while keeping the
    // opposite edge fixed, which is what makes this a resize and not
    // a move) - separated out from OnMouseMove for direct testability.
    wxRect ResizeFreeform(Handle handle, const wxRect& start, int dx, int dy) const;

    wxImage image_;
    wxBitmap scaled_bitmap_;
    int bitmap_offset_x_ = 0, bitmap_offset_y_ = 0;  // centering offset within the virtual area

    bool fit_mode_ = true;
    bool zoom_enabled_ = false;
    double manual_zoom_ = 1.0;     // used when fit_mode_ is false
    double effective_zoom_ = 1.0;  // whatever was actually rendered last (fit-computed or manual)
    double vertical_squash_ = 1.0;  // see SetVerticalSquash
    std::function<void(bool)> on_view_changed_;
    bool setting_view_ = false;  // inside SetView: whatever the window reports meanwhile is not the user's doing

    bool dragging_pan_ = false;
    wxPoint drag_start_mouse_;
    wxPoint drag_start_scroll_;  // in scroll units

    std::optional<wxRect> crop_overlay_;  // image-space
    std::function<void(wxRect)> on_crop_edited_;
    Handle dragging_handle_ = Handle::None;
    wxRect drag_start_overlay_;
    wxPoint drag_start_image_pt_;
    bool crop_ratio_locked_ = false;
    double crop_ratio_ = 1.0;  // width / height

    static constexpr double kMinZoom = 0.05;
    static constexpr double kMaxZoom = 32.0;
    static constexpr double kZoomStep = 1.5;
    static constexpr int kHandleTolerance = 7;  // logical/screen pixels
    // The scroll unit is one screen pixel, so a view can be put exactly where another one needs it
    // (a coarser unit would leave two linked views up to half a unit apart); a wheel notch, an
    // arrow click and the like move by these many pixels instead.
    static constexpr int kLineStep = 10;
};
