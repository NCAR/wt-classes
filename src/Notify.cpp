#include <algorithm>
#include <utility>

#include <Wt/WObject.h>
#include <Wt/WServer.h>
#include <Wt/WApplication.h>

#include "Notify.hpp"
#include "util.hpp"
#include "TimeDuration.hpp"

namespace Wt::Wc::notify {
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

    std::mutex do_func_mutex;

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
        schedule_action(td::TimeDuration::zero(), [func, app, b]() { do_func(func, app, b); });
    }
#endif
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
            for (const std::any& arg : anys_copy) {
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

OneAnyFunc one_bound_post(const OneAnyFunc& func, bool allow_merge) {
    OneAnyFuncBinder binder;
    OneAnyFuncHolder holder;
    binder.func = func;
    binder.arg_ptr = std::make_shared<OneData>();
    binder.arg_ptr->allow_merge = allow_merge;
    holder.arg_ptr = binder.arg_ptr;
    holder.posted_binder = bound_post(binder);
    return holder;
}

Event::~Event()
{ }

Event::operator Event::Key() const {
  return key();
}

Widget::Widget(const Event::Key& key, Server* server, const std::string& /*a*/):
    server_(server), app_id_(Wt::WApplication::instance()) {
    start_listening(key);
}

Widget::Widget(Server* server):
    server_(server), app_id_(Wt::WApplication::instance())
{ }

void Widget::start_listening(const Event::Key& key) {
    Server::WidgetAndKeyList changes;
    changes.push_back(std::make_pair(this, key));
    server_->start_listening(changes);
}

void Widget::start_listening(const Event::KeyList& keylist) {
    Server::WidgetAndKeyList changes;
    for (const Event::Key& key : keylist) {
        changes.push_back(std::make_pair(this, key));
    }
    server_->start_listening(changes);
}

void Widget::stop_listening(const Event::KeyList& keylist) {
    Server::WidgetAndKeyList changes;
    for (const Event::Key& key : keylist) {
        changes.push_back(std::make_pair(this, key));
    }
server_->stop_listening(changes);
}

void Widget::stop_listening(const Event::Key& key) {
    Server::WidgetAndKeyList changes;
    changes.push_back(std::make_pair(this, key));
    server_->stop_listening(changes);
}

void Widget::stop_listening() {
    stop_listening(keylist_);
}

Widget::~Widget() {
    stop_listening();
}

void Widget::notify(EventPtr event) {
    if (Wt::WApplication::instance()) {
        Wt::WApplication::instance()->log("warn") << "Base class notify::Widget was notified.";
    }
}

const Event::Key Widget::key() const {
    return keylist_.empty() ? "" : keylist_[0];
}

Server::Server(WServer* /* server */):
    updates_enabled_(true),
    direct_to_this_(false),
    merge_allowed_(true)
{ }

void Server::emit(EventPtr event) const {
    mutex_.lock();
    bool notify_in_this_app = false;
    // find any Applications interested in this event
    O2W::const_iterator it = o2w_.find(event->key());
    if (it != o2w_.end()) {
          for (const A2W::value_type& a2w : it->second) {
            WApplication* app = a2w.first;
            if (!direct_to_this_ || app != Wt::WApplication::instance() || app == 0) {
                const PosterAndWidgets& poster_and_widgets = a2w.second;
                const OneAnyFunc& poster = *(poster_and_widgets.first);
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

void Server::emit(Event* event) const {
    emit(EventPtr(event));
}

class DummyEvent : public Event {
public:
    DummyEvent(const std::string& key):
        key_(key)
    { }

    std::string key() const {
        return key_;
    }

private:
    std::string key_;
};

void Server::emit(const std::string& key) const {
    emit(std::make_shared<DummyEvent>(key));
}

Server::PosterPtr Server::get_poster_ptr(WApplication* app_id) {
    PosterWeakPtr& poster_weak_ptr = a2p_[app_id];
    PosterPtr poster_ptr;
    if (poster_weak_ptr.expired()) {
        OneAnyFunc notify = [this](const std::any& arg) { this->notify_widgets(arg); };
        OneAnyFunc poster = one_bound_post(notify, merge_allowed_);
        poster_ptr = std::make_shared<OneAnyFunc>(poster);
        poster_weak_ptr = poster_ptr;
        return poster_ptr;
    } else {
        return poster_weak_ptr.lock();
    }
}

void Server::start_listening(const WidgetAndKeyList& changes) {
      std::lock_guard<std::mutex> lock(mutex_);
    WApplication* app_id = Wt::WApplication::instance();
    PosterPtr poster_ptr = get_poster_ptr(app_id);
    for (const WidgetAndKey& widget_and_key : changes) {
        Widget* widget = widget_and_key.first;
        const Event::Key& key = widget_and_key.second;
        A2W& a2w = o2w_[key];
        if (a2w.find(app_id) == a2w.end()) {
            a2w[app_id] = std::make_pair(poster_ptr, Widgets());
        }
        Widgets& widgets = a2w[app_id].second;
        widgets.push_back(widget);
        widget->keylist_.push_back(key);
    }
}

typedef std::set<Widget*> WidgetsSet;

static WidgetsSet& widgets_set() {
    thread_local WidgetsSet widgets_s;
    return widgets_s;
}

void Server::remove_key(Widget* widget, const Event::Key& key) {
    // remove pair from internal map
    WApplication* app_id = widget->app_id_;
    Widgets& widgets = o2w_[key][app_id].second;
    Widgets::iterator it = std::find(widgets.begin(), widgets.end(), widget);
    if (it != widgets.end()) {
        *it = widgets.back();
        widgets.pop_back();
        if (widgets.empty()) {
            o2w_[key].erase(app_id);
            if (a2p_[app_id].expired()) {
                a2p_.erase(app_id);
            }
            if (o2w_[key].empty()) {
                o2w_.erase(key);
            }
        }
    }
    // remove key from widget
    Event::KeyList& keys = widget->keylist_;
    Event::KeyList::iterator kit = std::find(keys.begin(), keys.end(), key);
    if (kit != keys.end()) {
        // remove it: move back to it and pop back
        *kit = keys.back();
        keys.pop_back();
    }
}

void Server::stop_listening(const WidgetAndKeyList& changes) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const WidgetAndKey& widget_and_key : changes) {
        Widget* widget = widget_and_key.first;
        const Event::Key& key = widget_and_key.second;
        remove_key(widget, key);
        widgets_set().erase(widget);
    }
}

void Server::notify_widgets(const std::any& event) const {
    WidgetsSet& widgets_s = widgets_set();
    widgets_s.clear();
    mutex_.lock();
    const EventPtr* e = std::any_cast<EventPtr>(&event);
    O2W::const_iterator o2w_it = o2w_.find((*e)->key());
    if (o2w_it != o2w_.end()) {
        const A2W& a2w = o2w_it->second;
        A2W::const_iterator a2w_it = a2w.find(Wt::WApplication::instance());
        if (a2w_it != a2w.end()) {
            const PosterAndWidgets& paw = a2w_it->second;
            const Widgets& widgets_v = paw.second;
            WidgetsSet widgets_s_temp(widgets_v.begin(), widgets_v.end());
            widgets_s.swap(widgets_s_temp);
        }
    }
    mutex_.unlock();
    bool updates_needed = false;
    while (!widgets_s.empty()) {
        WidgetsSet::iterator it = widgets_s.begin();
        Widget* widget = *it;
        widgets_s.erase(it);
        updates_needed |= widget->updates_needed(*e);
        widget->notify(*e);
    }
    if (updates_needed && updates_enabled_) {
        if (Wt::WApplication::instance()) {
          Wt::WApplication::instance()->triggerUpdate();
        }
      }
    }

  }  // namespace Wt::Wc::notify