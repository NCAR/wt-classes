/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <map>
#include <mutex>
#include <thread>

#include <Wt/WDateTime.h>
#include <Wt/WApplication.h>
#include <Wt/WEnvironment.h>

#include "AbstractCaptcha.hpp"
#include "TimeDuration.hpp"
#include "util.hpp"

namespace Wt {

namespace Wc {

AbstractCaptcha::AbstractCaptcha(std::unique_ptr<WWidget> parent):
    WCompositeWidget(std::move(parent)),
    fault_(),
    in_progress_(false),
    is_solved_(false)
{ }

AbstractCaptcha::~AbstractCaptcha() {}

void AbstractCaptcha::check() {
    if (is_solved_) {
        solved_.emit();
    } else if (!in_progress_) {
        WString error = !precheck_ ? WString::Empty : precheck_();
        if (!error.empty()) {
            mistake(error);
        } else {
            in_progress_ = true;
            check_impl();
        }
    }
}

void AbstractCaptcha::update() {
    in_progress_ = false;
    is_solved_ = false;
    update_impl();
}

void AbstractCaptcha::set_buttons(bool)
{ }

void AbstractCaptcha::set_input(WFormWidget* /* input */)
{ }

namespace frequency_check_namespace {

std::mutex mutex;
typedef std::map<std::string, WDateTime> Map;
Map ip2last;
int calls = 0;
const int FILTER_EVERY = 1000;
const td::TimeDuration CHECK_INTERVAL(3.0); // seconds

}

WString AbstractCaptcha::frequency_check() {
    using namespace frequency_check_namespace;
    std::scoped_lock lock(mutex);
    WString result;
    const std::string ip = wApp->environment().clientAddress();
    Map::iterator it = ip2last.find(ip);
    if (it != ip2last.end()) {
        if (it->second + CHECK_INTERVAL > now()) {
            result = tr("wc.captcha.Too_often");
        }
    }
    ip2last[ip] = now();
    calls += 1;
    if (calls >= FILTER_EVERY) {
        calls = 0;
        Map::iterator i = ip2last.begin();
        while (i != ip2last.end()) {
            if (i->second  + CHECK_INTERVAL < now()) {
                ip2last.erase(i++);
            } else {
                ++i;
            }
        }
    }
    return result;
}

void AbstractCaptcha::solve() {
    in_progress_ = false;
    is_solved_ = true;
    solved_.emit();
}

void AbstractCaptcha::mistake(const WString& message) {
    update();
    fault_.emit(message);
}
}

}
