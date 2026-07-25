#pragma once

#include "terrapulse/gui/core/gradient.h"
#include "terrapulse/gui/qt.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPen>

namespace tp::gui {

class TP_GUI_API Scheme {
public:
    struct Colors {
        QColor background{17, 18, 32};
        QColor surface{25, 27, 43};
        QColor panel{238, 238, 238};
        QColor text{230, 232, 238};
        QColor muted{152, 160, 176};
        QColor normal{20, 199, 114};
        QColor warning{255, 205, 33};
        QColor critical{255, 44, 85};
        QColor offline{110, 113, 121};

        struct Records {
            QColor background{12, 15, 26};
            QColor alternateBackground{19, 23, 36};
            QColor foreground{220, 228, 240};
            QPen grid = QPen(QColor(70, 78, 96), 1, Qt::DotLine);
            QBrush gaps{{255, 205, 33, 70}};
            QBrush overlaps{{255, 44, 85, 70}};
        } records;

        struct Map {
            QColor grid{255, 255, 255, 80};
            QColor annotationText{255, 255, 255};
            QColor annotationHalo{10, 13, 24};
            QColor selected{78, 148, 255};
            QColor normal{20, 199, 114};
            QColor warning{255, 205, 33};
            QColor critical{255, 44, 85};
            QColor offline{110, 113, 121};
        } map;

        struct QC {
            QColor ok{20, 199, 114};
            QColor delay{255, 205, 33};
            QColor error{255, 44, 85};
            QColor missing{110, 113, 121};
        } qc;
    };

    struct Fonts {
        QFont base;
        QFont small;
        QFont heading;
    };

    Colors colors;
    Fonts fonts;
};

} // namespace tp::gui
