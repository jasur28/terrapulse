#pragma once

#include <QString>

namespace tp::datamodel::strongmotion {

enum class Component {
    X,
    Y,
    Z,
    Horizontal1,
    Horizontal2,
    Vertical,
    RotationX,
    RotationY,
    RotationZ
};

inline QString componentName(Component component) {
    switch (component) {
    case Component::X: return "X";
    case Component::Y: return "Y";
    case Component::Z: return "Z";
    case Component::Horizontal1: return "H1";
    case Component::Horizontal2: return "H2";
    case Component::Vertical: return "V";
    case Component::RotationX: return "RX";
    case Component::RotationY: return "RY";
    case Component::RotationZ: return "RZ";
    }
    return {};
}

} // namespace tp::datamodel::strongmotion
