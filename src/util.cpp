/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include "config.hpp"

#define USE_SERVER_POST (defined(WC_HAVE_SERVER_POST) && \
    defined(WC_HAVE_ENVIRONMENT_SERVER))

#include <sstream>
#include <climits>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>

#ifdef WC_USE_WT_MD5
#include <Wt/Utils.h>
#endif

#ifdef WC_USE_OPENSSL
#include <openssl/evp.h>
#include <sstream>
#include <string>
#endif

#ifdef WC_HAVE_WIOSERVICE
#include <Wt/WIOService.h>
#endif

#include <Wt/WApplication.h>
#include <Wt/WEnvironment.h>
#include <Wt/WServer.h>
#include <Wt/WLineEdit.h>
#include <Wt/WTextArea.h>
#include <Wt/WTextEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WComboBox.h>
#include <Wt/WAbstractToggleButton.h>
#include <Wt/WSlider.h>
#include <Wt/WDialog.h>
#include <Wt/WTableView.h>
#ifndef WC_HAVE_STRING_LOCALE
#include <Wt/WLocale.h>
#endif

#include "util.hpp"
#include "rand.hpp"
#include "TimeDuration.hpp"

namespace Wt {

namespace Wc {

#if USE_SERVER_POST
static void func_runner(const std::function<void()>& func) {
    if (WApplication::instance() && !WApplication::instance()->hasQuit()) {
        func();
    }
}

static void post(WServer* server, const std::string& app,
                 const std::function<void()>& func) {
    server->post(app, [func]() { func_runner(func); });
}
#else
typedef std::shared_ptr<bool> BoolPtr;

class AG : public WObject {
public:
    AG(const BoolPtr& ptr):
        ptr_(ptr)
    { }

    ~AG() {
        *ptr_ = true;
    }

private:
    BoolPtr ptr_;
};

static std::mutex do_func_mutex;

static void do_func(std::function<void()> func, WApplication* app,
                    BoolPtr b) {
    std::scoped_lock do_func_lock(do_func_mutex);
    if (!*b && !app->hasQuit()) {
        WApplication::UpdateLock app_lock(app);
        if (!*b && !app->hasQuit()) {
            func();
        }
    }
}

static void thread_func(std::function<void()> func, WApplication* app,
                        BoolPtr b) {
    schedule_action(td::TD_NULL, [func, app, b]() { do_func(func, app, b); });
}
#endif

std::function<void()> bound_post(std::function<void()> func) {
    if (WApplication::instance()) {
#if USE_SERVER_POST
        WServer* server = DOWNCAST<WServer*>(WApplication::instance()->environment().server());
        std::string sessionId = WApplication::instance()->sessionId();
        return [server, sessionId, func]() { post(server, sessionId, func); };
#else
        BoolPtr ptr = std::make_shared<bool>(false);
        WApplication::instance()->addChild(std::make_unique<AG>(ptr));
        WApplication* app = WApplication::instance();
        return [func, app, ptr]() { thread_func(func, app, ptr); };
#endif
    } else {
        return [func]() { schedule_action(td::TD_NULL, func); };
    }
}

typedef std::vector<std::any> Anys;

struct OneData {
    Anys anys;
    std::mutex mutex;
    bool allow_merge;
};

struct OneAnyFuncBinder {
    void operator()() {
        std::mutex& mutex = arg_ptr->mutex;
        Anys& anys = arg_ptr->anys;
        bool allow_merge = arg_ptr->allow_merge;
        if (allow_merge) {
            mutex.lock();
            Anys anys_copy = anys;
            anys.clear();
            mutex.unlock();
            for (const auto& arg : anys_copy) {
                func(arg);
            }
        } else {
            mutex.lock();
            std::any arg = anys.back();
            anys.pop_back();
            mutex.unlock();
            func(arg);
        }
    }
    OneAnyFunc func;
    std::shared_ptr<OneData> arg_ptr;
};

struct OneAnyFuncHolder {
    void operator()(const std::any& arg) {
        std::mutex& mutex = arg_ptr->mutex;
        Anys& anys = arg_ptr->anys;
        bool allow_merge = arg_ptr->allow_merge;
        mutex.lock();
        bool post_needed = !allow_merge || anys.empty();
        anys.push_back(arg);
        mutex.unlock();
        if (post_needed) {
            posted_binder();
        }
    }
    std::function<void()> posted_binder;
    std::shared_ptr<OneData> arg_ptr;
};

static OneAnyFunc one_bound_post(const OneAnyFunc& func, bool allow_merge) {
    OneAnyFuncBinder binder;
    OneAnyFuncHolder holder;
    binder.func = func;
    binder.arg_ptr = std::make_shared<OneData>();
    binder.arg_ptr->allow_merge = allow_merge;
    holder.arg_ptr = binder.arg_ptr;
    holder.posted_binder = bound_post(binder);
    return holder;
}

void updates_trigger() {
    if (WApplication::instance() && WApplication::instance()->updatesEnabled() && !WApplication::instance()->hasQuit()) {
        WApplication::instance()->triggerUpdate();
    }
}

void updates_poster(WServer* server, WApplication* app) {
#if USE_SERVER_POST
    server->post(app->sessionId(), updates_trigger);
#else
    bound_post(updates_trigger)();
#endif
}

std::string unique_filename()
{
    return std::tmpnam(nullptr);
};

std::string config_value(const std::string& name, const std::string& def) {
    std::string value = def;
    if (WApplication::instance()) {
        WApplication::instance()->readConfigurationProperty(name, value);
    }
    return value;
}

WDateTime now() {
    return WDateTime::currentDateTime();
}

#ifdef WC_HAVE_MD5
std::string md5(const std::string& data) {
#ifdef WC_USE_WT_MD5
    return Wt::Utils::hexEncode(Wt::Utils::md5(data));
#elif defined(WC_USE_OPENSSL)
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);
    std::stringstream result;
    result << std::hex;
    result.fill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        result.width(2);
        result << static_cast<unsigned int>(digest[i]);
    }
    return result.str();
#endif
}
#endif

std::string urlencode(const std::string& url) {
    std::stringstream result;
    result.setf(std::ios::hex, std::ios::basefield);
    result.setf(std::ios::uppercase);
    result.width(2);
    result.fill('0');
    for (char c : url) {
        if (c == ' ') {
            result.put('+');
        } else if (c == '-' || c == '_' || c == '.') {
            result.put(c);
        } else if (isalnum(c) && (0 < c && c < 127)) {
            result.put(c);
        } else {
            result.put('%');
            result << short(c);
        }
    }
    return result.str();
}

std::string urldecode(const std::string& text) {
    // source: src/Wt/Utils.C
    std::stringstream result;
    for (unsigned i = 0; i < text.length(); ++i) {
        char c = text[i];
        if (c == '+') {
            result << ' ';
        } else if (c == '%' && i + 2 < text.length()) {
            std::string h = text.substr(i + 1, 2);
            char* e = 0;
            int hval = std::strtol(h.c_str(), &e, 16);
            if (*e == 0) {
                result << (char)hval;
                i += 2;
            } else {
                // not a proper %XX with XX hexadecimal format
                result << c;
            }
        } else {
            result << c;
        }
    }
    return result.str();
}

void set_hidden(WWidget* widget, bool hidden) {
    if (hidden) {
        widget->hide();
    } else {
        widget->show();
    }
}

std::string bool_to_string(bool value) {
    return value ? "true" : "false";
}

#define USE_WIOSERVICE (defined(WC_HAVE_WIOSERVICE) && \
        defined(WC_HAVE_ENVIRONMENT_SERVER))

void schedule_action(const td::TimeDuration& wait,
                     const std::function<void()>& func) {
#if USE_WIOSERVICE
    int ms = wait.total_milliseconds();
    if (ms < 0) {
        ms = INT_MAX;
    }
    WIOService& io = WServer::instance()->ioService();
    io.schedule(std::chrono::milliseconds(ms), func);
#else
    std::thread([wait, func]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(wait.total_milliseconds()));
        func();
    }).detach();
#endif
}

std::string approot() {
#ifdef WC_HAVE_WAPPLICATION_APPROOT
    return WApplication::appRoot();
#else
    return config_value("approot");
#endif
}

WString value_text(const WFormWidget* form_widget) {
#ifdef WC_HAVE_WFORMWIDGET_VALUETEXT
    return form_widget->valueText();
#else
    if (isinstance<WLineEdit>(form_widget)) {
        return DOWNCAST<const WLineEdit*>(form_widget)->text();
    } else if (isinstance<WTextArea>(form_widget)) {
        return DOWNCAST<const WTextArea*>(form_widget)->text();
    } else if (isinstance<WPushButton>(form_widget)) {
        return DOWNCAST<const WPushButton*>(form_widget)->text();
    } else if (isinstance<WComboBox>(form_widget)) {
        return DOWNCAST<const WComboBox*>(form_widget)->currentText();
    } else if (isinstance<WAbstractToggleButton>(form_widget)) {
        return DOWNCAST<const WAbstractToggleButton*>(form_widget)->text();
    } else if (isinstance<WSlider>(form_widget)) {
        return std::to_string(reinterpret_cast<const WSlider*>(form_widget)->value());
        // NOTE: WSlider used to be WCompositeWidget's descendant
        // In that case reinterpret_cast newer happens, since isinstance check
    } else {
        return "";
    }
#endif
}

void set_closable(WDialog* dialog) {
#ifdef WC_HAVE_WDIALOG_SET_CLOSABLE
    dialog->setClosable(true);
#else
    WPushButton* close;
#ifdef WC_HAVE_WDIALOG_TITLEBAR
    close = dialog->titleBar()->insertWidget(std::make_unique<WPushButton>("X"), 0, close);
#else
    close = dialog->contents()->addWidget(std::make_unique<WPushButton>("X"));
#endif
#endif
close->clicked().connect(dialog, &WDialog::reject);
}

// class DeleteSender : public WObject {
// public:
//     void delete_sender() {
//         delete sender();
//     }
// } delete_sender;

// void delete_closed(WDialog* dialog) {
//     dialog->finished().connect(&delete_sender, &DeleteSender::delete_sender);
// }

// void fix_text_edit(WTextEdit* text_edit) {
//     if (WApplication::instance() && !WApplication::instance()->environment().ajax()) {
//         return;
//     }
//     WWidget* parent_widget = text_edit->parent();
//     if (isinstance<WContainerWidget>(parent_widget)) {
//         WContainerWidget* parent = DOWNCAST<WContainerWidget*>(parent_widget);
//         WContainerWidget* wrapper = new WContainerWidget;
//         parent->insertBefore(wrapper, text_edit);
//         parent->removeWidget(text_edit);
//         wrapper->addWidget(text_edit);
//         if (text_edit->width() == WLength::Auto ||
//                 text_edit->height() == WLength::Auto) {
//             text_edit->resize(525, 130);
//         }
//         wrapper->resize(text_edit->width(), text_edit->height().toPixels() + 5);
//     }
// }

std::string json_escape_utf8(const std::string& utf8) {
    const std::wstring& wstr = WString().fromUTF8(utf8).value();
    std::stringstream strm;
    for (unsigned i = 0; i < wstr.size(); ++i) {
        if (wstr[i] < 128) {
            strm.put(char(wstr[i]));
        } else {
            strm.put('\\');
            strm.put('u');
            strm << std::hex
                 << std::setfill('0')
                 << std::setw(4)
                 << short(wstr[i]);
        }
    }
    return strm.str();
}

void scroll_to_last(WTableView* view) {
#ifdef WC_HAVE_ITEMVIEW_PAGING
    if (!WApplication::instance() || !WApplication::instance()->environment().ajax() || view->pageCount() > 0) {
        view->setCurrentPage(view->pageCount() - 1);
    } else
#endif
        if (WApplication::instance()) {
            WApplication::instance()->doJavaScript("$('#" + view->id() + " div')"
                               ".scrollTop(999999999);");
        }
}

int str2int(const std::string& str, int bad) {
    try {
        return std::stoi(str);
    } catch (std::invalid_argument&) {
        return bad;
    }
}

void fix_plain_anchors(int interval_ms,
                       const std::string& skip_re,
                       const std::string& target_blank_re,
                       const std::string& internal_path_re) {
    if (!WApplication::instance()) {
        return;
    }
    std::stringstream s;
    s << "if (!$.fix_plain_anchors) {";
    s << "  $.fix_plain_anchors = function(o, event) {";
    s << "    var e = event || window.event;";
    s << "    var WT = Wt.WT;";
    s << "    if (e.ctrlKey || e.metaKey || (WT.button(e) > 1)) {";
    s << "      return true;";
    s << "    } else {";
    s << "      {";
    s << "        var f = function() {";
    s << "          var path = o.pathname;";
    s << "          var prefix = (path.length && path[0]=='/') ? '' : '/';";
    s << "          WT.history.navigate(prefix + path, true);";
    s << "        };";
    s << "        f(o, e);";
    s << "      }";
    s << "      WT.cancelEvent(e,0x2);";
    //s << "      // Wt._p_.update(o,'s22b9d',e,true); // FIXME ???";
    s << "    }";
    s << "  };";
    s << "  setInterval(function() {";
    s << "    $('a').each(function() {";
    s << "      var a = $(this); var href = a.attr('href');";
    s << "      if (href && !a.attr('onclick') && ";
    s << "          !href.match(" << skip_re << ")) {";
    s << "        var same_host = a[0].hostname.replace(/^www\\./, '') ==";
    s << "          location.hostname.replace(/^www\\./, '');";
    s << "        var external = href.match(" << target_blank_re << ");";
    s << "        var internal = href.match(" << internal_path_re << ");";
    s << "        if (external || (!internal && !same_host)) {";
    s << "          a.attr('target','_blank');";
    s << "        } else {";
    s << "          a.attr('onclick','return false;');";
    s << "          a.click(function(e) { $.fix_plain_anchors(a[0], e); });";
    s << "        }";
    s << "      }";
    s << "    });";
    s << "  }, " << interval_ms << ");";
    s << "}";
    wApp->doJavaScript(s.str());
}

std::string get_locale(WApplication* app) {
    if (!app) {
        app = WApplication::instance();
    }
#ifdef WC_HAVE_STRING_LOCALE
    return app->locale();
#else
    return app->locale().name();
#endif
}

void set_locale(const std::string& locale, WApplication* app) {
    if (!app) {
        app = WApplication::instance();
    }
    app->setLocale(locale);
}

std::string url_scheme(WApplication* app) {
    if (!app) {
        app = WApplication::instance();
    }
    const WEnvironment& env = app->environment();
    std::string proto = env.headerValue("X-Forwarded-Proto");
    if (proto == "http" || proto == "https") {
        return proto;
    }
    if (env.urlScheme() == "http" || env.urlScheme() == "https") {
        return env.urlScheme();
    }
    return "http";
}

bool stop_ioservice(WServer& server) {
#ifdef WC_HAVE_WIOSERVICE
    server.ioService().stop();
#endif
    return 0;
}

// int wrun_stop_ioservice(int argc, char** argv, ApplicationCreator creator) {
//     WServer server(argv[0], "");
//     server.setServerConfiguration(argc, argv);
//     server.addEntryPoint(Wt::Application, creator);
//     if (server.start()) {
//         Wt::WServer::waitForShutdown();
//         stop_ioservice(server);
//         server.stop();
//         return 0;
//     } else {
//         return 1;
//     }
// }

}

}