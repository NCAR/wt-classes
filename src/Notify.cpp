
#include <algorithm>
#include <utility>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <Wt/WServer>
#include <Wt/WApplication>

#include "Notify.hpp"

namespace Wt {

namespace Wc {

namespace notify {

Event::operator Event::Key() const {
    return key();
}

Widget::Widget(const Event::Key& key, Server* server, const std::string& /*a*/):
    key_(key), server_(server), app_id_(wApp) {
    server_->start_listening(this);
}

Widget::~Widget() {
    server_->stop_listening(this, app_id_);
}

void Widget::notify(EventPtr event)
{ }

Server::Server(WServer* /* server */):
    updates_enabled_(true),
    direct_to_this_(false)
{ }

void Server::emit(EventPtr event) {
    mutex_.lock();
    bool notify_in_this_app = false;
    O2W::iterator it = o2w_.find(event->key());
    if (it != o2w_.end()) {
        BOOST_FOREACH (const A2W::value_type& a2w, it->second) {
            WApplication* app = a2w.first;
            if (!direct_to_this_ || app != wApp) {
                const PosterAndWidgets& poster_and_widgets = a2w.second;
                OneAnyFunc& poster = *(poster_and_widgets.first);
                poster(event);
            } else {
                notify_in_this_app = true;
            }
        }
    }
    mutex_.unlock();
    if (notify_in_this_app) {
        notify_widgets(event);
    }
}

void Server::emit(Event* event) {
    emit(EventPtr(event));
}

void Server::start_listening(Widget* widget) {
    boost::mutex::scoped_lock lock(mutex_);
    WApplication* app_id = wApp;
    A2W& a2w = o2w_[widget->key()];
    if (a2w.find(app_id) == a2w.end()) {
        OneAnyFunc notifier = boost::bind(&Server::notify_widgets, this, _1);
        OneAnyFunc poster = one_bound_post(notifier);
        Poster poster_ptr = boost::make_shared<OneAnyFunc>(poster);
        a2w[app_id] = std::make_pair(poster_ptr, Widgets());
    }
    Widgets& widgets = a2w[app_id].second;
    widgets.push_back(widget);
}

void Server::stop_listening(Widget* widget, WApplication* app_id) {
    boost::mutex::scoped_lock lock(mutex_);
    Widgets& widgets = o2w_[widget->key()][app_id].second;
    widgets.erase(std::find(widgets.begin(), widgets.end(), widget));
    if (widgets.empty()) {
        o2w_[widget->key()].erase(app_id);
        if (o2w_[widget->key()].empty()) {
            o2w_.erase(widget->key());
        }
    }
}

void Server::notify_widgets(const boost::any& event) {
    Widgets widgets;
    mutex_.lock();
    const EventPtr* e = boost::any_cast<EventPtr>(&event);
    O2W::const_iterator o2w_it = o2w_.find((*e)->key());
    if (o2w_it != o2w_.end()) {
        const A2W& a2w = o2w_it->second;
        A2W::const_iterator a2w_it = a2w.find(wApp);
        if (a2w_it != a2w.end()) {
            const PosterAndWidgets& paw = a2w_it->second;
            widgets = paw.second;
        }
    }
    mutex_.unlock();
    bool updates_needed = false;
    BOOST_FOREACH (Widget* widget, widgets) {
        updates_needed |= widget->updates_needed(*e);
        widget->notify(*e);
    }
    if (updates_needed && updates_enabled_) {
        updates_trigger();
    }
}

}

}

}

