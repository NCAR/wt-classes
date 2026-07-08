/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include "IntervalWidget.hpp"

namespace Wt {

namespace Wc {

IntervalWidget::IntervalWidget(std::unique_ptr<WWidget> min, std::unique_ptr<WWidget> max):
    WTemplate(tr("wc.time.interval_template")) {
    bindWidget("min", std::move(min));
    bindWidget("max", std::move(max));
}

}

}
