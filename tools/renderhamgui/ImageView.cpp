// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "ImageView.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

ImageView::ImageView(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN | wxVSCROLL | wxHSCROLL) {
    SetBackgroundColour(wxColour(210, 210, 210));
    SetScrollRate(1, 1);  // see kLineStep
    Bind(wxEVT_PAINT, &ImageView::OnPaint, this);
    Bind(wxEVT_SIZE, &ImageView::OnSize, this);
    Bind(wxEVT_MOUSEWHEEL, &ImageView::OnMouseWheel, this);
    Bind(wxEVT_LEFT_DOWN, &ImageView::OnMouseDown, this);
    Bind(wxEVT_MOTION, &ImageView::OnMouseMove, this);
    Bind(wxEVT_LEFT_UP, &ImageView::OnMouseUp, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &ImageView::OnCaptureLost, this);
    // With a one-pixel unit the stock "one line" and "one page" steps would be one pixel; the arrow
    // buttons of the scrollbars step by kLineStep instead. Everything else (thumb, page clicks)
    // is left to wxWidgets and only reported.
    Bind(wxEVT_SCROLLWIN_LINEUP, &ImageView::OnScrollLine, this);
    Bind(wxEVT_SCROLLWIN_LINEDOWN, &ImageView::OnScrollLine, this);
    for (const auto& type : {wxEVT_SCROLLWIN_PAGEUP, wxEVT_SCROLLWIN_PAGEDOWN, wxEVT_SCROLLWIN_THUMBTRACK,
                             wxEVT_SCROLLWIN_THUMBRELEASE, wxEVT_SCROLLWIN_TOP, wxEVT_SCROLLWIN_BOTTOM})
        Bind(type, &ImageView::OnScrollOther, this);
}

void ImageView::SetImage(wxImage image, bool keep_view) {
    const bool same_size = keep_view && image_.IsOk() && image.IsOk() && image.GetWidth() == image_.GetWidth() &&
                           image.GetHeight() == image_.GetHeight();
    const View before = GetView();
    image_ = std::move(image);
    if (!same_size) fit_mode_ = true;
    crop_overlay_.reset();
    RebuildScaledBitmap();
    if (same_size && !fit_mode_) {
        setting_view_ = true;
        PlaceImagePoint(before.center_x, before.center_y, wxPoint(GetClientSize().x / 2, GetClientSize().y / 2));
        setting_view_ = false;
    }
    Refresh();
    NotifyViewChanged(false);
}

void ImageView::SetVerticalSquash(double factor) {
    factor = factor > 0.0 ? factor : 1.0;
    if (factor == vertical_squash_) return;
    // The squash changes the height of the picture on screen; the point in the middle stays there.
    const View before = GetView();
    vertical_squash_ = factor;
    RebuildScaledBitmap();
    if (!fit_mode_ && image_.IsOk()) {
        setting_view_ = true;
        PlaceImagePoint(before.center_x, before.center_y, wxPoint(GetClientSize().x / 2, GetClientSize().y / 2));
        setting_view_ = false;
    }
    Refresh();
    NotifyViewChanged(false);
}

ImageView::View ImageView::GetView() const {
    View v;
    v.fit = fit_mode_;
    v.zoom = effective_zoom_;
    if (image_.IsOk() && effective_zoom_ > 0.0) {
        const wxSize client = GetClientSize();
        const wxPoint logical = CalcUnscrolledPosition(wxPoint(client.GetWidth() / 2, client.GetHeight() / 2));
        v.center_x = (logical.x - bitmap_offset_x_) / effective_zoom_;
        v.center_y = (logical.y - bitmap_offset_y_) / (effective_zoom_ * vertical_squash_);
    }
    return v;
}

void ImageView::SetView(const View& view) {
    if (!image_.IsOk()) return;
    setting_view_ = true;
    if (view.fit) {
        fit_mode_ = true;
        RebuildScaledBitmap();
    } else {
        fit_mode_ = false;
        manual_zoom_ = std::clamp(view.zoom, kMinZoom, kMaxZoom);
        RebuildScaledBitmap();
        PlaceImagePoint(view.center_x, view.center_y, wxPoint(GetClientSize().x / 2, GetClientSize().y / 2));
    }
    setting_view_ = false;
    Refresh();
}

void ImageView::PlaceImagePoint(double ix, double iy, wxPoint client_pt) {
    // Scroll() takes a position in scroll units (one pixel, see the constructor) and leaves an
    // axis alone for a negative value; the ranges have to be current for it to clamp properly.
    AdjustScrollbars();
    const double lx = bitmap_offset_x_ + ix * effective_zoom_;
    const double ly = bitmap_offset_y_ + iy * effective_zoom_ * vertical_squash_;
    const int x = std::max(0, static_cast<int>(std::lround(lx - client_pt.x)));
    const int y = std::max(0, static_cast<int>(std::lround(ly - client_pt.y)));
    Scroll(x, y);
}

void ImageView::PanBy(int dx, int dy) {
    if (!image_.IsOk() || (dx == 0 && dy == 0)) return;
    const wxPoint start = GetViewStart();
    Scroll(std::max(0, start.x + dx), std::max(0, start.y + dy));
    NotifyViewChanged(true);
}

void ImageView::NotifyViewChanged(bool by_user) {
    if (setting_view_ || !on_view_changed_) return;
    on_view_changed_(by_user);
}

void ImageView::OnScrollLine(wxScrollWinEvent& event) {
    if (setting_view_) return;
    const int step = event.GetEventType() == wxEVT_SCROLLWIN_LINEUP ? -kLineStep : kLineStep;
    if (event.GetOrientation() == wxHORIZONTAL) {
        PanBy(step, 0);
    } else {
        PanBy(0, step);
    }
}

void ImageView::OnScrollOther(wxScrollWinEvent& event) {
    event.Skip();  // wxWidgets moves the content after this handler; report once it has
    if (setting_view_) return;
    CallAfter([this] { NotifyViewChanged(true); });
}

void ImageView::RebuildScaledBitmap() {
    if (!image_.IsOk() || image_.GetWidth() <= 0 || image_.GetHeight() <= 0) {
        scaled_bitmap_ = wxBitmap();
        SetVirtualSize(GetClientSize());
        return;
    }

    wxSize client = GetClientSize();
    double zoom;
    if (fit_mode_) {
        double sx = client.GetWidth() > 0 ? static_cast<double>(client.GetWidth()) / image_.GetWidth() : 1.0;
        // The squashed height is what actually needs to fit vertically.
        double effective_h = image_.GetHeight() * vertical_squash_;
        double sy = client.GetHeight() > 0 ? static_cast<double>(client.GetHeight()) / effective_h : 1.0;
        zoom = std::min(sx, sy);
        if (!std::isfinite(zoom) || zoom <= 0.0) zoom = 1.0;
        zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    } else {
        zoom = manual_zoom_;
    }
    effective_zoom_ = zoom;

    int nw = std::max(1, static_cast<int>(image_.GetWidth() * zoom));
    int nh = std::max(1, static_cast<int>(image_.GetHeight() * zoom * vertical_squash_));
    // Deliberately NOT wxImage::Scale(), not even at
    // wxIMAGE_QUALITY_NEAREST: empirically verified (a synthetic
    // alternating-red/blue-rows test image, see git history) that wx's
    // "nearest" quality still blends when the source/target ratio
    // works out to roughly 2:1 - every output row came out as blended
    // purple, not pure red or blue. That silently contradicts this
    // class's whole stated purpose (nearest-neighbor, honest blocky
    // pixels, no implied anti-aliasing) any time fit-mode or the
    // interlace vertical squash lands on that kind of ratio. A manual
    // per-pixel nearest sample - each output pixel copies exactly one
    // source pixel's exact bytes - guarantees the real thing regardless
    // of scale factor.
    int src_w = image_.GetWidth(), src_h = image_.GetHeight();
    wxImage scaled(nw, nh, false);
    unsigned char* dst = scaled.GetData();
    const unsigned char* src = image_.GetData();
    for (int y = 0; y < nh; ++y) {
        int sy = std::min(src_h - 1, static_cast<int>(static_cast<double>(y) * src_h / nh));
        const unsigned char* src_row = src + static_cast<size_t>(sy) * src_w * 3;
        unsigned char* dst_row = dst + static_cast<size_t>(y) * nw * 3;
        for (int x = 0; x < nw; ++x) {
            int sx = std::min(src_w - 1, static_cast<int>(static_cast<double>(x) * src_w / nw));
            std::memcpy(dst_row + static_cast<size_t>(x) * 3, src_row + static_cast<size_t>(sx) * 3, 3);
        }
    }
    scaled_bitmap_ = wxBitmap(scaled);

    int vw = std::max(nw, client.GetWidth());
    int vh = std::max(nh, client.GetHeight());
    SetVirtualSize(vw, vh);
    bitmap_offset_x_ = std::max(0, (vw - nw) / 2);
    bitmap_offset_y_ = std::max(0, (vh - nh) / 2);
}

wxPoint ImageView::ImageToLogical(wxPoint image_pt) const {
    return wxPoint(bitmap_offset_x_ + static_cast<int>(std::lround(image_pt.x * effective_zoom_)),
                    bitmap_offset_y_ + static_cast<int>(std::lround(image_pt.y * effective_zoom_ * vertical_squash_)));
}

wxPoint ImageView::LogicalToImage(wxPoint logical_pt) const {
    if (effective_zoom_ <= 0.0) return wxPoint(0, 0);
    return wxPoint(
        static_cast<int>(std::lround((logical_pt.x - bitmap_offset_x_) / effective_zoom_)),
        static_cast<int>(std::lround((logical_pt.y - bitmap_offset_y_) / (effective_zoom_ * vertical_squash_))));
}

wxRect ImageView::ImageRectToLogical(const wxRect& r) const {
    wxPoint tl = ImageToLogical(wxPoint(r.GetLeft(), r.GetTop()));
    wxPoint br = ImageToLogical(wxPoint(r.GetRight() + 1, r.GetBottom() + 1));
    return wxRect(tl, br);
}

wxRect ImageView::ClampRectToImage(wxRect r) const {
    // Deliberately no longer clamped to stay within [0,image
    // dimensions] - the crop box needs to be able to extend past any
    // edge (or lie entirely outside the source image) for an
    // animation keyframe sequence to "wander into" or "wander out of"
    // frame. Anything with no corresponding source pixel gets filled
    // with the configured background color - see crop_image's
    // fill_color and ParameterPanel::GetOptions' crop_fill_color.
    const int min_size = 2;
    r.width = std::max(r.width, min_size);
    r.height = std::max(r.height, min_size);
    return r;
}

wxRect ImageView::ResizeFreeform(Handle handle, const wxRect& start, int dx, int dy) const {
    wxRect r = start;
    // wxRect::SetTop()/SetLeft() only reassign y/x directly - unlike
    // SetBottom()/SetRight(), they do NOT adjust height/width to
    // compensate (bottom/right aren't stored fields, so setting them
    // has to derive a new height/width from the target value and the
    // fixed origin; top/left ARE the stored origin, so setting them
    // has nothing else to derive). Calling SetTop() alone therefore
    // just translates the rect vertically - the bottom edge silently
    // moves right along with it - which is a move, not a resize.
    // Capturing the opposite edge before moving the near one, then
    // restoring it via SetBottom()/SetRight() afterward (which DOES
    // recompute the size, now against the new origin), is what
    // actually keeps that opposite edge fixed in place.
    switch (handle) {
        case Handle::N: {
            int bottom = r.GetBottom();
            r.SetTop(r.GetTop() + dy);
            r.SetBottom(bottom);
            break;
        }
        case Handle::S:
            r.SetBottom(r.GetBottom() + dy);
            break;
        case Handle::E:
            r.SetRight(r.GetRight() + dx);
            break;
        case Handle::W: {
            int right = r.GetRight();
            r.SetLeft(r.GetLeft() + dx);
            r.SetRight(right);
            break;
        }
        case Handle::NE: {
            int bottom = r.GetBottom();
            r.SetTop(r.GetTop() + dy);
            r.SetBottom(bottom);
            r.SetRight(r.GetRight() + dx);
            break;
        }
        case Handle::NW: {
            int bottom = r.GetBottom();
            int right = r.GetRight();
            r.SetTop(r.GetTop() + dy);
            r.SetBottom(bottom);
            r.SetLeft(r.GetLeft() + dx);
            r.SetRight(right);
            break;
        }
        case Handle::SE:
            r.SetBottom(r.GetBottom() + dy);
            r.SetRight(r.GetRight() + dx);
            break;
        case Handle::SW: {
            int right = r.GetRight();
            r.SetBottom(r.GetBottom() + dy);
            r.SetLeft(r.GetLeft() + dx);
            r.SetRight(right);
            break;
        }
        default:
            break;
    }
    return r;
}

wxRect ImageView::ResizeWithLockedRatio(Handle handle, const wxRect& start, wxPoint img_pt) const {
    // Anchor = the corner that stays fixed while the dragged handle
    // moves. For edge handles, the dimension THEY don't directly
    // control (e.g. width, for a N/S drag) is anchored at the left/top
    // by convention, so the ratio-derived size always grows the same
    // direction regardless of which edge is being dragged.
    int ax, ay;
    bool anchor_left, anchor_top;
    switch (handle) {
        case Handle::NW: ax = start.GetRight() + 1; ay = start.GetBottom() + 1; anchor_left = false; anchor_top = false; break;
        case Handle::NE: ax = start.GetLeft();       ay = start.GetBottom() + 1; anchor_left = true;  anchor_top = false; break;
        case Handle::SW: ax = start.GetRight() + 1; ay = start.GetTop();        anchor_left = false; anchor_top = true;  break;
        case Handle::SE: ax = start.GetLeft();       ay = start.GetTop();        anchor_left = true;  anchor_top = true;  break;
        case Handle::N:  ax = start.GetLeft();       ay = start.GetBottom() + 1; anchor_left = true;  anchor_top = false; break;
        case Handle::S:  ax = start.GetLeft();       ay = start.GetTop();        anchor_left = true;  anchor_top = true;  break;
        case Handle::W:  ax = start.GetRight() + 1; ay = start.GetTop();        anchor_left = false; anchor_top = true;  break;
        case Handle::E:  ax = start.GetLeft();       ay = start.GetTop();        anchor_left = true;  anchor_top = true;  break;
        default: return start;
    }

    double raw_w = std::abs(img_pt.x - ax);
    double raw_h = std::abs(img_pt.y - ay);
    double new_w, new_h;
    switch (handle) {
        case Handle::N:
        case Handle::S:
            new_h = std::max(2.0, raw_h);
            new_w = new_h * crop_ratio_;
            break;
        case Handle::E:
        case Handle::W:
            new_w = std::max(2.0, raw_w);
            new_h = new_w / crop_ratio_;
            break;
        default:  // corners: drive from whichever axis wants the bigger box
            new_w = std::max(raw_w, raw_h * crop_ratio_);
            new_h = new_w / crop_ratio_;
            break;
    }
    new_w = std::max(2.0, new_w);
    new_h = std::max(2.0, new_h);

    // Deliberately no longer clamped against the image bounds here -
    // see ClampRectToImage for why (the crop box needs to be able to
    // extend past any edge for the "wander into/out of frame"
    // animation effect). This used to shrink new_w/new_h proportionally
    // (keeping the ratio) whenever the anchor-relative size would have
    // exceeded the image dimensions; that whole step is gone now.

    int rx = anchor_left ? ax : static_cast<int>(std::lround(ax - new_w));
    int ry = anchor_top ? ay : static_cast<int>(std::lround(ay - new_h));
    return wxRect(rx, ry, static_cast<int>(std::lround(new_w)), static_cast<int>(std::lround(new_h)));
}

ImageView::Handle ImageView::HitTestOverlay(wxPoint logical_pt) const {
    if (!crop_overlay_) return Handle::None;
    wxRect r = ImageRectToLogical(*crop_overlay_);
    const int t = kHandleTolerance;

    bool near_left = std::abs(logical_pt.x - r.GetLeft()) <= t;
    bool near_right = std::abs(logical_pt.x - r.GetRight()) <= t;
    bool near_top = std::abs(logical_pt.y - r.GetTop()) <= t;
    bool near_bottom = std::abs(logical_pt.y - r.GetBottom()) <= t;
    bool within_x = logical_pt.x >= r.GetLeft() - t && logical_pt.x <= r.GetRight() + t;
    bool within_y = logical_pt.y >= r.GetTop() - t && logical_pt.y <= r.GetBottom() + t;

    if (near_left && near_top) return Handle::NW;
    if (near_right && near_top) return Handle::NE;
    if (near_left && near_bottom) return Handle::SW;
    if (near_right && near_bottom) return Handle::SE;
    if (near_top && within_x) return Handle::N;
    if (near_bottom && within_x) return Handle::S;
    if (near_left && within_y) return Handle::W;
    if (near_right && within_y) return Handle::E;
    if (r.Contains(logical_pt)) return Handle::Inside;
    return Handle::None;
}

wxCursor ImageView::CursorForHandle(Handle h) const {
    switch (h) {
        case Handle::Inside: return wxCursor(wxCURSOR_SIZING);
        case Handle::N:
        case Handle::S: return wxCursor(wxCURSOR_SIZENS);
        case Handle::E:
        case Handle::W: return wxCursor(wxCURSOR_SIZEWE);
        case Handle::NE:
        case Handle::SW: return wxCursor(wxCURSOR_SIZENESW);
        case Handle::NW:
        case Handle::SE: return wxCursor(wxCURSOR_SIZENWSE);
        default: return wxNullCursor;
    }
}

void ImageView::SetCropOverlay(std::optional<wxRect> rect_image_space) {
    crop_overlay_ = rect_image_space;
    Refresh();
}

void ImageView::OnPaint(wxPaintEvent&) {
    wxPaintDC dc(this);
    DoPrepareDC(dc);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    if (scaled_bitmap_.IsOk()) {
        wxSize virt = GetVirtualSize();
        int x = std::max(0, (virt.GetWidth() - scaled_bitmap_.GetWidth()) / 2);
        int y = std::max(0, (virt.GetHeight() - scaled_bitmap_.GetHeight()) / 2);
        dc.DrawBitmap(scaled_bitmap_, x, y, false);
    }

    if (crop_overlay_) {
        wxRect r = ImageRectToLogical(*crop_overlay_);
        dc.SetPen(wxPen(*wxYELLOW, 2));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(r);

        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxYELLOW_BRUSH);
        const int hs = 4;
        auto handle = [&](int hx, int hy) { dc.DrawRectangle(hx - hs, hy - hs, hs * 2, hs * 2); };
        handle(r.GetLeft(), r.GetTop());
        handle(r.GetRight(), r.GetTop());
        handle(r.GetLeft(), r.GetBottom());
        handle(r.GetRight(), r.GetBottom());
        handle((r.GetLeft() + r.GetRight()) / 2, r.GetTop());
        handle((r.GetLeft() + r.GetRight()) / 2, r.GetBottom());
        handle(r.GetLeft(), (r.GetTop() + r.GetBottom()) / 2);
        handle(r.GetRight(), (r.GetTop() + r.GetBottom()) / 2);
    }
}

void ImageView::OnSize(wxSizeEvent& event) {
    RebuildScaledBitmap();
    Refresh();
    event.Skip();
    // Fit mode re-fits to the new size, which is a new zoom (the labels show it); a zoomed view
    // keeps its zoom, and nobody needs to hear about it.
    if (fit_mode_) NotifyViewChanged(false);
}

void ImageView::OnMouseWheel(wxMouseEvent& event) {
    if (zoom_enabled_ && event.ControlDown()) {
        // Zooms around the mouse pointer: what is under it stays under it.
        const wxPoint anchor = event.GetPosition();
        const double factor = event.GetWheelRotation() > 0 ? kZoomStep : 1.0 / kZoomStep;
        ZoomAround((fit_mode_ ? effective_zoom_ : manual_zoom_) * factor, anchor);
        return;
    }
    if (!zoom_enabled_ || !image_.IsOk()) {
        event.Skip();
        return;
    }
    // Scrolls by kLineStep pixels per line, like it did when that was the scroll unit; shift (or a
    // sideways wheel) scrolls sideways.
    const double notches = static_cast<double>(event.GetWheelRotation()) / std::max(1, event.GetWheelDelta());
    const int pixels = static_cast<int>(std::lround(notches * std::max(1, event.GetLinesPerAction()) * kLineStep));
    if (event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL) {
        PanBy(pixels, 0);
    } else if (event.ShiftDown()) {
        PanBy(-pixels, 0);
    } else {
        PanBy(0, -pixels);
    }
}

void ImageView::OnMouseDown(wxMouseEvent& event) {
    wxPoint logical = CalcUnscrolledPosition(event.GetPosition());
    Handle hit = HitTestOverlay(logical);
    if (hit != Handle::None) {
        dragging_handle_ = hit;
        drag_start_overlay_ = *crop_overlay_;
        drag_start_image_pt_ = LogicalToImage(logical);
        CaptureMouse();
        return;
    }
    if (zoom_enabled_ && image_.IsOk()) {
        dragging_pan_ = true;
        drag_start_mouse_ = event.GetPosition();
        drag_start_scroll_ = GetViewStart();
        CaptureMouse();
        SetCursor(wxCursor(wxCURSOR_SIZING));
        return;
    }
    event.Skip();
}

void ImageView::OnMouseMove(wxMouseEvent& event) {
    if (dragging_handle_ != Handle::None) {
        wxPoint logical = CalcUnscrolledPosition(event.GetPosition());
        wxPoint img_pt = LogicalToImage(logical);
        int dx = img_pt.x - drag_start_image_pt_.x;
        int dy = img_pt.y - drag_start_image_pt_.y;
        wxRect r;
        if (dragging_handle_ == Handle::Inside) {
            r = drag_start_overlay_;
            r.Offset(dx, dy);
            r = ClampRectToImage(r);
        } else if (crop_ratio_locked_ && crop_ratio_ > 0.0) {
            r = ResizeWithLockedRatio(dragging_handle_, drag_start_overlay_, img_pt);
        } else {
            r = ResizeFreeform(dragging_handle_, drag_start_overlay_, dx, dy);
            r = ClampRectToImage(r);
        }
        crop_overlay_ = r;
        Refresh();
        if (on_crop_edited_) on_crop_edited_(*crop_overlay_);
        return;
    }

    if (dragging_pan_ && event.Dragging() && event.LeftIsDown()) {
        wxPoint delta = event.GetPosition() - drag_start_mouse_;
        // One scroll unit is one pixel (see the constructor), so the picture follows the mouse exactly.
        Scroll(std::max(0, drag_start_scroll_.x - delta.x), std::max(0, drag_start_scroll_.y - delta.y));
        NotifyViewChanged(true);
        return;
    }

    // Hover feedback: show a resize/move cursor near the overlay even
    // before a drag starts, so the interaction is discoverable.
    if (crop_overlay_ && !event.Dragging()) {
        wxPoint logical = CalcUnscrolledPosition(event.GetPosition());
        SetCursor(CursorForHandle(HitTestOverlay(logical)));
    }
    event.Skip();
}

void ImageView::OnMouseUp(wxMouseEvent& event) {
    if (dragging_handle_ != Handle::None) {
        dragging_handle_ = Handle::None;
        if (HasCapture()) ReleaseMouse();
    } else if (dragging_pan_) {
        dragging_pan_ = false;
        if (HasCapture()) ReleaseMouse();
        SetCursor(wxNullCursor);
    }
    event.Skip();
}

void ImageView::OnCaptureLost(wxMouseCaptureLostEvent&) {
    dragging_pan_ = false;
    dragging_handle_ = Handle::None;
    SetCursor(wxNullCursor);
}

void ImageView::ZoomAround(double new_zoom, wxPoint client_anchor) {
    if (!zoom_enabled_ || !image_.IsOk()) return;
    // The picture point under the anchor now - it has to be under it afterwards, too.
    const wxPoint logical = CalcUnscrolledPosition(client_anchor);
    const double ix = (logical.x - bitmap_offset_x_) / effective_zoom_;
    const double iy = (logical.y - bitmap_offset_y_) / (effective_zoom_ * vertical_squash_);
    fit_mode_ = false;
    manual_zoom_ = std::clamp(new_zoom, kMinZoom, kMaxZoom);
    RebuildScaledBitmap();
    setting_view_ = true;
    PlaceImagePoint(ix, iy, client_anchor);
    setting_view_ = false;
    Refresh();
    NotifyViewChanged(true);
}

void ImageView::ZoomIn() { ApplyZoom((fit_mode_ ? effective_zoom_ : manual_zoom_) * kZoomStep); }

void ImageView::ZoomOut() { ApplyZoom((fit_mode_ ? effective_zoom_ : manual_zoom_) / kZoomStep); }

void ImageView::ZoomActual() { ApplyZoom(1.0); }

void ImageView::ZoomToFit() {
    fit_mode_ = true;
    RebuildScaledBitmap();
    Refresh();
    NotifyViewChanged(true);
}
