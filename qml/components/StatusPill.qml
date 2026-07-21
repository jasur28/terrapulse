import QtQuick
import TerraPulse

Rectangle {
    id: root

    property string text: ""
    property color fill: Theme.colorOffline
    property color textColor: "#ffffff"

    implicitWidth: Math.max(label.implicitWidth + 16, 54)
    implicitHeight: 20
    radius: 3
    color: root.fill

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.textColor
        font.pixelSize: 10
        font.bold: true
        elide: Text.ElideRight
        width: parent.width - 8
        horizontalAlignment: Text.AlignHCenter
    }
}
