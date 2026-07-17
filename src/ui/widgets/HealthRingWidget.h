#pragma once
#include <QWidget>

namespace tp {

// Circular progress ring showing health index 0..1.
// Green > 0.6, Yellow 0.3..0.6, Red < 0.3
class HealthRingWidget : public QWidget {
    Q_OBJECT
public:
    explicit HealthRingWidget(QWidget* parent = nullptr);
    void setValue(float value);  // 0.0 – 1.0
    float value() const { return m_value; }
    QSize sizeHint() const override { return {100, 100}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    float m_value = 1.0f;
};

} // namespace tp
