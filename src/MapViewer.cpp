/*
 * Copyright (C) 2012 Ramil Mintaev
 *
 * See the LICENSE.MapViewer file for terms of use.
 */

#include "config.hpp"
#include "global.hpp"

#include <cmath>
#include <iomanip>
#include <numbers>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <Wt/WApplication.h>
#include <Wt/WLength.h>
#include <Wt/WString.h>
#include <Wt/WEnvironment.h>
#include <Wt/WGridLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WImage.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>
#include <Wt/WPointF.h>
#include <Wt/WPushButton.h>
#include <Wt/WCssStyleSheet.h>
#ifdef WC_HAVE_WHTTP_MESSAGE
#include <Wt/Http/Client.h>
#include <Wt/Http/Message.h>
#endif // WC_HAVE_WHTTP_MESSAGE
#ifdef WC_HAVE_JSON_OBJECT
#include <Wt/Json/Object.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Array.h>
#include <Wt/Json/Parser.h>
#endif // WC_HAVE_JSON_OBJECT

#ifndef WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION
// FIXME nasty public morozov
#define private friend class Wt::Wc::MapViewer; private
#include <Wt/WCompositeWidget.h>
#undef private
#define implementation() Wt::WCompositeWidget::impl_
#endif // WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION

#include "MapViewer.hpp"
#include "util.hpp"
#include "MapImage.hpp"

namespace Wt::Wc {

const double pi = std::numbers::pi;
const std::string nurl = "http://nominatim.openstreetmap.org/"
                         "search?format=json&q=";

const std::string tzurl = "http://ws.geonames.org/timezoneJSON?";
// lat=xx.xx&lng=xxx.xx

MapViewer::MapViewer():
    Wt::WCompositeWidget(), zoom_(1), sp_title_("search"),
    pos_(std::make_pair(Coordinate(0, 0), "")), jclicked_(this, "click"),
    markers_(false),
    smp_(false), html_search_panel_(false), enable_updates_(false),
    sr_button_(nullptr), sr_cw_(nullptr)
#if defined(WC_HAVE_WHTTP_MESSAGE) && defined(WC_HAVE_JSON_OBJECT)
    , http_(nullptr), tz_http_(nullptr)
#endif
{
    wApp->require("http://openlayers.org/api/OpenLayers.js",
                  "OpenLayers");
    setImplementation(std::make_unique<Wt::WContainerWidget>());
    get_impl()->setStyleClass("impl_cw");
    wApp->styleSheet().addRule(".impl_cw", "position:relative;");
    map_name_ = "map_" + get_impl()->id();
    layer_name_ = map_name_ + "_layer";
    marker_img_url_ = "http://www.openlayers.org/dev/img/marker.png";
    update_impl();
}

MapViewer::~MapViewer() {
    destroy_map();
#if defined(WC_HAVE_WHTTP_MESSAGE) && defined(WC_HAVE_JSON_OBJECT)
    delete http_;
    delete tz_http_;
#endif
}

void MapViewer::update_impl() {
    setLayoutSizeAware(true);
    if (!implementation()) {
        setImplementation(std::make_unique<Wt::WContainerWidget>());
    }
    get_impl()->clear();
    if (js()) {
        std::string map_created_str;
        map_created_str = "new OpenLayers.Map('"
                          + get_impl()->id() + "')";
        jclicked().connect([this](const Coordinate& c) { this->click_on(c); });
        doJavaScript(store_jsv(map_name_, map_created_str));
        add_osm_layer(layer_name_);
        set_click_signal_();
        wApp->styleSheet().addRule(".olControlAttribution",
                                   "position:absolute !important;"
                                   "bottom:0 !important;right:0 !important;");
    } else {
        html_v(get_impl());
    }
}

void MapViewer::add_osm_layer(const std::string& layer_var_name,
                              const std::string& param) {
    if (js()) {
        std::stringstream strm;
        strm << store_jsv(layer_var_name,
                          "new OpenLayers.Layer.OSM(" + param + ")")
             << get_stored_jsv(map_name_) << ".addLayer("
             << get_stored_jsv(layer_var_name) << ");";
        doJavaScript(strm.str());
    }
}

void MapViewer::remove_layer() {
    doJavaScript(get_stored_jsv(map_name_) + ".removeLayer("
                 + get_stored_jsv(layer_name_) + ", false);");
}

void MapViewer::destroy_map() {
    if (js()) {
        doJavaScript(get_stored_jsv(map_name_) + ".destroy();");
    }
}

void MapViewer::set_center(const Coordinate& center) {
    pos_ = std::make_pair(center, "");
    if (js()) {
        std::stringstream strm;
        strm << store_jsv("lonLatOf_" + map_name_,
                          get_lonlat_jsc(center))
             << get_stored_jsv(map_name_) << ".setCenter("
             << get_stored_jsv("lonLatOf_" + map_name_) << ");";
        doJavaScript(strm.str());
    } else {
        html_v(get_impl());
    }
}

void MapViewer::set_center(const Coordinate& center, int zoom) {
    zoom_ = zoom;
    pos_ = std::make_pair(center, "");
    marginal_tile_coords_ = marginal_pic_coords(center);
    map_param_calc();
    if (js()) {
        std::stringstream strm;
        strm << store_jsv("lonLatOf_" + map_name_,
                          get_lonlat_jsc(center))
             << store_jsv("zoomOf_" + map_name_, std::to_string(zoom_))
             << get_stored_jsv(map_name_) << ".setCenter("
             << get_stored_jsv("lonLatOf_" + map_name_)
             << ", " << get_stored_jsv("zoomOf_" + map_name_) + ");";
        doJavaScript(strm.str());
    } else {
        html_v(get_impl());
    }
}

void MapViewer::pan_to(const Coordinate& pos) {
    pos_ = std::make_pair(pos, pos_.second);
    marginal_tile_coords_ = marginal_pic_coords(pos);
    map_param_calc();
    if (js()) {
        std::stringstream strm;
        strm << store_jsv("pan_to_lonLatOf_" + map_name_,
                          get_lonlat_jsc(pos))
             << get_stored_jsv(map_name_) << ".panTo("
             << get_stored_jsv("pan_to_lonLatOf_" + map_name_)
             << ");";
        doJavaScript(strm.str());
    } else {
        html_v(get_impl());
    }
}

void MapViewer::pan_to(const GeoNode& node) {
    pos_ = node;
    pan_to(node.first);
}

void MapViewer::left_shift(double power) {
    double w = diff_between(marginal_tile_coords_.second.longitude(),
                            marginal_tile_coords_.first.longitude());
    pan_to(Coordinate(pos_.first.latitude(),
                      coord_control(pos_.first.longitude() - w * power)));
}

void MapViewer::right_shift(double power) {
    double w = diff_between(marginal_tile_coords_.second.longitude(),
                            marginal_tile_coords_.first.longitude());
    pan_to(Coordinate(pos_.first.latitude(),
                      coord_control(pos_.first.longitude() + w * power)));
}

void MapViewer::top_shift(double power) {
    double h = diff_between(marginal_tile_coords_.second.latitude(),
                            marginal_tile_coords_.first.latitude());
    pan_to(Coordinate(coord_control(pos_.first.latitude() + h * power, "lat"),
                      pos_.first.longitude()));
}

void MapViewer::bottom_shift(double power) {
    double h = diff_between(marginal_tile_coords_.second.latitude(),
                            marginal_tile_coords_.first.latitude());
    pan_to(Coordinate(coord_control(pos_.first.latitude() - h * power, "lat"),
                      pos_.first.longitude()));
}

void MapViewer::zoom_in() {
    if (zoom_ < 18) {
        zoom_++;
        marginal_tile_coords_ = marginal_pic_coords(pos_.first);
        map_param_calc();
    }
    if (js()) {
        doJavaScript(get_stored_jsv(map_name_) + ".zoomIn();");
    } else {
        html_v(get_impl());
    }
}

void MapViewer::zoom_out() {
    if (zoom_ > 1) {
        zoom_--;
        marginal_tile_coords_ = marginal_pic_coords(pos_.first);
        map_param_calc();
    }
    if (js()) {
        doJavaScript(get_stored_jsv(map_name_) + ".zoomOut();");
    } else {
        html_v(get_impl());
    }
}

void MapViewer::zoom_to(int level) {
    if (level > 0 && level <= 18) {
        zoom_ = level;
        marginal_tile_coords_ = marginal_pic_coords(pos_.first);
        map_param_calc();
    }
    if (js()) {
        doJavaScript(get_stored_jsv(map_name_) +
                     ".zoomTo(" + std::to_string(level) + ");");
    } else {
        html_v(get_impl());
    }
}

std::unique_ptr<Wt::WContainerWidget> MapViewer::get_html_map() {
    std::vector<int>  rmns;
    int wwidth = get_impl()->width().value();
    int wheight = get_impl()->height().value();
    int half_width = wwidth / 2;
    int half_height = wheight / 2;
    int to_left_margin =  half_width - tile_lt_.first;
    rmns.push_back(to_left_margin);
    int to_top_margin =  half_height - tile_lt_.second;
    int to_bottom_margin =  wheight - to_top_margin;
    rmns.push_back(to_bottom_margin);
    int to_right_margin =  wwidth - to_left_margin;
    rmns.push_back(to_right_margin);
    rmns.push_back(to_top_margin);
    //
    std::vector<int> crops;
    int column = 0;
    int row = 0;
    xy_pos_ = w2t(pos_.first, zoom_);
    int X = xy_pos_.x();
    int y = xy_pos_.y();
    double half_tls = std::pow(2, zoom_) / 2.0;
    double half_tls_px = half_tls * 256.0;
    if ((rmns[0] < half_tls_px && rmns[2] > half_tls_px)
            || (rmns[0] > half_tls_px && rmns[2] < half_tls_px)) {
        rmns[0] = half_width;
        rmns[2] = half_width;
    }
    if ((rmns[1] < half_tls_px && rmns[3] > half_tls_px)
            || (rmns[1] > half_tls_px && rmns[3] < half_tls_px)) {
        rmns[1] = half_height;
        rmns[3] = half_height;
    }
    double w = 0;
    double h = 0;
    for (int i = 0; i < 4; i++) {
        double invm = rmns[i] / 256.0;
        invm = invm > half_tls ? half_tls : invm;
        int cr = invm > 1.0 ? 256 - 256 * (invm - (int)invm) :
                 256 * (invm > 0 ? (1 - invm) : invm);
        int v = invm;
        if (cr > 0) {
            v++;
        }
        crops.push_back(get_abs(cr));
        if (i % 2 == 0) {
            column += v;
            X -= i ? 0 : v;
            w += invm * 256.0;
        } else {
            row += v;
            y -= i == 3 ? v : 0;
            h += invm * 256.0;
        }
    }
    //
    std::vector<int> img_margin(4, 0);
    auto gcw = std::make_unique<Wt::WContainerWidget>();
    auto gl = gcw->setLayout(std::make_unique<Wt::WGridLayout>());
    wApp->styleSheet().addRule(".mapContainer",
                               "position:relative;top:0px;");
    gcw->setStyleClass("mapContainer");
    gl->setHorizontalSpacing(0);
    gl->setVerticalSpacing(0);
    gl->setContentsMargins(0, 0, 0, 0);
    if (y < 0) {
        y = 0;
    }
    if (X < 0) {
        X = 0;
    }
    for (int i = 0; i < row; i++) {
        std::string ystd = std::to_string(y);
        bool vori = false;
        int cw_h = 256;
        img_margin[3] = 0;
        img_margin[1] = 0;
        if (i == 0) {
            vori = true;
            cw_h = 256 - crops[3] + (row < 2 ? 256 - crops[1] : 0);
            img_margin[3] = -crops[3];
            img_margin[1] = row < 2 ? -crops[1] : 0;
        } else if (i == row - 1) {
            vori = true;
            cw_h = 256 - crops[1];
            img_margin[1] = -crops[1];
        }
        int x = X;
        for (int j = 0; j < column; j++) {
            auto cw = std::make_unique<Wt::WContainerWidget>();
            std::string xstd = std::to_string(x);
            auto img = std::make_unique<Wt::WImage>("http://a.tile.openstreetmap.org/" +
                                                    std::to_string(zoom_) + "/" +
                                                    xstd + "/" + ystd + ".png");
            bool hori = false;
            int cw_w = 256;
            img_margin[0] = 0;
            img_margin[2] = 0;
            if (j == 0) {
                hori = true;
                cw_w = 256 - crops[0] + (column < 2 ? 256 - crops[2] : 0);
                img_margin[0] = -crops[0];
                img_margin[2] = column < 2 ? -crops[2] : 0;
            } else if (j == column - 1) {
                hori = true;
                cw_w = 256 - crops[2];
                img_margin[2] = -crops[2];
            }
            cw->resize(cw_w, cw_h);
            if (hori || vori) {
                cw->setOverflow(Wt::Overflow::Hidden);
                //
                for (int k = 0; k < 4; k++) {
                    img->setMargin(img_margin[k], get_side(k));
                }
            }
            auto map_img = std::make_unique<MapImage>(std::move(img));
            map_img->clicked().connect([this, x, y]() { this->click_on(Coordinate(x, y)); });
            
            gl->addWidget(std::move(map_img), i, j);
            x++;
        }
        y++;
    }
    gcw->resize(w, h);

    return gcw;
}

void MapViewer::html_markers_view(WContainerWidget* cw) {
    int cout = 0;
    wApp->styleSheet().addRule(".mvMarkers",
                               "position:absolute;z-index:2013;");
    for (const auto& mn : marker_nodes_) {
        if (is_map_contained(mn.first)) {
            WPoint coords = w2px(mn.first);
            auto link_img = std::make_unique<Wt::WImage>(marker_img_url_);
            link_img->setStyleClass("mvMarkers");
            link_img->setId("mvMarker" + std::to_string(cout));
            wApp->styleSheet().addRule("#mvMarker" + std::to_string(cout),
                                       "top:" + std::to_string(coords.y() - 25) + "px;"
                                       "left:" + std::to_string(coords.x() - 11) + "px;");
            cw->addWidget(std::move(link_img));
        }
        cout++;
    }
}

std::unique_ptr<Wt::WContainerWidget> MapViewer::get_html_osm_attribution() {
    auto cw = std::make_unique<Wt::WContainerWidget>();
    cw->setContentAlignment(Wt::AlignmentFlag::Right);
    cw->addWidget(std::make_unique<Wt::WText>("Data CC-By-SA by <b>OpenStreetMap</b>"));
    cw->setStyleClass("olControlAttribution");
    wApp->styleSheet().addRule(".olControlAttribution",
                               "position:absolute;"
                               "bottom:7px;right:5px;z-index:2013;"
                               "font-size:12px;");
    return cw;
}

std::unique_ptr<Wt::WContainerWidget> MapViewer::get_html_control_panel() {
    auto cw = std::make_unique<Wt::WContainerWidget>();
    auto vl = std::make_unique<Wt::WVBoxLayout>();
    vl->setSpacing(0);
    vl->setContentsMargins(0, 0, 0, 0);
    
    auto north_cw = std::make_unique<Wt::WContainerWidget>();
    north_cw->resize(WLength(), 18);
    north_cw->setContentAlignment(Wt::AlignmentFlag::Center);
    auto img_north = std::make_unique<Wt::WImage>("http://openlayers.org/api/img/north-mini.png");
    img_north->clicked().connect([this]() { this->top_shift(0.34); });
    north_cw->addWidget(std::move(img_north));
    vl->addWidget(std::move(north_cw));
    
    auto west_east_cw = std::make_unique<Wt::WContainerWidget>();
    west_east_cw->resize(WLength(), 18);
    west_east_cw->setContentAlignment(Wt::AlignmentFlag::Center);
    auto img_west = std::make_unique<Wt::WImage>("http://openlayers.org/api/img/west-mini.png");
    img_west->clicked().connect([this]() { this->left_shift(0.34); });
    west_east_cw->addWidget(std::move(img_west));
    
    auto img_east = std::make_unique<Wt::WImage>("http://openlayers.org/api/img/east-mini.png");
    img_east->clicked().connect([this]() { this->right_shift(0.34); });
    west_east_cw->addWidget(std::move(img_east));
    vl->addWidget(std::move(west_east_cw));
    
    auto south_cw = std::make_unique<Wt::WContainerWidget>();
    south_cw->resize(WLength(), 18);
    south_cw->setContentAlignment(Wt::AlignmentFlag::Center);
    auto img_south = std::make_unique<Wt::WImage>("http://openlayers.org/api/img/south-mini.png");
    img_south->clicked().connect([this]() { this->bottom_shift(0.34); });
    south_cw->addWidget(std::move(img_south));
    vl->addWidget(std::move(south_cw));
    
    vl->addWidget(std::make_unique<Wt::WBreak>());
    
    auto zoom_plus_cw = std::make_unique<Wt::WContainerWidget>();
    zoom_plus_cw->resize(WLength(), 18);
    zoom_plus_cw->setContentAlignment(Wt::AlignmentFlag::Center);
    auto img_zoom_plus = std::make_unique<Wt::WImage>("http://openlayers.org/api/img/zoom-plus-mini.png");
    img_zoom_plus->clicked().connect([this]() { this->zoom_in(); });
    zoom_plus_cw->addWidget(std::move(img_zoom_plus));
    vl->addWidget(std::move(zoom_plus_cw));
    
    auto zoom_minus_cw = std::make_unique<Wt::WContainerWidget>();
    zoom_minus_cw->resize(WLength(), 18);
    zoom_minus_cw->setContentAlignment(Wt::AlignmentFlag::Center);
    auto img_zoom_minus = std::make_unique<Wt::WImage>("http://openlayers.org/api/img/zoom-minus-mini.png");
    img_zoom_minus->clicked().connect([this]() { this->zoom_out(); });
    zoom_minus_cw->addWidget(std::move(img_zoom_minus));
    vl->addWidget(std::move(zoom_minus_cw));
    
    wApp->styleSheet().addRule(".menuControlPanel", "position:absolute;"
                               "width:60px;top:8px;left:0px;z-index:2013;");
    cw->setStyleClass("menuControlPanel");
    cw->setLayout(std::move(vl));
    return cw;
}

std::unique_ptr<Wt::WContainerWidget> MapViewer::html_search_panel() {
    auto cw = std::make_unique<Wt::WContainerWidget>();
    auto vl = std::make_unique<Wt::WVBoxLayout>();
    vl->setSpacing(0);
    vl->setContentsMargins(0, 0, 0, 0);
    //
    //
    auto edit = std::make_unique<Wt::WLineEdit>();
    edit->setText(sp_title_);
    edit->resize(90, WLength());
    //
    auto button_edit = std::make_unique<Wt::WPushButton>("ok");
    button_edit->clicked().connect([this, search_str = edit->text()]() { this->panel_html_search(search_str); });
    //
    auto hl = std::make_unique<Wt::WHBoxLayout>();
    hl->setSpacing(0);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->addWidget(std::move(edit), 0, Wt::AlignmentFlag::Middle);
    hl->addWidget(std::move(button_edit), 0, Wt::AlignmentFlag::Middle);
    //
    auto sr_button_ptr = std::make_unique<Wt::WPushButton>("ok");
    sr_button_ = sr_button_ptr.get();
    sr_button_->hide();
    //
    auto sr_cw_ptr = std::make_unique<Wt::WContainerWidget>();
    sr_cw_ = sr_cw_ptr.get();
    //
    vl->addWidget(std::move(sr_button_ptr), 0, Wt::AlignmentFlag::Right);
    vl->addWidget(std::move(sr_cw_ptr), 0, Wt::AlignmentFlag::Right);
    vl->addLayout(std::move(hl), 0, Wt::AlignmentFlag::Right);
    
    wApp->styleSheet().addRule(".mvSearchPanel", "position:absolute;"
                               "bottom:25px;right:10px;z-index:2014;");
    cw->setStyleClass("mvSearchPanel");
    cw->setStyleClass("mvSearch");
    if (sp_fns_.size() != 0) {
        html_search_present(sp_fns_);
    }
    cw->setLayout(std::move(vl));
    return cw;
}

void MapViewer::html_v(WContainerWidget* cw) {
    cw->clear();
    auto map_cw = std::make_unique<Wt::WContainerWidget>();
    map_cw->setStyleClass("map_cw");
    wApp->styleSheet().addRule(".map_cw", "position:relative;");
    if (markers_ && zoom_ > 4) {
        html_markers_view(map_cw.get());
    }
    map_cw->addWidget(get_html_map());
    cw->setContentAlignment(Wt::AlignmentFlag::Top);
    cw->addWidget(get_html_control_panel());
    cw->addWidget(std::move(map_cw));
    cw->addWidget(get_html_osm_attribution());
    if (html_search_panel_) {
        cw->addWidget(html_search_panel());
    }
}

Wt::Signal<MapViewer::GeoNodes>& MapViewer::found() {
    if (!found_) {
        found_ = std::make_unique<Wt::Signal<GeoNodes>>();
    }
    return *found_;
}

void MapViewer::search(const WString& query) {
    if (!found_) {
        found_ = std::make_unique<Wt::Signal<GeoNodes>>();
    }
    search(query, found_.get());
}

void MapViewer::search(const WString& query,
                       Signal<GeoNodes>* signal) {
    found_signal_ = signal;
    if (js()) {
        if (!jfound_) {
            jfound_ = std::make_unique<Wt::JSignal<std::string>>(this, "search");
        }
        jfound().connect([this](const std::string& str) { this->nominatim_data_parser(str); });
        doJavaScript(set_ajax_action(nurl + query.toUTF8(),
                                     jfound().createCall(std::initializer_list<std::string>{"str"})));
    } else {
#if defined(WC_HAVE_WHTTP_MESSAGE) && defined(WC_HAVE_JSON_OBJECT)
        http_ = new Http::Client(this); // Assuming this is cleaned up elsewhere
        http_->done().connect([this](std::error_code err, const Wt::Http::Message& msg) { this->nominatim_data_parser(err, msg); });
        if (http_->get(nurl + query.toUTF8())) {
        }
#endif // WC_HAVE_WHTTP_MESSAGE
    }
}

const MapViewer::GeoNode
MapViewer::found_node_parser(const std::string& data) const {
    auto amp_pos = data.find('&');
    std::string str_coords = data.substr(0, amp_pos);
    std::string str_fn = (amp_pos != std::string::npos) ? data.substr(amp_pos + 1) : "";
    
    auto space_pos = str_coords.find(' ');
    std::string lat_str = str_coords.substr(0, space_pos);
    std::string lng_str = (space_pos != std::string::npos) ? str_coords.substr(space_pos + 1) : "";
    
    double lat = std::stod(lat_str);
    double lng = std::stod(lng_str);
    
    lng = coord_control(lng);
    lat = coord_control(lat, "lat");
    return std::make_pair(Coordinate(lat, lng),
                          WString().fromUTF8(str_fn));
}

void MapViewer::nominatim_data_parser(const std::string& data) {
    GeoNodes found_nodes;
    size_t start = 0;
    size_t end = data.find('#');
    
    while (end != std::string::npos) {
        found_nodes.push_back(found_node_parser(data.substr(start, end - start)));
        start = end + 1;
        end = data.find('#', start);
    }
    if (start < data.length()) {
        found_nodes.push_back(found_node_parser(data.substr(start)));
    }
    
    found_signal_->emit(found_nodes);
}

const std::string MapViewer::cipher(const std::string& str) {
    return json_escape_utf8(str);
}

#if defined(WC_HAVE_WHTTP_MESSAGE) && defined(WC_HAVE_JSON_OBJECT)
const MapViewer::GeoNodes
MapViewer::http_request_parser(std::error_code e,
                               const Http::Message& response) {
    GeoNodes found_nodes;
    if (!e) {
        Json::Value val;
        try {
            Json::parse(cipher(response.body()), val);
        } catch (...) {
            found_nodes.push_back(std::make_pair(Coordinate(0, 0),
                                                 "problems with Wt::Json"));
            return found_nodes;
        }
        const Json::Array& arr = val;
        for (const auto& val1 : arr) {
            try {
                const Json::Object obj = val1;
                const WString& display_name = obj.get("display_name");
                double lat = obj.get("lat").toNumber().orIfNull(0.0);
                double lon = obj.get("lon").toNumber().orIfNull(0.0);
                lon = coord_control(lon);
                lat = coord_control(lat, "lat");
                found_nodes.push_back(std::make_pair(Coordinate(lat, lon),
                                                     display_name));
            } catch (...) {
                found_nodes.push_back(std::make_pair(Coordinate(0, 0),
                                                     "problems with Wt::Json"));
            }
        }
    } else {
        wApp->log("error") << "Http::Client error: " << e.message();
        found_nodes.push_back(std::make_pair(Coordinate(0, 0),
                                             "problems with Wt::Client"));
    }
    return found_nodes;
}

void MapViewer::nominatim_data_parser(std::error_code e,
                                      const Http::Message& response) {
    found_signal_->emit(http_request_parser(e, response));
}
#endif

void MapViewer::choice_data_parser(const std::string data) {
    chosen_->emit(found_node_parser(data));
}

const std::string MapViewer::get_smp_jsc() const {
    std::stringstream strm;
    strm << "if(" + get_stored_jsv("markers") + "!=undefined){"
         + get_stored_jsv("markers") + ".destroy();}"
         << "function add_markers(j){if(smarkers){"
         + get_stored_jsv("smarkers") + ".destroy();}"
         << store_jsv("smarkers",
                      "new OpenLayers.Layer.Markers('SMarkers')")
         << get_stored_jsv(map_name_) + ".addLayer("
         + get_stored_jsv("smarkers") + ");smarkers=true;"
         << "var size=new OpenLayers.Size(21,25);"
         "var sm_size=new OpenLayers.Size(9,13);"
         "var offset=new OpenLayers.Pixel(-(size.w/2),-size.h);"
         "var sm_offset=new OpenLayers.Pixel(-(sm_size.w/2),-sm_size.h);"
         "var marker_icon=new OpenLayers.Icon('"
         "http://www.openlayers.org/dev/img/marker.png',size,offset);"
         "var sm_marker_icon=new OpenLayers.Icon('"
         "http://www.openlayers.org/dev/img/marker.png',sm_size,sm_offset);"
         << "$(latLonArr).each(function(i,val){"
         << "if(j==-1){"
         + get_stored_jsv("smarkers") + ".addMarker("
         "new OpenLayers.Marker(" + get_lonlat_jsc("val[0]", "val[1]")
         + ",marker_icon.clone()));}else{"
         "if(j==i){" + get_stored_jsv("smarkers") + ".addMarker("
         "new OpenLayers.Marker(" + get_lonlat_jsc("val[0]", "val[1]")
         + ",marker_icon));}else{"
         + get_stored_jsv("smarkers") + ".addMarker("
         "new OpenLayers.Marker(" + get_lonlat_jsc("val[0]", "val[1]")
         + ",sm_marker_icon.clone()));}}"
         "});}add_markers(-1);"
         << get_stored_jsv(map_name_) + ".zoomToExtent("
         + get_stored_jsv("smarkers") + ".getDataExtent());";
    return strm.str();
}

const std::string MapViewer::get_search_js_action() const {
    std::stringstream strm;
    strm << "$('#mvSearchResult').empty();"
         "var choice='0 0&syntax.eror, you can try: \\\"a street, a town\\\"';"
         << "$('#bok').click(function(){"
         << "$('#mvSearchResult').empty();"
         << "$('#mvSearchResult').hide();"
         << "$('#bok').hide();"
         << jchosen_->createCall({"choice"}) // Wt4 requires initializer list
         << "if(smarkers){" + get_stored_jsv("smarkers") + ".destroy();"
         << "smarkers=false;}"
         << "$('#sq').attr('value',title);});"
         << "var arr = str.split('#');var latLonArr=[];"
         << "$(arr).each(function(i, val){"
         << "var node = val.split('&');"
         << "var latLon = node[0].split(' ');latLonArr.push(latLon);"
         << "var id = 'mvSearchResult_'+i;"
         << "$('#mvSearchResult').append('<div id=\\''+id"
         << "+'\\' margin=\\'15px\\' class=\\'mvSearchResultNode\\'>'"
         "+node[1]+'</div><br />');"
         << "$('#'+id).click(function(){"
         "choice=val;add_markers(i);"
         << get_stored_jsv(map_name_) << ".panTo("
         << get_lonlat_jsc("latLon[0]", "latLon[1]") << ");"
         << "});"
         << "});"
         << get_smp_jsc()
         << "var sr_h = $('#mvSearchResult').height();"
         << "var sr_w = $('#mvSearchResult').width();"
         << "if(sr_h>90){$('#mvSearchResult').height(90);}"
         << "if(sr_w>210){$('#mvSearchResult').width(210);}"
         << "$('#bok').show();$('#mvSearchResult').show();";
    return strm.str();
}

void MapViewer::set_search_panel(const WString& title) {
    if (!chosen_) {
        chosen_ = std::make_unique<Wt::Signal<GeoNode>>();
    }
    if (title != "") {
        sp_title_ = title.toUTF8();
    }
    wApp->styleSheet().addRule(".mvSearch:hover",
                               "filter:progid:"
                               "DXImageTransform.Microsoft.Alpha(opacity=100);"
                               "-moz-opacity: 1.0;"
                               "-khtml-opacity: 1.0;"
                               "opacity: 1.0;");
    wApp->styleSheet().addRule(".mvSearch",
                               "filter:progid:"
                               "DXImageTransform.Microsoft.Alpha(opacity=70);"
                               "-moz-opacity: 0.7;"
                               "-khtml-opacity: 0.7;"
                               "opacity: 0.7;");
    wApp->styleSheet().addRule(".mvSearchResultNode:hover",
                               "background:#cdcdcd;");
    wApp->styleSheet().addRule("#mvSearchResult",
                               "overflow:auto;"
                               "background:#bbbbbb;"
                               "font-size:12px;");
    if (js()) {
        if (!jchosen_) {
            jchosen_ = std::make_unique<Wt::JSignal<std::string>>(this, "chosen");
        }
        jchosen_->connect([this](const std::string& data) { this->choice_data_parser(data); });
        wApp->styleSheet().addRule("#mvSearchPanel",
                                   "position:absolute;"
                                   "bottom:20px;"
                                   "right:10px;"
                                   "z-index:2012;");
        wApp->styleSheet().addRule("#mvSearchClose",
                                   "overflow:auto;"
                                   "background:grey;"
                                   "font-size:12px;");
        std::stringstream strm;
        strm << "var title='" + sp_title_ + "';var smarkers=false;"
             << "$('<div id=\\'mvSearchPanel\\' class=\\'mvSearch\\' "
             "align=\\'right\\'>"
             << "<input type=\\'button\\' id=\\'bok\\' size=\\'4\\' "
             "value=\\'ok\\' style=\\'margin:0;z-index:2012;\\'/>"
             << "<div id=\\'mvSearchResult\\' class=\\'mvSearch\\'></div>"
             "<input type=\\'text\\' id=\\'sq\\' size=\\'12\\' "
             "value=\\''+title+'\\' style=\\'margin:0;\\'/>"
             "</div>').appendTo('#" << get_impl()->id() << "');"
             << "$('#bok').hide();"
             << "$('#sq').focus(function(){if($('#sq').val()==title){"
             "$('#sq').attr('value','');}});"
             << "$('#sq').keypress(function(e){"
             "if(e.which==13){"
             << set_ajax_action(nurl + "'+$('#sq').val()+'",
                                get_search_js_action())
             << "}});";
        doJavaScript(strm.str());
    } else {
        if (!html_search_panel_) {
            html_search_panel_ = true;
        }
        html_v(get_impl());
    }
}

void MapViewer::panel_html_search(const Wt::WString& search_str) {
    if (!html_found_signal_) {
        html_found_signal_ = std::make_unique<Wt::Signal<GeoNodes>>();
    }
    search(search_str, html_found_signal_.get());
    sp_fns_.clear();
    html_found_signal_->connect([this](const GeoNodes& ns) { this->html_search_present(ns); });
    smp_ = true;
    set_html_result_visible(false);
    
    auto refresh_button = std::make_unique<Wt::WPushButton>();
    refresh_button->setIcon("http://www.gettyicons.com/"
                            "free-icons/112/must-have/png/16/refresh_16.png");
    refresh_button->clicked().connect([this]() { this->simple_refresh(); });
    sr_cw_->addWidget(std::move(refresh_button));
    wApp->styleSheet().addRule(".mvSearchPanel", "bottom:30px;");
}

void MapViewer::simple_refresh() {  }

void MapViewer::html_search_present(const MapViewer::GeoNodes& ns) {
    set_html_result_visible();
    if (sp_fns_.size() == 0) {
        sp_fns_ = ns;
    }
    
    int cout = 0;
    
    // Wt4 Refactor: WScrollArea -> WContainerWidget with Overflow::Auto
    auto scroll = std::make_unique<Wt::WContainerWidget>();
    scroll->resize(sr_cw_->width(), sr_cw_->height());
    scroll->setOverflow(Wt::Overflow::Auto);
    
    auto cw = std::make_unique<Wt::WContainerWidget>();
    cw->setId("mvSearchResult");
    cw->setContentAlignment(Wt::AlignmentFlag::Right);
    
    // Construct layout, capture pointer, and set it on the widget immediately
    auto vl_unique = std::make_unique<Wt::WVBoxLayout>();
    Wt::WVBoxLayout* vl = vl_unique.get();
    vl->setContentsMargins(0, 0, 0, 0);
    cw->setLayout(std::move(vl_unique));
    
    for (const auto& n : ns) {
        auto n_cw = std::make_unique<Wt::WContainerWidget>();
        n_cw->setStyleClass("mvSearchResultNode");
        n_cw->setContentAlignment(Wt::AlignmentFlag::Center);
        
        auto hl_unique = std::make_unique<Wt::WHBoxLayout>();
        Wt::WHBoxLayout* hl = hl_unique.get();
        hl->setContentsMargins(0, 0, 0, 0);
        n_cw->setLayout(std::move(hl_unique));
        
        auto link_img = std::make_unique<Wt::WImage>("http://www.openlayers.org/dev/img/marker.png");
        link_img->clicked().connect([this, n, cout]() { this->click_node(n, cout); });
        hl->addWidget(std::move(link_img), 0, Wt::AlignmentFlag::Center);
        
        auto d_t = std::make_unique<Wt::WText>(n.second, Wt::TextFormat::XHTML);
        d_t->setStyleClass("mvSearchText");
        hl->addWidget(std::move(d_t), 0, Wt::AlignmentFlag::Center);
        
        vl->addWidget(std::move(n_cw), 0, Wt::AlignmentFlag::Center);
        cout++;
    }
    
    scroll->addWidget(std::move(cw));
    
    sr_cw_->setHidden(false);
    sr_cw_->setContentAlignment(Wt::AlignmentFlag::Right);
    sr_cw_->addWidget(std::move(scroll));
    
    if (smp_) {
        smp_ = false;
        smp_calc(ns);
    }
}

void MapViewer::click_node(const GeoNode& node, int num) {
    pan_to(node);
    ch_markers_size(num);
}

void MapViewer::ch_markers_size(int num) {
    int w = 21;
    int h = 25;
    if (num != -1) {
        w = 9;
        h = 13;
    }
    for (unsigned i = 0; i < marker_nodes_.size(); i++) {
        wApp->styleSheet().addRule("#mvMarker" + std::to_string(i),
                                   "width:" + std::to_string(w) + "px;"
                                   "height:" + std::to_string(h) + "px;");
    }
    if (num != -1) {
        wApp->styleSheet().addRule("#mvMarker" + std::to_string(num),
                                   "width:21px;"
                                   "height:25px;");
    }
}

void MapViewer::html_searh_chosen() {
    if (pos_.second.toUTF8() != "") {
        chosen_->emit(pos_);
        ch_markers_size();
        set_html_result_visible(false);
        sp_fns_.clear();
        smp_ = false;
        destroy_markers();
    }
}

void MapViewer::set_html_result_visible(bool enable) {
    sr_cw_->clear();
    if (enable) {
        sr_button_->show();
        sr_cw_->resize(270, 80);
        wApp->styleSheet().addRule(".mvSearchPanel", "bottom:25px;");
    } else {
        sr_button_->hide();
        sr_cw_->resize(WLength(), WLength());
        wApp->styleSheet().addRule(".mvSearchPanel", "bottom:25px;");
    }
}

void MapViewer::smp_calc(const MapViewer::GeoNodes& ns) {
    WRectF rect = tauten(ns);
    double xts = get_impl()->width().value() / 256.0;
    double yts = get_impl()->height().value() / 256.0;
    double dl = 1.01;
    double wl = get_abs(rect.width() * dl);
    double hl = get_abs(rect.height() * dl);
    int xn = std::round(std::log(360.0 * xts / wl) / std::log(2));
    int yn = std::round(std::log(180.0 * yts / hl) / std::log(2));
    int n = 1;
    if (xn >= yn) {
        n = yn;
    } else if (xn < yn) {
        n = xn;
    }
    if (n >= 0 && n <= 18) {
        if (n > 4) {
            zoom_ = n;
            double lon = coord_control(rect.center().x());
            double lat = coord_control(rect.center().y(), "lat");
            pan_to(Coordinate(lat, lon));
        }
        add_markers(ns);
    }
}

WRectF MapViewer::tauten(const MapViewer::GeoNodes& ns) {
    WPointF lt;
    WPointF rb;
    if (ns.size()) {
        lt = WPointF(ns[0].first.longitude(),
                     ns[0].first.latitude());
        rb = WPointF(ns[0].first.longitude(),
                     ns[0].first.latitude());
    }
    WPointF& lt_ = lt;
    WPointF& rb_ = rb;
    for (const auto& n : ns) {
        double lng = n.first.longitude();
        double lat = n.first.latitude();
        if (lt_.x() > lng) {
            lt_.setX(lng);
        }
        if (rb_.x() < lng) {
            rb_.setX(lng);
        }
        if (lt_.y() < lat) {
            lt_.setY(lat);
        }
        if (rb_.y() > lat) {
            rb_.setY(lat);
        }
    }
    return WRectF(lt, rb);
}

void MapViewer::add_markers(const MapViewer::GeoNodes& ns) {
    destroy_markers();
    if (js()) {
        std::stringstream strm;
        for (const auto& n : ns) {
            strm << adding_marker_jsc(n.first);
        }
        doJavaScript(strm.str());
    } else {
        marker_nodes_ = ns;
        markers_ = true;
        html_v(get_impl());
    }
}

const std::string MapViewer::adding_marker_jsc(const Coordinate& pos) {
    std::stringstream strm;
    if (!markers_) {
        markers_ = true;
        strm << store_jsv("markers",
                          "new OpenLayers.Layer.Markers('Markers')")
             << get_stored_jsv(map_name_) + ".addLayer("
             + get_stored_jsv("markers") + ");"
             << "var size = new OpenLayers.Size(21,25);"
             "var offset = new OpenLayers.Pixel(-(size.w/2),"
             "-size.h);"
             << store_jsv("marker_icon",
                          "new OpenLayers.Icon('http://www.openlayers.org/"
                          "dev/img/marker.png',size, offset)");
    }
    strm << get_stored_jsv("markers") + ".addMarker("
         "new OpenLayers.Marker(" + get_lonlat_jsc(pos)
         + "," + get_stored_jsv("marker_icon") + ".clone()));";
    return strm.str();
}

void MapViewer::destroy_markers() {
    if (markers_) {
        if (js()) {
            doJavaScript(get_stored_jsv("markers")
                         + ".destroy();");
        }
        markers_ = false;
        marker_nodes_.clear();
    }
}

const WPoint MapViewer::w2t(const Coordinate& pos, int zoom) const {
    // World to tile position.
    int x = (int)(std::floor((pos.longitude() + 180.0) /
                        360.0 * std::pow(2.0, zoom)));
    double lat_rad = pos.latitude() * pi / 180.0;
    int y = (int)(std::floor((1.0 - std::log(std::tan(lat_rad) + 1.0 /
                                        std::cos(lat_rad)) / pi) /
                        2.0 * std::pow(2.0, zoom)));
    return WPoint(x, y);
}

const MapViewer::Coordinate MapViewer::t2w(const WPoint& pos,
        int zoom) const {
    // Tile to World position.
    double lng = pos.x() / std::pow(2.0, zoom) * 360.0 - 180;
    double n = pi - 2.0 * pi * pos.y() / std::pow(2.0, zoom);
    double lat = 180.0 / pi * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
    lng = coord_control(lng);
    lat = coord_control(lat, "lat");
    return MapViewer::Coordinate(lat, lng);
}

const WPoint MapViewer::w2px(const Coordinate& pos) const {
    const std::pair<Coordinate, Coordinate>& m_coords =
        get_map_marginal_coords();
    double lng_diff = diff_between(m_coords.first.longitude(),
                                   pos.longitude());
    double lat_diff = diff_between(m_coords.first.latitude(),
                                   pos.latitude());
    return WPoint(std::round(lng_diff * to_px_.first),
                  std::round(lat_diff * to_px_.second));
}

void MapViewer::map_param_calc() {
    std::pair<double, double> tl_size = tile_size();
    double width_half_of_map_in_w_coords = get_impl()->width().value()
                                           / 512.0 * tl_size.first;
    double height_half_of_map_in_w_coords = get_impl()->height().value()
                                            / 512.0 * tl_size.second;
    double lt_lat = pos_.first.latitude() + height_half_of_map_in_w_coords;
    double rb_lat = pos_.first.latitude() - height_half_of_map_in_w_coords;
    double lt_lng = pos_.first.longitude() - width_half_of_map_in_w_coords;
    double rb_lng = pos_.first.longitude() + width_half_of_map_in_w_coords;
    lt_lat = coord_control(lt_lat, "lat");
    rb_lat = coord_control(rb_lat, "lat");
    lt_lng = coord_control(lt_lng);
    rb_lng = coord_control(rb_lng);
    marginal_map_coords_ = std::make_pair(Coordinate(lt_lat, lt_lng),
                                          Coordinate(rb_lat, rb_lng));
    int left = (int)(std::round(diff_between(pos_.first.longitude(),
                                        marginal_tile_coords_.first.longitude())
                           / tl_size.first * 256.0));
    int top = (int)(std::round(diff_between(pos_.first.latitude(),
                                       marginal_tile_coords_.first.latitude())
                          / tl_size.second * 256.0));
    tile_lt_ = std::make_pair(left, top);
    double wm = diff_between(marginal_map_coords_.first.longitude(),
                             marginal_map_coords_.second.longitude());
    double lm = diff_between(marginal_map_coords_.first.latitude(),
                             marginal_map_coords_.second.latitude());
    to_px_ = std::make_pair(get_impl()->width().value() / wm,
                            get_impl()->height().value() / lm);
}

const MapViewer::CoordinatePair
MapViewer::marginal_pic_coords(const WPoint& tile) const {
    return std::make_pair(t2w(tile, zoom_),
                          t2w(WPoint(tile.x() + 1, tile.y() + 1), zoom_));
}

const MapViewer::CoordinatePair
MapViewer::marginal_pic_coords(const MapViewer::Coordinate& pos) const {
    return marginal_pic_coords(w2t(pos, zoom_));
}

const std::string
MapViewer::set_ajax_action(const std::string& url,
                           const std::string& js_action) const {
    std::stringstream strm;
    strm << "jQuery.ajax({"
         << "url:'" << url << "',"
         << "dataType:'json',"
         << "success:function(data,textStatus){"
         << "var str='';"
         << "if(data.length>0){"
         << "$(data).each(function(i,val){"
         << "str+=val.lat+' '+val.lon+'&'+val.display_name;"
         << "if(i<data.length-1){str+='#';}"
         << "});}else{str='0 0&falied';}"
         << js_action
         << "}});";
    return strm.str();
}

std::string MapViewer::set_js_listener_control_(
    const JSignal<Coordinate> &signal,
    const std::string& signal_name) const {
    std::stringstream strm;
    strm << "OpenLayers.Control.Click = OpenLayers.Class(OpenLayers.Control, {"
         << "defaultHandlerOptions:  {'single': true, 'double': false,"
         << "'pixelTolerance': 0, 'stopSingle': false, 'stopDouble': false},"
         <<      "initialize: function(options) {"
         << "this.handlerOptions = OpenLayers.Util.extend("
         << "{}, this.defaultHandlerOptions);"
         << "OpenLayers.Control.prototype.initialize.apply(this, arguments);"
         << "this.handler = new OpenLayers.Handler.Click(this, {"
         << "'" + signal_name + "': this.trigger}, this.handlerOptions);},"
         <<      "trigger: function(e) {"
         << "var map = " + get_stored_jsv(map_name_) + ";"
         << "var lonlat = map.getLonLatFromViewPortPx(e.xy)"
         << ".transform(map.getProjectionObject(), "
         "new OpenLayers.Projection('EPSG:4326'));"
         << signal.createCall({"lonlat.lat +' ' + lonlat.lon"}) // Wt4 requires initializer list
         << ";}});";
    return strm.str();
}

void MapViewer::set_click_signal_() {
    std::stringstream strm;
    strm << set_js_listener_control_(jclicked_, "click")
         << store_jsv("click1", "new OpenLayers.Control.Click()")
         << get_stored_jsv(map_name_) << ".addControl("
         << get_stored_jsv("click1") << ");"
         << get_stored_jsv("click1") << ".activate();";
    doJavaScript(strm.str());
}

void MapViewer::click_on(const Coordinate& pos) {
    clicked_.emit(pos);
}

void MapViewer::click_on(const WPoint& tile_xy,
                         const Wt::Coordinates& img_xy) {
    std::pair<double, double> tsize = tile_size();
    Coordinate lt_coord = t2w(tile_xy, zoom_);
    double lng = lt_coord.longitude() + img_xy.x / 256.0 * tsize.first;
    double lat = lt_coord.latitude() - img_xy.y / 256.0 * tsize.second;
    lng = coord_control(lng);
    lat = coord_control(lat, "lat");
    clicked_.emit(Coordinate(lat, lng));
}

bool MapViewer::js() const {
    return wApp->environment().javaScript();
}

const std::string MapViewer::store_jsv(const std::string& key,
                                       const std::string& value) const {
    return "$(" + jsRef() + ").data('" + key + "', " + value + ");";
}

const std::string
MapViewer::get_stored_jsv(const std::string& key) const {
    return "$(" + jsRef() + ").data('" + key + "')";
}

const std::string MapViewer::get_lonlat_jsc(const std::string& lat,
        const std::string& lon) const {
    std::stringstream strm;
    strm << "new OpenLayers.LonLat("
         << lon + ", " + lat + ")"
         << ".transform(new OpenLayers.Projection('EPSG:4326'),"
         << get_stored_jsv(map_name_) + ".getProjectionObject())";
    return strm.str();
}

const std::string
MapViewer::get_lonlat_jsc(const Coordinate& pos) const {
    std::string lon = std::to_string(pos.longitude());
    std::string lat = std::to_string(pos.latitude());
    return get_lonlat_jsc(lat, lon);
}

Wt::Side MapViewer::get_side(int v) const {
    if (v == 0) {
        return Wt::Side::Left;
    } else if (v == 1) {
        return Wt::Side::Bottom;
    } else if (v == 2) {
        return Wt::Side::Right;
    } else if (v == 3) {
        return Wt::Side::Top;
    }
    return Wt::Side::Left;
}

Wt::WContainerWidget* MapViewer::get_impl() {
    return dynamic_cast<Wt::WContainerWidget*>(implementation().get());
}

template <class Type>
Type MapViewer::get_abs(Type val) const {
    return val < 0 ? -val : val;
}

double MapViewer::diff_between(double x, double y) const {
    if ((x >= 0 && y >= 0) || (x <= 0 && y <= 0)) {
        return get_abs(x - y);
    } else {
        if (x >= 0 && y < 0) {
            return x - y;
        } else {
            return get_abs(y - x);
        }
    }
    return 0;
}

double MapViewer::mod(double x, double y) const {
    double div = x / y;
    bool sign = false;
    if (div < 0) {
        sign = true;
        div *= -1;
    }
    double rest = div - std::floor(div);
    if (sign) {
        rest *= -1;
    }
    return y * rest;
}

double MapViewer::coord_control(double val, std::string dir) const {
    if (dir == "lon") {
        if (val > 360 || val < -360) {
            val = mod(val, 360);
        }
        if (val >= 0) {
            return (val <= 180 ? val : (val - 360.0));
        } else {
            return (val > -180 ? val : (val + 360.0));
        }
    } else if (dir == "lat") {
        if (val >= 0) {
            return (val <= 90 ? val : 90);
        } else {
            return (val >= -90 ? val : -90);
        }
    }
    return 0;
}

bool MapViewer::is_map_contained(const Coordinate& pos)
const {
    const std::pair<Coordinate, Coordinate>& m_coords =
        get_map_marginal_coords();
    bool is_cont = false;
    if (pos.latitude() < m_coords.first.latitude() &&
            pos.latitude() > m_coords.second.latitude() &&
            pos.longitude() > m_coords.first.longitude() &&
            pos.longitude() < m_coords.second.longitude()) {
        is_cont = true;
    }
    return is_cont;
}

std::pair<double, double> MapViewer::tile_size() {
    double wp = diff_between(marginal_tile_coords_.second.longitude(),
                             marginal_tile_coords_.first.longitude());
    double lp = diff_between(marginal_tile_coords_.second.latitude(),
                             marginal_tile_coords_.first.latitude());
    return std::make_pair(wp, lp);
}

Wt::Signal<MapViewer::TZ>& MapViewer::time_zone(const Coordinate& pos, bool ajax) {
    if (!tz_signal_) {
        tz_signal_ = std::make_unique<Wt::Signal<TZ>>();
    }
    std::string url = tzurl + "lat=" + std::to_string(pos.latitude())
                      + "&lng=" + std::to_string(pos.longitude());
    if (js() && ajax) {
        if (!jtz_signal_) {
            jtz_signal_ = std::make_unique<Wt::JSignal<std::string>>(this, "tz");
        }
        jtz_signal_->connect([this](const std::string& str) { this->tz_data_parser(str); });
        std::stringstream strm;
        strm << "jQuery.ajax({"
             << "url:'" << url << "',"
             << "dataType:'json',"
             << "success:function(data,textStatus){"
             << "var tz_data='';if(data.rawOffset!=undefined){"
             << "tz_data+=data.rawOffset + ' ';var is_st=0;"
             << "var tdiff=data.gmtOffset - data.dstOffset;"
             << "if(tdiff==1||tdiff==-1){is_st=1;}"
             << "tz_data+=is_st;}"
             << jtz_signal_->createCall({"tz_data"}) // Wt4 requires initializer list
             << "}});";
        doJavaScript(strm.str());
    } else {
#if defined(WC_HAVE_WHTTP_MESSAGE) && defined(WC_HAVE_JSON_OBJECT)
        tz_http_ = new Wt::Http::Client(this);
        tz_http_->done().connect([this](std::error_code err, const Wt::Http::Message& msg) { this->tz_data_parser(err, msg); });
        if (tz_http_->get(url)) {
        }
#endif
    }
    return *tz_signal_;
}

void MapViewer::tz_data_parser(const std::string& data) {
    int tz;
    bool is_st;
    if (data != "") {
        auto space_pos = data.find(' ');
        tz = std::stoi(data.substr(0, space_pos));
        is_st = std::stoi(data.substr(space_pos + 1)) ? true : false;
    } else {
        tz = -13;
        is_st = false;
    }
    TZ time_zone(tz, is_st);
    tz_signal_->emit(time_zone);
}

#if defined(WC_HAVE_WHTTP_MESSAGE) && defined(WC_HAVE_JSON_OBJECT)
void MapViewer::tz_data_parser(std::error_code e,
                               const Wt::Http::Message& response) {
    TZ tz;
    if (!e) {
        Wt::Json::Value val;
        try {
            Wt::Json::parse(cipher(response.body()), val);
        } catch (...) {
            tz = TZ(-13, false);
            tz_signal_->emit(tz);
            return;
        }
        try {
            const Wt::Json::Object obj = val;
            int gmt_off_set = 0;
            int dst_off_set = 0;
            try {
                tz.tz = obj.get("rawOffset").toNumber().orIfNull(-13);
            } catch (...) {
                tz = TZ(-13, false);
                tz_signal_->emit(tz);
                return;
            }
            try {
                gmt_off_set = obj.get("gmtOffset").toNumber().orIfNull(0);
            } catch (...) {
                tz.is_st = false;
            }
            try {
                dst_off_set = obj.get("dstOffset").toNumber().orIfNull(0);
            } catch (...) {
                tz.is_st = false;
            }
            tz.is_st = get_abs(gmt_off_set - dst_off_set) == 1 ? true : false;
        } catch (...) {
            tz = TZ(-13, false);
            tz_signal_->emit(tz);
            return;
        }
    } else {
        wApp->log("error") << "Http::Client error: " << e.message();
        tz = TZ(-13, false);
    }
    tz_signal_->emit(tz);
}
#endif

}   // namespace Wt::Wc

