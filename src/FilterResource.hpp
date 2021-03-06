/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef WC_DOT_RESOURCE_HPP_
#define WC_DOT_RESOURCE_HPP_

#include <fstream>
#include "boost-xtime.hpp"
#include <boost/thread/mutex.hpp>

#include <Wt/WFileResource>

namespace Wt {

namespace Wc {

/** Abstract resource, streaming data through an external program.
Temporary output file is created using \c system().
The command called must be available.

Example (graphviz dot wrapper):
\code
class DotResource : public Wt::Wc::FilterResource {
public:
    DotResource(Wt::WObject* parent = 0):
        Wt::Wc::FilterResource("dot -Tpng {1} -o {2}", parent) {
        setMimeType("image/png");
    }

    void write_input(std::ostream& out) const {
        out << "graph {" << std::endl;
        out << "a -- b -- c;" << std::endl;
        out << "}" << std::endl;
    }
};
\endcode

\ingroup bindings
*/
class FilterResource : public WFileResource {
public:
    /** Constructor.
    \param cmd The command to be executed.
        Should contain two positional arguments:
        {1} -- input file; {2} -- output file.
    \param parent The parent.
    */
    FilterResource(const WString& cmd, WObject* parent = 0);

    /** Destructor.
    Delete output file
    */
    ~FilterResource();

    /** Method writing input file for the external command.
    The method is called at most once (unless update() is called).
    All exceptions thrown by this function are caught in handleRequest().
    */
    virtual void write_input(std::ostream& out) const = 0;

    /** Handles a request.
    Output file is lazily created here.
    */
    void handleRequest(const Http::Request& request, Http::Response& response);

    /** Mark current output file as outdated.
    \param cal_set_changed If WResource::setChanged() will be called.

    Next handleRequest() will call write_input() again.
    This may be usefull, if write_input() uses a parameter,
    which value can be changed, which makes current output out-dated.
    */
    void update(bool cal_set_changed = true);

private:
    WString cmd_;
    std::string output_file_;
    boost::mutex mutex_;
};

}

}

#endif

