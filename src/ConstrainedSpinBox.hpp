/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef WC_CONSTRAINED_SPIN_BOX_HPP_
#define WC_CONSTRAINED_SPIN_BOX_HPP_

#include <Wt/WGlobal.h>
#include "config.hpp"

#ifdef WC_HAVE_WSPINBOX
#include <Wt/WSpinBox.h>
#endif

#ifdef WC_HAVE_WDOUBLESPINBOX
#include <Wt/WDoubleSpinBox.h>
#endif

namespace Wt {

namespace Wc {

#ifdef WC_HAVE_WSPINBOX
/** Spin box with constrained value.

\ingroup util
*/
class ConstrainedSpinBox : public WSpinBox {
public:
    /** Constructor */
    ConstrainedSpinBox();

    /** Return the corrected value.
    Same as value(), but it is guaranteed to lie in [minimum(), maximum()].
    */
    int corrected_value() const;
};
#endif

#ifdef WC_HAVE_WDOUBLESPINBOX
/** Double spin box with constrained value.

\ingroup util
*/
class ConstrainedDoubleSpinBox : public WDoubleSpinBox {
public:
    /** Constructor */
    ConstrainedDoubleSpinBox();

    /** Return the corrected value.
    Same as value(), but it is guaranteed to lie in [minimum(), maximum()].
    */
    double corrected_value() const;
};
#endif

}

}

#endif
