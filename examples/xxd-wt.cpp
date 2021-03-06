/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <boost/bind.hpp>

#include <Wt/WApplication>
#include <Wt/WIntValidator>
#include <Wt/WLineEdit>
#include <Wt/WCheckBox>
#include <Wt/WFileUpload>
#include <Wt/Wc/Wbi.hpp>
#include <Wt/Wc/util.hpp>

using namespace Wt;
using namespace Wt::Wc;

TaskNumberQueue queue(2);

bool validate(AbstractTask* task, WCheckBox* bits, WCheckBox* ps) {
    if (bits->isChecked() && ps->isChecked()) {
        task->set_message("Postscript style can not be mixed with Bits style");
        return false;
    } else {
        task->set_message("");
        return true;
    }
}

std::string name_gen(FileInput* input) {
    std::string input_file = input->file_upload()->clientFileName().toUTF8();
    if (input_file.empty()) {
        input_file = "example.txt";
    }
    input_file += ".xxd";
    return input_file;
}

class XxdApp : public WApplication {
public:
    XxdApp(const WEnvironment& env):
        WApplication(env) {
        enableUpdates(true);
        messageResourceBundle().use(approot() + "locales/wtclasses");
        TableTask* task = new TableTask(root());
        FileInput* input = new FileInput("<");
        task->add_input(input, "Binary file");
        LineEditInput* cols_input = new LineEditInput(0);
        cols_input->set_option_name("-c");
        WLineEdit* cols = new WLineEdit("16");
        cols->setValidator(new WIntValidator(1, 256));
        cols_input->set_widget(cols);
        task->add_input(cols_input, "Octets per line");
        WCheckBox* bits = new WCheckBox();
        task->add_input(new BoolInput(bits, "-b"), "Bits");
        WCheckBox* ps = new WCheckBox();
        task->add_input(new BoolInput(ps, "-ps"), "Postscript style");
        ViewFileOutput* output = new ViewFileOutput(">");
        output->set_suggested_gen(boost::bind(name_gen, input));
        task->add_output(output, "Hex dump");
        task->set_runner(new ForkingRunner("sleep 2 && xxd", " && sleep 3"));
        task->set_queue(&queue);
        bits->checked().connect(boost::bind(&WLineEdit::setText, cols, "6"));
        bits->unChecked().connect(boost::bind(&WLineEdit::setText, cols, "16"));
        ps->checked().connect(boost::bind(&WLineEdit::setText, cols, "30"));
        ps->unChecked().connect(boost::bind(&WLineEdit::setText, cols, "16"));
        task->set_validator(boost::bind(validate, task, bits, ps));
        new TaskCountup(task, root());
    }
};

WApplication* createXxdApp(const WEnvironment& env) {
    return new XxdApp(env);
}

int main(int argc, char** argv) {
    return WRun(argc, argv, &createXxdApp);
}

