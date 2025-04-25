/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <Wt/WDateTime.h>
#include <sstream>

#include "TimeDuration.hpp"
#include "rand.hpp"

namespace Wt {

namespace Wc {

namespace td {

namespace ptime = std::chrono;

TimeDuration::TimeDuration():
    std::chrono::seconds(0)
{ }

// TimeDuration::TimeDuration(const std::chrono::seconds& duration):
//     ptime::duration(duration)
// { }

TimeDuration::operator std::string() const {
    std::ostringstream os;
    os << count() << " s";
    return os.str();
}

// TimeDuration TimeDuration::operator +(const TimeDuration& b) const {
//     return static_cast<const Base&>(*this) + static_cast<const Base&>(b);
// }

// TimeDuration TimeDuration::operator -(const TimeDuration& b) const {
//     return static_cast<const Base&>(*this) - static_cast<const Base&>(b);
// }

// TimeDuration TimeDuration::operator -() const {
//     return TimeDuration() - *this;
// }

// TimeDuration TimeDuration::operator /(const double& b) const {
//     return ptime::milliseconds(long(total_milliseconds() / b));
// }

// TimeDuration TimeDuration::operator /(int b) const {
//     return static_cast<const Base&>(*this) / b;
// }

// TimeDuration TimeDuration::operator *(const double& b) const {
//     return ptime::milliseconds(long(total_milliseconds() * b));
// }

// TimeDuration TimeDuration::operator *(int b) const {
//     return static_cast<const Base&>(*this) * b;
// }

// double TimeDuration::operator /(const TimeDuration& b) const {
//     return double(total_milliseconds()) / double(b.total_milliseconds());
// }

// long TimeDuration::total_minutes() const {
//     return total_seconds() / 60;
// }

// TimeDuration operator -(const WDateTime& a, const WDateTime& b) {
//     return a.toPosixTime() - b.toPosixTime();
// }

// WDateTime operator +(const WDateTime& a, const TimeDuration& b) {
//     return WDateTime::fromPosixTime(a.toPosixTime() +
//                                     static_cast<const TimeDuration::Base&>(b));
// }

// WDateTime operator -(const WDateTime& a, const TimeDuration& b) {
//     return a + (-b);
// }

// WDateTime& operator +=(WDateTime& a, const TimeDuration& b) {
//     return a = a + b;
// }

// WDateTime& operator -=(WDateTime& a, const TimeDuration& b) {
//     return a = a - b;
// }

// TimeDuration operator *(const double& b, const TimeDuration& a) {
//     return a * b;
// }
// 
// TimeDuration rand_range(const TimeDuration& start, const TimeDuration& stop) {
//     unsigned int start_int = start.total_milliseconds();
//     unsigned int stop_int = stop.total_milliseconds();
//     return ptime::milliseconds(rr(start_int, stop_int));
// }

}

}

}

