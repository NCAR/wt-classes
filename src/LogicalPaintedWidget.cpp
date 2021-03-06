/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <Wt/WPainter>
#include <Wt/WRectF>
#include <Wt/WPaintDevice>
#include <Wt/WPainter>
#include <Wt/WContainerWidget>

#include "LogicalPaintedWidget.hpp"
#include "Matrix3x3.hpp"

namespace Wt {

namespace Wc {

LogicalPaintedWidget::LogicalPaintedWidget(WContainerWidget* parent):
    WPaintedWidget(parent)
{ }

void LogicalPaintedWidget::set_logical_window(const WRectF& window,
        float border, bool preserve_aspect) {
    logical_window_ = add_borders(window, border);
    update_matrices(preserve_aspect);
}

void LogicalPaintedWidget::use_logical(WPainter& painter) const {
    painter.setWindow(logical_window());
    painter.setViewPort(logical_view_port());
}

void LogicalPaintedWidget::use_device(WPainter& painter) const {
    painter.setWindow(device_window());
    painter.setViewPort(device_window());
}

WPointF LogicalPaintedWidget::logical2device(const WPointF& logical) const {
    return logical2device_.map(logical);
}

WPointF LogicalPaintedWidget::device2logical(const WPointF& device) const {
    return device2logical_.map(device);
}

WRectF LogicalPaintedWidget::device_window() const {
    return WRectF(0, 0, width().toPixels(), height().toPixels());
}

WRectF LogicalPaintedWidget::add_borders(const WRectF& rect, float border) {
    double width = rect.width();
    double height = rect.height();
    WRectF res = rect;
    res.setX(rect.x() - width * border);
    res.setY(rect.y() - height * border);
    res.setWidth(width * (1 + 2 * border));
    res.setHeight(height * (1 + 2 * border));
    return res;
}

void LogicalPaintedWidget::update_matrices(bool preserve_aspect) {
    update_matrices(device_window(), preserve_aspect);
}

void LogicalPaintedWidget::update_matrices(const WRectF& device,
        bool preserve_aspect) {
    WRectF out = device;
    WRectF& in = logical_window_;
    if (preserve_aspect) {
        out = change_aspect(out, in);
    }
    logical_view_port_ = out;
    ThreeWPoints from(in.topLeft(), in.topRight(), in.bottomLeft());
    ThreeWPoints to(out.topLeft(), out.topRight(), out.bottomLeft());
    logical2device_ = Matrix3x3(from, to);
    device2logical_ = logical2device_.inverted();
    preserve_aspect_ = preserve_aspect;
}

void LogicalPaintedWidget::resize(const WLength& width, const WLength& height) {
    WPaintedWidget::resize(width, height);
    update_matrices(preserve_aspect_);
}

void LogicalPaintedWidget::layoutSizeChanged(int width, int height) {
    WPaintedWidget::layoutSizeChanged(width, height);
    update_matrices(preserve_aspect_);
}

WRectF LogicalPaintedWidget::change_aspect(const WRectF& rect,
        const WRectF& master) {
    float rect_aspect = rect.width() / rect.height();
    float master_aspect = master.width() / master.height();
    WRectF result = rect;
    if (rect_aspect > master_aspect) {
        // change width
        float aspect_factor = rect_aspect / master_aspect;
        result.setWidth(rect.width() / aspect_factor);
        float border_part = (rect_aspect - master_aspect) / rect_aspect;
        double border_width = rect.width() * (border_part / 2);
        result.setX(rect.x() + border_width);
    } else if (rect_aspect < master_aspect) {
        // change height
        float aspect_factor = master_aspect / rect_aspect;
        result.setHeight(rect.height() / aspect_factor);
        float border_part = (master_aspect - rect_aspect) / master_aspect;
        double border_width = rect.height() * (border_part / 2);
        result.setY(rect.y() + border_width);
    }
    return result;
}

}

}

