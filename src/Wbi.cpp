/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

#ifdef WC_HAVE_BOOST_PROGRAM_OPTIONS
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#endif

#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WLineEdit.h>
#include <Wt/WCheckBox.h>
#include <Wt/WFileUpload.h>
#include <Wt/WTextArea.h>
#include <Wt/WBreak.h>
#include <Wt/WFileResource.h>
#include <Wt/WAnchor.h>
#include <Wt/WText.h>
#include <Wt/WApplication.h>
#include <Wt/WServer.h>
#include <Wt/WLink.h>

#include "config.hpp"
#include "global.hpp"

#ifndef WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION
// FIXME nasty public morozov
#define private friend class Wt::Wc::AbstractOutput; \
                friend class Wt::Wc::TableTask; \
                friend class Wt::Wc::FormWidgetInput; private
#include <Wt/WCompositeWidget.h>
#undef private
#define implementation() Wt::WCompositeWidget::impl_.get()
#endif // WC_HAVE_WCOMPOSITEWIDGET_IMPLEMENTATION

#include "Wbi.hpp"
#include "TableForm.hpp"
#include "FileView.hpp"
#include "util.hpp"

namespace Wt {

namespace Wc {

AbstractArgument::AbstractArgument(const std::string& option_name):
    option_name_(option_name)
{ }

void AbstractArgument::add_args(const ArgUser& f) const {
    add_args_impl(f);
}

bool AbstractArgument::large() const {
    return large_impl();
}

void AbstractArgument::add_args_impl(const ArgUser& f) const {
    add_option(f);
}

void AbstractArgument::add_option(const ArgUser& f) const {
    const_cast<AbstractArgument*>(this)->set_option();
    if (!option_name_.empty()) {
        f(option_name_, /* escape */ false);
    }
    if (!option_value_.empty()) {
        f(option_value_, /* escape */ true);
    }
}

bool AbstractArgument::large_impl() const {
    return false;
}

AbstractInput::AbstractInput(const std::string& option_name):
    AbstractArgument(option_name), required_(true)
{ }

void AbstractInput::add_args_impl(const ArgUser& f) const {
    if (state() == VALID) {
        add_option(f);
    }
}

WFormWidget* AbstractInput::form_widget() {
    return form_widget_impl();
}

const WFormWidget* AbstractInput::form_widget() const {
    AbstractInput* nonconst_this = const_cast<AbstractInput*>(this);
    return nonconst_this->form_widget_impl();
}

AbstractInput::State AbstractInput::state() const {
    return VALID;
}

bool AbstractInput::accepted() const {
    return state() == VALID || (!is_required() && state() == EMPTY);
}

WFormWidget* AbstractInput::form_widget_impl() {
    return 0;
}
}
}
