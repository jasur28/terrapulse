#pragma once

#include "terrapulse/gui/qt.h"

#include <algorithm>
#include <limits>

namespace tp::gui::plot {

class TP_GUI_API Range {
public:
    Range() = default;
    Range(double lower, double upper) : lower(lower), upper(upper), valid(true) {
        normalize();
    }

    bool isValid() const { return valid; }
    bool isEmpty() const { return !valid || lower == upper; }
    double length() const { return valid ? upper - lower : 0.0; }
    double center() const { return (lower + upper) * 0.5; }

    void reset();
    void normalize();
    void expand(double value);
    void merge(const Range& other);
    bool contains(double value) const;
    Range padded(double ratio) const;

    double lower = 0.0;
    double upper = 0.0;
    bool valid = false;
};

} // namespace tp::gui::plot
