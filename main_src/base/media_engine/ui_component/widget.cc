// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "ui_component/widget.h"

#include <algorithm>
#include "drawer.h"

namespace media_engine {

Widget::Widget(float x_pct, float y_pct, float w_pct, float h_pct)
    : x_pct_(x_pct), y_pct_(y_pct), w_pct_(w_pct), h_pct_(h_pct) {
}

Widget::~Widget() {
    if (parent_) parent_->RemoveChild(this);
    for (auto* child : children_) {
        child->parent_ = nullptr;
    }
}

void Widget::SetRoot(float w_px, float h_px) {
    root_w_ = w_px;
    root_h_ = h_px;
    has_root_ = true;
    if (!parent_) ResolveSelf();
}

void Widget::SetPosition(float x_pct, float y_pct, float w_pct, float h_pct) {
    x_pct_ = x_pct;
    y_pct_ = y_pct;
    w_pct_ = w_pct;
    h_pct_ = h_pct;
    if (parent_ || has_root_) ResolveSelf();
}

void Widget::ResolveSelf() {
    float pw, ph, px, py;
    if (parent_) {
        pw = parent_->width_;
        ph = parent_->height_;
        px = parent_->x_;
        py = parent_->y_;
    } else if (has_root_) {
        pw = root_w_;
        ph = root_h_;
        px = 0;
        py = 0;
    } else {
        return;
    }

    float new_x = px + pw * (x_pct_ / 100.0f);
    float new_y = py + ph * (y_pct_ / 100.0f);
    float new_w = pw * (w_pct_ / 100.0f);
    float new_h = ph * (h_pct_ / 100.0f);

    bool size_changed = (new_w != width_ || new_h != height_);
    x_ = new_x;
    y_ = new_y;
    width_ = new_w;
    height_ = new_h;

    if (size_changed) OnResize();
}

void Widget::SetPixelRect(float x, float y, float w, float h) {
    x_ = x;
    y_ = y;
    bool size_changed = (w != width_ || h != height_);
    width_ = w;
    height_ = h;
    if (size_changed) OnResize();
}

void Widget::AddChild(Widget* child) {
    if (!child) return;
    if (child->parent_) child->parent_->RemoveChild(child);
    child->parent_ = this;
    children_.push_back(child);
    child->ResolveSelf();
}

void Widget::RemoveChild(Widget* child) {
    if (!child || child->parent_ != this) return;
    child->parent_ = nullptr;
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) children_.erase(it);
}

void Widget::OnResize() {
    for (auto* child : children_) {
        child->ResolveSelf();
    }
}

void Widget::Render(const RenderContext& ctx) {
    if (!visible_) return;
    Drawer::Instance().DrawFillRect(x_, y_, width_, height_, bg_color_);
    for (auto* child : children_) {
        child->Render(ctx);
    }
}

}  // namespace media_engine
