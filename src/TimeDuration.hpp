/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef WC_TIME_DURATION_HPP_
#define WC_TIME_DURATION_HPP_

#include <string>
#include <chrono>

#include <Wt/WGlobal.h>

/** \defgroup time Time-related
Classes and typedef simplifying time management.
*/

/** Namespace for time duration constants and integration with WDateTime.
\ingroup time
*/
namespace Wt::Wc::td {

/** Time duration.

To integrate this type into Wt::Dbo see
Wt::Dbo::sql_value_traits< Wc::td::TimeDuration, void >.

\ingroup time
*/
class TimeDuration : public std::chrono::duration<double, std::chrono::seconds::period>{

public:
	// Explicitly inherit all constructors from our base duration class. Most
    // importantly, this gives us the TimeDuration(double seconds = 0.0) constructor.
	using std::chrono::duration<double, std::chrono::seconds::period>::duration;

    // Return a TimeDuration of zero seconds.
    static constexpr TimeDuration zero() { return TimeDuration(0.0); };

    /**  Copy construction from TimeDuration */
    TimeDuration(const TimeDuration& other) = default;

    /**  Copy construction from duration<double, std::chrono::seconds::period> */
    TimeDuration(const std::chrono::duration<double, std::chrono::seconds::period>& other) {
        *this = other;
    }

    /** Convert to string */
    operator std::string() const;


    /** Return the duration in whole minutes */
    long total_minutes() const;

    /** Return the duration in whole seconds */
    long total_seconds() const;

    /** Return the duration in whole milliseconds */
    long total_milliseconds() const;

    /** Return the duration in whole nanoseconds */
    long total_nanoseconds() const;

};
/** Return time duration between two datetimes.

\ingroup time
*/
TimeDuration operator -(const WDateTime& a, const WDateTime& b);

/** Increase the datetime by the time duration.

\ingroup time
*/
WDateTime operator +(const WDateTime& a, const TimeDuration& b);

/** Decrease the datetime by the time duration.

\ingroup time
*/
WDateTime operator -(const WDateTime& a, const TimeDuration& b);

/** Increase the datetime by the time duration.

\ingroup time
*/
WDateTime& operator +=(WDateTime& a, const TimeDuration& b);

/** Decrease the datetime by the time duration.

\ingroup time
*/
WDateTime& operator -=(WDateTime& a, const TimeDuration& b);

/** Multiply the datetime.

\ingroup time
*/
/** Return random time duration from open interval [min, max).

\ingroup time
*/
TimeDuration rand_range(const TimeDuration& start, const TimeDuration& stop);

TimeDuration operator *(const TimeDuration& a, const double& b);

TimeDuration operator *(const TimeDuration& a, const long& b);

/** Null time duration.
Valid time duration, 0 seconds.

\ingroup time
*/
const TimeDuration TD_NULL(0);

/** One second.

\ingroup time
*/
const TimeDuration SECOND(1);

/** One minute.

\ingroup time
*/
const TimeDuration MINUTE(60.0 * SECOND);

/** One hour.

\ingroup time
*/
const TimeDuration HOUR(60.0 * MINUTE);

/** One day.

\ingroup time
*/
const TimeDuration DAY(24.0 * HOUR);

/** One week.

\ingroup time
*/
const TimeDuration WEEK(7.0 * DAY);


}   // namespace Wt::Wc::td

#endif

