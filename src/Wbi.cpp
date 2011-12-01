/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * wt-classes is licensed under the GNU GPL Version 2.
 * Other versions of the GPL do not apply.
 * See the LICENSE file for terms of use.
 */

#define BOOST_FILESYSTEM_VERSION 3

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <Wt/WContainerWidget>
#include <Wt/WCompositeWidget>
#include <Wt/WPushButton>
#include <Wt/WLineEdit>
#include <Wt/WCheckBox>
#include <Wt/WFileUpload>
#include <Wt/WTextArea>
#include <Wt/WBreak>
#include <Wt/WFileResource>
#include <Wt/WAnchor>
#include <Wt/WText>
#include <Wt/WApplication>
#include <Wt/WServer>

#include "Wbi.hpp"
#include "TableForm.hpp"
#include "util.hpp"

namespace Wt {

namespace Wc {

AbstractArgument::AbstractArgument(const std::string& option_name):
    option_name_(option_name)
{ }

void AbstractArgument::add_args(const ArgUser& f) {
    add_args_impl(f);
}

void AbstractArgument::add_args_impl(const ArgUser& f) {
    add_option(f);
}

void AbstractArgument::add_option(const ArgUser& f) {
    set_option();
    if (!option_name_.empty()) {
        f(option_name_, /* escape */ false);
    }
    if (!option_value_.empty()) {
        f(option_value_, /* escape */ true);
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

void AbstractInput::add_args_impl(const ArgUser& f) {
    if (is_valid()) {
        add_option(f);
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
    file_upload_->uploaded().connect(this, &TextFileInput::uploaded_handler);
    // TODO File too large
}

void TextFileInput::uploaded_handler() {
    std::string filename = file_upload_->spoolFileName();
    std::ifstream file(filename.c_str());
    text_area_->setText(std::string(std::istreambuf_iterator<char>(file),
                                    std::istreambuf_iterator<char>()));
}

WFormWidget* TextFileInput::form_widget_impl() {
    return text_area_;
}

void TextFileInput::set_option() {
    std::string filename = file_upload_->spoolFileName();
    std::ofstream file(filename.c_str());
    file << text_area_->text().toUTF8();
    file.close();
    option_value_ = filename;
}

BoolInput::BoolInput(const std::string& name_if_true, bool checked):
    AbstractInput(""),
    name_if_true_(name_if_true) {
    initialize(checked);
}

BoolInput::BoolInput(const std::string& name_if_true,
                     const std::string& name_if_false, bool checked):
    AbstractInput(""),
    name_if_true_(name_if_true), name_if_false_(name_if_false) {
    initialize(checked);
}

BoolInput::BoolInput(const std::string& name,
                     const std::string& value_if_true,
                     const std::string& value_if_false, bool checked):
    AbstractInput(""),
    name_if_true_(name), name_if_false_(name),
    value_if_true_(value_if_true), value_if_false_(value_if_false) {
    initialize(checked);
}

void BoolInput::set_option() {
    if (impl_->isChecked()) {
        option_name_ = name_if_true_;
        option_value_ = value_if_true_;
    } else {
        option_name_ = name_if_false_;
        option_value_ = value_if_false_;
    }
}

void BoolInput::initialize(bool checked) {
    setImplementation(impl_ = new WCheckBox());
    impl_->setChecked(checked);
}

AbstractOutput::AbstractOutput(const std::string& option_name):
    AbstractArgument(option_name),
    selectable_(true), selected_by_default_(true),
    selected_(selected_by_default_) {
    setImplementation(new WContainerWidget());
    WCheckBox* box = new WCheckBox();
    box->setChecked(selected_by_default_);
    box->setEnabled(selectable_);
    if (selectable_) {
        box->changed().connect(this, &AbstractOutput::select_handler);
    }
    container()->addWidget(box);
}

bool AbstractOutput::is_needed() const {
    return !is_selectable() || is_selected();
}

void AbstractOutput::select_handler() {
    WCheckBox* box = static_cast<WCheckBox*>(sender());
    selected_ = box->isChecked();
}

WContainerWidget* AbstractOutput::container() {
    return static_cast<WContainerWidget*>(implementation());
}

void AbstractOutput::finished_handler() {
    finished_handler_impl();
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

void FileOutput::finished_handler_impl() {
    container()->clear();
    container()->addWidget(anchor());
}

WAnchor* FileOutput::anchor() const {
    WFileResource* r = new WFileResource(download_mime_, temp_file());
    r->suggestFileName(suggested_name());
    return new WAnchor(r, tr("wc.wbi.Download"));
}

ViewFileOutput::ViewFileOutput(const std::string& option_name,
                               const NameGen& temp_gen,
                               const std::string& download_mime,
                               const std::string& view_mime):
    FileOutput(option_name, temp_gen, download_mime),
    view_mime_(view_mime)
{ }

void ViewFileOutput::finished_handler_impl() {
    container()->clear();
    WFileResource* r = new WFileResource(view_mime_, temp_file());
    WAnchor* a = new WAnchor(r, tr("wc.wbi.View"));
    a->setTarget(TargetNewWindow);
    container()->addWidget(FileOutput::anchor());
    container()->addWidget(new WText(" | "));
    container()->addWidget(a);
}

AbstractTask::AbstractTask(WContainerWidget* p):
    WCompositeWidget(p), runner_(0), queue_(0), queued_(false)
{ }

void AbstractTask::add_input(AbstractInput* input, const WString& name,
                             const WString& description) {
    args_.push_back(input);
    add_input_impl(input, name, description);
}

void AbstractTask::add_output(AbstractOutput* output, const WString& name,
                              const WString& description) {
    args_.push_back(output);
    add_output_impl(output, name, description);
}

void AbstractTask::set_runner(AbstractRunner* runner) {
    if (runner_) {
        WObject::removeChild(runner_);
    }
    runner_ = runner;
    WObject::addChild(runner);
    runner_->set_task(this);
}

void AbstractTask::set_queue(AbstractQueue* queue) {
    if (state() == UNSET || state() == NEW) {
        queue_ = queue;
    }
}

void AbstractTask::run() {
    if (queue_) {
        queue_->add(this);
    } else {
        run_impl();
    }
}

void AbstractTask::visit_args(const AbstractArgument::ArgUser& f) {
    BOOST_FOREACH (AbstractArgument* arg, args_) {
        arg->add_args(f);
    }
}

RunState AbstractTask::state() const {
    return queued_ ? QUEUED : runner_ ? runner_->state() : UNSET;
}

void AbstractTask::changed_emitter() {
    BOOST_FOREACH (AbstractArgument* arg, args_) {
        if (isinstance<AbstractOutput>(arg)) {
            dynamic_cast<AbstractOutput*>(arg)->finished_handler();
        }
    }
    changed_.emit();
}

void AbstractTask::run_impl() {
    if (runner_) {
        runner_->run();
    }
    changed_.emit();
}

TableTask::TableTask(WContainerWidget* p):
    AbstractTask(p) {
    WContainerWidget* impl = new WContainerWidget();
    inputs_ = new TableForm(impl);
    inputs_->section(tr("wc.wbi.Inputs"));
    outputs_ = new TableForm(impl);
    outputs_->section(tr("wc.wbi.Outputs"));
    WPushButton* go = new WPushButton(tr("wc.wbi.Run"), impl);
    go->clicked().connect(this, &AbstractTask::run);
    setImplementation(impl);
}

void TableTask::add_input_impl(AbstractInput* input, const WString& name,
                               const WString& description) {
    // TODO row argument of item()
    inputs_->item(name, description, input->form_widget(), input);
}

void TableTask::add_output_impl(AbstractOutput* output, const WString& name,
                                const WString& description) {
    outputs_->item(name, description, 0, output);
}

AbstractRunner::AbstractRunner():
    state_(UNSET),
    task_(0),
    server_(WServer::instance()), session_id_(wApp->sessionId())
{ }

void AbstractRunner::run() {
    if (!(state() == UNSET || state() == WORKING)) {
        run_impl();
    }
}

void AbstractRunner::cancel() {
    if (state() == WORKING) {
        cancel_impl();
    }
    set_state(NEW);
}

RunState AbstractRunner::state() const {
    return task_ ? state_ : UNSET;
}

void AbstractRunner::finish() {
    set_state(FINISHED);
    if (task_->queue_) {
        task_->queue_->remove(task_);
    }
    server_->post(session_id_,
                  boost::bind(&AbstractTask::changed_emitter, task()));
}

void AbstractRunner::set_task(AbstractTask* task) {
    task_ = task;
    set_state(NEW);
}

ForkingRunner::ForkingRunner(const std::string& command):
    command_(command), pid_file_(FileOutput::unique_name())
{ }

ForkingRunner::~ForkingRunner() {
    if (state() == WORKING) {
        cancel_impl();
    }
    remove(pid_file_.c_str());
}

void ForkingRunner::run_impl() {
    if (state() == FINISHED) {
        set_state(NEW);
    }
    if (state() == NEW) {
        set_state(WORKING);
        boost::thread(&ForkingRunner::start_process, this);
    }
}

void ForkingRunner::cancel_impl() {
    std::stringstream cmd;
    cmd << "kill `cat " << pid_file_ << "`";
    system(cmd.str().c_str());
}

std::string ForkingRunner::escape_arg(const std::string& arg) {
    return std::string("'") + boost::replace_all_copy(arg, "'", "'\''") + "'";
}

void arg_to_stream(std::stringstream& stream, const std::string& arg,
                   bool escape) {
    stream << " ";
    if (escape) {
        stream << ForkingRunner::escape_arg(arg);
    } else {
        stream << arg;
    }
    stream << " ";
}

void ForkingRunner::start_process() {
    std::stringstream cmd;
    cmd << "echo $$ > " << pid_file_ << ";";
    cmd << command_ << " ";
    task()->visit_args(boost::bind(arg_to_stream, boost::ref(cmd), _1, _2));
    system(cmd.str().c_str());
    finish();
}

AbstractQueue::AbstractQueue(WObject* p):
    WObject(p), server_(0)
{ }

void AbstractQueue::add(AbstractTask* task) {
    mutex_.lock();
    task2session_[task] = wApp->sessionId();
    add_impl(task);
    mutex_.unlock();
}

void AbstractQueue::remove(AbstractTask* task) {
    mutex_.lock();
    remove_impl(task);
    task2session_.erase(task);
    mutex_.unlock();
}

void AbstractQueue::run_task(AbstractTask* task) {
    WServer* s = server_ ? server_ : WServer::instance();
    s->post(task2session_[task], boost::bind(&AbstractTask::run_impl, task));
}

TaskNumberQueue::TaskNumberQueue(int max_tasks, WObject* p):
    AbstractQueue(p), max_tasks_(max_tasks)
{ }

void TaskNumberQueue::add_impl(AbstractTask* task) {
    waiting_.push_back(task);
    set_queued(task);
    try_to_run();
}

void TaskNumberQueue::remove_impl(AbstractTask* task) {
    waiting_.remove(task);
    running_.erase(task);
    try_to_run();
}

void TaskNumberQueue::try_to_run() {
    if (!waiting_.empty()) {
        if (max_tasks_ == -1 || (int)running_.size() < max_tasks_) {
            AbstractTask* task = waiting_.front();
            waiting_.pop_front();
            set_queued(task, false);
            run_task(task);
            if (task->state() == WORKING) {
                running_.insert(task);
            }
        }
    }
}

}

}

