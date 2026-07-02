/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <Wt/WDateTime.h>

#include "TimeDuration.hpp"
#include "rand.hpp"

namespace Wt::Wc::td {


TimeDuration::operator std::string() const {
		std::string sign = (count() < 0) ? "-" : "";
		auto duration_secs = std::fabs(count());
		std::ostringstream os;
		long hours = duration_secs / 3600;
		long minutes = long(duration_secs) % 3600 / 60;
		long seconds = long(duration_secs) % 60;
		auto subseconds = fmod(duration_secs, 1.0);

		os << sign << std::setw(2) << std::setfill('0') << hours << ":"
		   << std::setw(2) << std::setfill('0') << minutes << ":"
		   << std::setw(2) << std::setfill('0') << seconds;
		if (subseconds > 0) {
			os << "." << std::setw(9) << std::setfill('0') << int(subseconds * 1e9);
		}
		return os.str();
}


long TimeDuration::total_minutes() const {
    return total_seconds() / 60;
}

long TimeDuration::total_seconds() const {
    return static_cast<long>(count());
}

long TimeDuration::total_milliseconds() const {
    return static_cast<long>(count() * 1000);
}

long TimeDuration::total_nanoseconds() const {
    return static_cast<long>(count() * 1e9);
}

// TimeDuration operator -(const WDateTime& a, const WDateTime& b) {
//     // @TODO NEEDS IMPLEMENTATION
// }
TimeDuration operator -(const WDateTime& a, const WDateTime& b) {
    return TimeDuration(b.secsTo(a));
}
WDateTime operator +(const WDateTime& a, const TimeDuration& b) {
    return(a.addSecs(b.count()));
}

WDateTime operator -(const WDateTime& a, const TimeDuration& b) {
    return a - b;
}

WDateTime& operator +=(WDateTime& a, const TimeDuration& b) {
    return a = a + b;
}

WDateTime& operator -=(WDateTime& a, const TimeDuration& b) {
    return a = a - b;
}

TimeDuration rand_range(const TimeDuration& start, const TimeDuration& stop) {
    unsigned int start_int = fabs(start.count() * 1000); // to integer milliseconds
    unsigned int stop_int = fabs(stop.count() * 1000);   // to integer milliseconds
    return TimeDuration(rr(start_int, stop_int) * 0.001); // milliseconds back to seconds
}

TimeDuration operator *(const TimeDuration& a, const double& b) {
	return b * a;
};

TimeDuration operator *(const TimeDuration& a, const long& b) {
	return b * a;
};

}   // namespace Wt::Wc::td
