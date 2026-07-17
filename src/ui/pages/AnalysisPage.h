#pragma once
#include "core/SafRecord.h"
#include <QWidget>

namespace tp {

class AnalysisPage : public QWidget {
    Q_OBJECT
public:
    explicit AnalysisPage(QWidget* parent = nullptr);

public slots:
    void onSafReceived(const tp::SafRecord& saf);
};

} // namespace tp
