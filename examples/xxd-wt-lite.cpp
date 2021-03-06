
#include <Wt/WApplication>
#include <Wt/Wc/Wbi.hpp>
#include <Wt/Wc/util.hpp>

using namespace Wt;
using namespace Wt::Wc;

class XxdLiteApp : public WApplication {
public:
    XxdLiteApp(const WEnvironment& env):
        WApplication(env) {
        enableUpdates(true);
        messageResourceBundle().use(approot() + "locales/wtclasses");
        TableTask* task = new TableTask(root());
        task->add_input(new FileInput("<"), "Binary file");
        task->add_output(new ViewFileOutput(">"), "Hex dump");
        task->set_runner(new ForkingRunner("sleep 2; xxd", "; sleep 3"));
        new TaskCountup(task, root());
    }
};

WApplication* createXxdLiteApp(const WEnvironment& env) {
    return new XxdLiteApp(env);
}

int main(int argc, char** argv) {
    return WRun(argc, argv, &createXxdLiteApp);
}

