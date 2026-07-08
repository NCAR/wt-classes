/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include "config.hpp"
#include "global.hpp"

#include <functional>

#include <Wt/WApplication.h>
#include <Wt/WEnvironment.h>
#include <Wt/WResource.h>
#include <Wt/Http/Request.h>
#include <Wt/Http/Response.h>
#include <Wt/WAnchor.h>
#include <Wt/WImage.h>

#ifndef WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION
// FIXME nasty public morozov
#define private friend class Wt::Wc::MapImage; private
#include <Wt/WCompositeWidget.h>
#undef private
// Wt 4 uses unique_ptr, so we must call .get() to extract the raw pointer for DOWNCAST
#define implementation() Wt::WCompositeWidget::impl_.get()
#endif // WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION

#include "MapImage.hpp"
#include "util.hpp"

namespace Wt {

namespace Wc {

class MapResource : public WResource {
public:
    MapResource(const std::string& redirect_to, MapImage* map_image):
        WResource(),
        redirect_to_(redirect_to),
        map_image_(map_image),
        app_(Wt::WApplication::instance())
    { }

    ~MapResource() {
        beingDeleted();
    }

    void handleRequest(const Wt::Http::Request& request,
                       Wt::Http::Response& response) override {
        Wt::WApplication::UpdateLock app_lock(app_);
        Wt::Coordinates xy(-1, -1);
        const std::string* xy_str = request.getParameter("MapImageXY");
        if (xy_str) {
            size_t comma_pos = xy_str->find(',');
            if (comma_pos != std::string::npos) {
                std::string x_str = xy_str->substr(1, comma_pos - 1);
                std::string y_str = xy_str->substr(comma_pos + 1);
                try {
                    int x = std::stoi(x_str);
                    int y = std::stoi(y_str);
                    xy.x = x;
                    xy.y = y;
                } catch (...) {
                }
            }
        }
        map_image_->clicked().emit(xy);
        response.setMimeType("text/html");
        response.out() << "<html><head>";
        response.out() << "<meta http-equiv='refresh' ";
        response.out() << " content='0; url=" << redirect_to_ << "' />";
        response.out() << "</head><body></body></html>";
    }

private:
    std::string redirect_to_;
    MapImage* map_image_;
    WApplication* app_;
};

MapImage::MapImage(std::unique_ptr<WImage> image):
    WCompositeWidget() {
    if (Wt::WApplication::instance()->environment().ajax()) {
        setImplementation(std::move(image));
        image->clicked().connect([this](const Wt::WMouseEvent& event) {
            clicked_.emit(event.widget());
        });
    } else {
        std::string url = Wt::WApplication::instance()->bookmarkUrl();
        char join = url.find('?') == std::string::npos ? '?' : '&';
        url += join;
        url += "wtd=" + Wt::WApplication::instance()->sessionId();
        auto resource = std::make_shared<MapResource>(url, this);
        image->setAttributeValue("ismap", "ismap");

        auto anchor = std::make_unique<WAnchor>(Wt::WLink(resource));
        anchor->setLink(Wt::WLink(resource->url() + "&MapImageXY="));
        anchor->addWidget(std::move(image));
        setImplementation(std::move(anchor));
    }
}

WImage* MapImage::image() {
    if (Wt::WApplication::instance()->environment().ajax()) {
        return DOWNCAST<WImage*>(implementation());
    } else {
        return DOWNCAST<WImage*>(DOWNCAST<WAnchor*>(implementation())->widget(0));
    }
}

}

}
