/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include "config.hpp"
#include "global.hpp"
#include <memory>

#include <Wt/WConfig.h>
#include <Wt/WServer.h>
#include <Wt/WApplication.h>
#include <Wt/WEnvironment.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WTemplate.h>
#include <Wt/WLineEdit.h>
#include <Wt/WTextArea.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <Wt/Http/Client.h>
#include <Wt/Http/Message.h>

#ifndef WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION
// FIXME nasty public morozov
#define private friend class Wt::Wc::Recaptcha; private
#include <Wt/WCompositeWidget.h>
#undef private
#define implementation() Wt::WCompositeWidget::impl_.get()
#endif // WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION

#include "Recaptcha.hpp"
#include "util.hpp"

namespace Wt {

namespace Wc {

Recaptcha::Recaptcha(const std::string& public_key,
                     const std::string& private_key):
    AbstractCaptcha(),
    buttons_enabled_(true),
    public_key_(public_key),
    private_key_(private_key),
    input_(0),
    response_field_(0),
    challenge_field_(0) {
    Wt::WApplication::instance()->enableUpdates();
    Wt::WApplication::instance()->require("https://www.google.com/recaptcha/api/js/recaptcha_ajax.js",
                  "Recaptcha");
    http_ = new Http::Client(this);
    http_->done().connect(this, [this](std::error_code e, const Wt::Http::Message& msg) { http_done(e, msg); });
    update_impl();
}

Recaptcha::~Recaptcha() {
    doJavaScript("Recaptcha.destroy();");
    doJavaScript("clearTimeout($(" + jsRef() + ").data('timer'));");
}

void Recaptcha::get_image() {
    doJavaScript("Recaptcha.switch_type('image');");
}

void Recaptcha::get_audio() {
    doJavaScript("Recaptcha.switch_type('audio');");
}

void Recaptcha::set_buttons(bool enabled) {
    buttons_enabled_ = enabled;
    update();
}

void Recaptcha::set_input(WFormWidget* input) {
    input_ = input;
    update();
}

void Recaptcha::update_impl() {
    if (!implementation()) {
        setImplementation(std::make_unique<WContainerWidget>());
    }
    get_impl()->clear();
    auto title = get_impl()->addWidget(std::make_unique<WText>("reCAPTCHA"));
    title->addStyleClass("wc_recaptcha_title");
    if (js()) {
        auto image = get_impl()->addWidget(std::make_unique<WContainerWidget>());
        image->setId("recaptcha_image");
        response_field_ = input_ ? input_ : get_impl()->addWidget(std::make_unique<WLineEdit>());
        challenge_field_ = get_impl()->addWidget(std::make_unique<WLineEdit>());
        // not challenge_field_->hide() to get its .text()
        doJavaScript("$(" + challenge_field_->jsRef() + ").hide();");
        response_field_->setId("recaptcha_response_field");
        doJavaScript("Recaptcha.create('" + public_key_  + "', '',"
                     "{theme: 'custom'});");
        if (buttons_enabled_) {
            add_buttons();
        }
        doJavaScript("clearTimeout($(" + jsRef() + ").data('timer'));");
        doJavaScript("$(" + jsRef() + ").data('timer',"
                     "setInterval(function() {"
                     "$(" + challenge_field_->jsRef() + ")"
                     ".val(Recaptcha.get_challenge());"
                     "}, 200));");
    } else {
        auto iframe = get_impl()->addWidget(std::make_unique<WTemplate>());
        iframe->setTemplateText("<iframe src='https://www.google.com/recaptcha/"
                                "api/noscript?k=" + public_key_ +
                                "' height='300' width='500' frameborder='0'>"
                                "</iframe>", Wt::TextFormat::XHTMLUnsafe);
        if (input_) {
            challenge_field_ = input_;
        } else {
            auto ta = get_impl()->addWidget(std::make_unique<WTextArea>());
            ta->setColumns(40);
            ta->setRows(3);
            challenge_field_ = ta;
        }
        response_field_ = get_impl()->addWidget(std::make_unique<WLineEdit>("manual_challenge"));
        response_field_->hide();
    }
}

void Recaptcha::check_impl() {
    std::string challenge = value_text(challenge_field_).toUTF8();
    std::string response = value_text(response_field_).toUTF8();
    const std::string& remoteip = Wt::WApplication::instance()->environment().clientAddress();
    Http::Message m;
    m.setHeader("Content-Type", "application/x-www-form-urlencoded");
    m.addBodyText("privatekey=" + private_key_ + "&");
    m.addBodyText("remoteip=" + remoteip + "&");
    m.addBodyText("challenge=" + urlencode(challenge) + "&");
    m.addBodyText("response=" + urlencode(response) + "&");
#ifdef WT_WITH_SSL
    std::string schema = "https";
#else
    std::string schema = "http";
#endif
    if (!http_->post(schema + "://www.google.com/recaptcha/api/verify", m)) {
        mistake(tr("wc.captcha.Internal_error"));
    }
}

bool Recaptcha::js() const {
    return Wt::WApplication::instance()->environment().javaScript();
}

WContainerWidget* Recaptcha::get_impl() {
    return DOWNCAST<WContainerWidget*>(implementation());
}

void Recaptcha::http_done(const std::error_code& e,
                          const Http::Message& response) {
    if (e) {
        mistake(tr("wc.captcha.Internal_error"));
    } else if (response.body().find("true") == 0) {
        solve();
    } else if (response.body().find("incorrect-captcha-sol") != std::string::npos) {
        mistake(tr("wc.captcha.Wrong_response"));
    } else {
        mistake(tr("wc.captcha.Internal_error"));
    }
    updates_poster(Wt::WServer::instance(), Wt::WApplication::instance());
}

void Recaptcha::add_buttons() {
    auto u = get_impl()->addWidget(std::make_unique<WPushButton>(tr("wc.common.Update")));
    u->clicked().connect(this, &AbstractCaptcha::update);
    auto get_image_btn = get_impl()->addWidget(std::make_unique<WPushButton>());
    get_image_btn->addStyleClass("recaptcha_only_if_audio");
    get_image_btn->setText(tr("wc.captcha.Get_image"));
    get_image_btn->clicked().connect(this, &Recaptcha::get_image);
    auto get_audio_btn = get_impl()->addWidget(std::make_unique<WPushButton>());
    get_audio_btn->addStyleClass("recaptcha_only_if_image");
    get_audio_btn->setText(tr("wc.captcha.Get_audio"));
    get_audio_btn->clicked().connect(this, &Recaptcha::get_audio);
}

}

}
