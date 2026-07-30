import QtQuick
import QtQuick.Controls
import TerraPulse

Button {
    id: control

    property bool danger: false
    property bool success: false

    implicitHeight: 24
    implicitWidth: Math.max(28, contentItem.implicitWidth + 14)
    padding: 0
    leftPadding: 7
    rightPadding: 7
    spacing: 5

    font.pixelSize: 11
    font.bold: false
    palette.buttonText: Theme.textPrimary

    contentItem: Text {
        text: control.text
        color: !control.enabled ? "#8e8e8e"
              : control.danger ? "#8a0000"
              : control.success ? "#006800"
              : Theme.textPrimary
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: 24
        radius: 1
        color: !control.enabled ? "#e7e7e7"
             : control.checked ? "#c8dff4"
             : control.down ? "#c6c6c6"
             : control.hovered ? "#f8f8f8"
             : "#e2e2e2"
        border.color: control.checked ? "#4aa3df" : "#a8a8a8"
        border.width: 1
    }
}
