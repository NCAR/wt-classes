/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <memory>

#include <Wt/WApplication.h>
#include <Wt/WEnvironment.h>
#include <Wt/WWebWidget.h>
#include <Wt/WResource.h>
#include <Wt/WImage.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>
#include <Wt/WMemoryResource.h>

#include "AdBlockDetector.hpp"
#include "rand.hpp"
#include "util.hpp"
#include "config.hpp"

namespace Wt {
namespace Wc {
class ReporterResource;
}
}


namespace Wt {

namespace Wc {

struct DefaultLists {
    DefaultLists() {
        banner_libs = std::make_shared<AdBlockDetector::Libs>();
        b("https://js.adscale.de/getads.js", "adscale");
        regular_libs = std::make_shared<AdBlockDetector::Libs>();
        r("https://cdnjs.cloudflare.com/ajax/libs/underscore.js/"
          "1.3.1/underscore-min.js", "_");
        image_paths = std::make_shared<AdBlockDetector::Strings>();
        i("/ad/side_");
        i("/adseo.");
        i("/adzone_");
        i("/external/ad/ad");
        i("/getmarketplaceads.");
        i("/popunderking.");
        i("/simpleadvert/ad");
        i("/top_ads/ad");
        i("_adwrap.");
        i("/160x600.");
        html_ids = std::make_shared<AdBlockDetector::Strings>();
        h("ad-sponsors");
        h("adBlock125");
        h("advertising-banner");
        h("home-rectangle-ad");
        h("priceGrabberAd");
        h("tmglBannerAd");
        h("topAdvBox");
        h("adsense");
    }

    void b(const std::string& url, const std::string& symbol) {
        banner_libs->push_back(std::make_pair(url, symbol));
    }

    void r(const std::string& url, const std::string& symbol) {
        regular_libs->push_back(std::make_pair(url, symbol));
    }

    void i(const std::string& part) {
        image_paths->push_back(part);
    }

    void h(const std::string& html_id) {
        html_ids->push_back(html_id);
    }

    AdBlockDetector::LibsPtr banner_libs;
    AdBlockDetector::LibsPtr regular_libs;
    AdBlockDetector::StringsPtr image_paths;
    AdBlockDetector::StringsPtr html_ids;
} default_lists;

static const unsigned char gif1x1[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00,
    0x80, 0x00, 0x00, 0xdb, 0xdf, 0xef, 0x00, 0x00, 0x00, 0x21,
    0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44,
    0x01, 0x00, 0x3b
};

class ReporterResource : public WMemoryResource {
public:
    ReporterResource(bool* flag):
        WMemoryResource("image/gif"),
        flag_(flag) {
        setData(gif1x1, 43);
    }

protected:
    void handleRequest(const Http::Request& request, Http::Response& response) override {
        *flag_ = true;
        WMemoryResource::handleRequest(request, response);
    }

private:
    bool* flag_;
};

AdBlockDetector::AdBlockDetector(bool call_start):
    signal_(this, "abdetector"),
    local_banner_image_(false),
    local_regular_image_(false),
    remote_banner_js_(false),
    remote_regular_js_(false),
    banner_ids_hidden_(false),
    regular_ids_hidden_(false),
    skip_remote_js_(false),
    skip_local_image_(false),
    skip_ids_hidden_(false),
    banner_libs_(default_lists.banner_libs),
    regular_libs_(default_lists.regular_libs),
    image_paths_(default_lists.image_paths),
    html_ids_(default_lists.html_ids),
    banner_image_(nullptr),
    regular_image_(nullptr) {
    if (call_start) {
        start();
    }
}

bool AdBlockDetector::has_adblock(bool true_if_maybe) const {
    bool adblock = false;
    bool maybe = false;
    if (!skip_local_image_) {
        adblock |= local_regular_image_ && !local_banner_image_;
        maybe |= !local_banner_image_;
    }
    if (WApplication::instance()->environment().ajax()) {
        if (!skip_remote_js_) {
            adblock |= remote_regular_js_ && !remote_banner_js_;
            maybe |= !remote_banner_js_;
        }
        if (!skip_ids_hidden_) {
            adblock |= banner_ids_hidden_ && !regular_ids_hidden_;
            maybe |= banner_ids_hidden_;
        }
    }
    return adblock || (maybe && true_if_maybe);
}

void AdBlockDetector::start() {
    if (!skip_local_image_) {
        banner_image_ = std::make_shared<ReporterResource>(&local_banner_image_);
        addWidget(std::make_unique<WImage>(Wt::WLink(banner_image_), /* alt text */ ""));
        regular_image_ = std::make_shared<ReporterResource>(&local_regular_image_);
        addWidget(std::make_unique<WImage>(Wt::WLink(regular_image_), /* alt text */ ""));
    }
    if (WApplication::instance()->environment().ajax()) {
        if (!skip_remote_js_ || !skip_ids_hidden_) {
            signal_.connect([this](std::string name) { signal_handler(name); });
        }
        if (!skip_remote_js_) {
            check_remote_js(true);
            check_remote_js(false);
        }
        if (!skip_ids_hidden_) {
            check_hidden(true);
            check_hidden(false);
        }
    }
}

void AdBlockDetector::set_js_params(std::string& url, std::string& symbol,
                                    bool banner) const {
    const Libs& libs = banner ? *banner_libs_ : *regular_libs_;
    int i = rr(libs.size());
    url = libs[i].first;
    symbol = libs[i].second;
}

std::string AdBlockDetector::image_path(bool banner) const {
    std::string result = "/" + rand_string(rr(5, 10));
    if (banner) {
        const Strings& parts = *image_paths_;
        result += parts[rr(parts.size())];
    } else {
        result += ".";
    }
    result += rand_string(rr(2, 4));
    return result;
}

std::string AdBlockDetector::html_id(bool banner) const {
    std::string result;
    if (banner) {
        const Strings& ids = *html_ids_;
        result = ids[rr(ids.size())];
    } else {
        result = rand_string(rr(5, 15));
    }
    return result;
}

void AdBlockDetector::signal_handler(std::string name) {
    if (!skip_remote_js_ && name == "remote_regular_js") {
        remote_regular_js_ = true;
    } else if (!skip_remote_js_ && name == "remote_banner_js") {
        remote_banner_js_ = true;
    } else if (!skip_ids_hidden_ && name == "banner_ids_hidden") {
        banner_ids_hidden_ = true;
    } else if (!skip_ids_hidden_ && name == "regular_ids_hidden") {
        regular_ids_hidden_ = true;
    }
}

void AdBlockDetector::check_remote_js(bool banner) {
    std::string js_url, js_symbol;
    set_js_params(js_url, js_symbol, banner);
    std::string name = "remote_";
    name += (banner ? "banner" : "regular");
    name += "_js";
    std::string js_name_str = WWebWidget::jsStringLiteral(name);

    // FIXME loadScript() is undocumented Wt JavaScript function
    doJavaScript("loadScript('" + js_url + "', function() {"
                 "if (typeof " + js_symbol + " != 'undefined') {" +
                 signal_.createCall(std::initializer_list<std::string>{js_name_str}) +
                 "}"
                 "js_symbol = undefined;"
                 "});");
}

void AdBlockDetector::check_hidden(bool banner) {
    auto text = addWidget(std::make_unique<WText>(" "));
    text->setId(html_id(banner));
    std::string name = banner ? "banner" : "regular";
    name += "_ids_hidden";
    std::string js_name_str = WWebWidget::jsStringLiteral(name);
    doJavaScript("if ($(" + text->jsRef() + ").css('display') == 'none') {" +
                 signal_.createCall(std::initializer_list<std::string>{js_name_str}) +
                 "}");
}

}

}
