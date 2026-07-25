#pragma once

#include "terrapulse/core/record.h"

#include <QPointF>

#include <vector>

namespace tp::gui {

std::vector<QPointF> buildRecordPolyline(const tp::core::Record& record, double width, double height);

} // namespace tp::gui
