/**
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * wt-classes is licensed under the GNU GPL Version 2.
 * Other versions of the GPL do not apply.
 * See the LICENSE file for terms of use.
 */

#define BOOST_FILESYSTEM_VERSION 3

#include <cstdio>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>

#include <Wt/WContainerWidget>
#include <Wt/WCompositeWidget>
#include <Wt/WLineEdit>
#include <Wt/WCheckBox>
#include <Wt/WFileUpload>
#include <Wt/WTextArea>
#include <Wt/WBreak>
#include <Wt/WFileResource>
#include <Wt/WAnchor>

#include "Wbi.hpp"
#include "TableForm.hpp"

namespace Wt {

namespace Wc {

AbstractArgument::AbstractArgument(const std::string& option_name):
    option_name_(option_name)
{ }

void AbstractArgument::add_args(std::vector<std::string>& args) {
    if (!option_name_.empty()) {
        args.push_back(option_name_);
    }
    if (!option_value_.empty()) {
        args.push_back(option_value_);
    }
}

AbstractInput::AbstractInput(const std::string& option_name):
    AbstractArgument(option_name), required_(true)
{ }

WFormWidget* AbstractInput::form_widget() {
    return form_widget_impl();
}

const WFormWidget* AbstractInput::form_widget() const {
    AbstractInput* nonconst_this = const_cast<AbstractInput*>(this);
    return nonconst_this->form_widget_impl();
}

WFormWidget* AbstractInput::form_widget_impl() {
    return 0;
}

bool AbstractInput::is_valid() const {
    return true;
}

void AbstractInput::add_args(std::vector<std::string>& args) {
    if (is_valid()) {
        AbstractArgument::add_args(args);
    }
}

FormWidgetInput::FormWidgetInput(WFormWidget* widget,
                                 const std::string& option_name):
    AbstractInput(option_name) {
    setImplementation(widget);
}

WFormWidget* FormWidgetInput::form_widget_impl() {
    return static_cast<WFormWidget*>(implementation());
}

bool FormWidgetInput::is_valid() const {
    WFormWidget* fw = const_cast<WFormWidget*>(form_widget()); // FIXME
    return fw->validate() == WValidator::Valid;
}

LineEditInput::LineEditInput(WLineEdit* widget, const std::string& option_name):
    FormWidgetInput(widget, option_name)
{ }

WLineEdit* LineEditInput::line_edit() {
    return static_cast<WLineEdit*>(form_widget());
}

const WLineEdit* LineEditInput::line_edit() const {
    return static_cast<const WLineEdit*>(form_widget());
}

void LineEditInput::set_option() {
    option_value_ = line_edit()->text().toUTF8();
};

FileInput::FileInput(const std::string& option_name):
    AbstractInput(option_name) {
    impl_ = new WContainerWidget();
    file_upload_ = new WFileUpload(impl_);
    file_upload_->changed().connect(file_upload_, &WFileUpload::upload);
    setImplementation(impl_);
}

void FileInput::set_option() {
    option_value_ = file_upload_->spoolFileName();
}

bool FileInput::is_valid() const {
    return !file_upload_->empty();
}

TextFileInput::TextFileInput(const std::string& option_name):
    FileInput(option_name) {
    new WBreak(impl_);
    text_area_ = new WTextArea(impl_);
    // TODO connection with file_upload_
}

WFormWidget* TextFileInput::form_widget_impl() {
    return text_area_;
}

void TextFileInput::set_option() {
    // TODO save contents of text_area_ to file_upload_->spoolFileName()
    // TODO and return its name
}

AbstractOutput::AbstractOutput(const std::string& option_name):
    AbstractArgument(option_name),
    selectable_(true), selected_by_default_(true),
    selected_(selected_by_default_) {
    WCheckBox* box = new WCheckBox();
    box->setChecked(selected_by_default_);
    box->setEnabled(selectable_);
    if (selectable_) {
        box->changed().connect(this, &AbstractOutput::select_handler_);
    }
    setImplementation(box);
}

bool AbstractOutput::is_needed() const {
    return !is_selectable() || is_selected();
}

void AbstractOutput::select_handler_() {
    WCheckBox* box = static_cast<WCheckBox*>(sender());
    selected_ = box->isChecked();
}

FileOutput::FileOutput(const std::string& option_name,
                       const FileOutput::NameGen& temp_gen,
                       const std::string& download_mime):
    AbstractOutput(option_name),
    temp_gen_(temp_gen),
    suggested_gen_(boost::bind(&FileOutput::temp_notdir, this)),
    download_mime_(download_mime)
{ }

FileOutput::~FileOutput() {
    if (!temp_file_.empty()) {
        remove(temp_file_.c_str());
        temp_file_ = "";
    }
}

const std::string& FileOutput::temp_file() const {
    if (temp_file_.empty()) {
        temp_file_ = temp_gen_();
    }
    return temp_file_;
}

std::string FileOutput::suggested_name() const {
    return suggested_gen_();
}

std::string FileOutput::unique_name() {
    return boost::filesystem::unique_path().string();
}

std::string FileOutput::temp_notdir() const {
    return boost::filesystem::path(temp_file()).filename().string();
}

void FileOutput::set_option() {
    option_value_ = temp_file();
}

void FileOutput::task_finished_handler() {
    WFileResource* r = new WFileResource(download_mime_, temp_file());
    r->suggestFileName(suggested_name());
    setImplementation(new WAnchor(r, tr("wc.Download")));
}

}

}

