/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef WC_RESOURCE_VIEW_HPP_
#define WC_RESOURCE_VIEW_HPP_

#include <memory>
#include <Wt/WGlobal.h>

#include "StreamView.hpp"

namespace Wt {

namespace Wc {

/** View of a resource.

\ingroup util
*/
class ResourceView : public StreamView {
public:
    /** Constructor */
    ResourceView(WResource* resource);

    /** Constructor */
    ResourceView();

    /** Set resource */
    void set_resource(WResource* resource);

protected:
    std::unique_ptr<WWidget> renderView() override;

private:
    WResource* resource_;
};

}

}

#endif
