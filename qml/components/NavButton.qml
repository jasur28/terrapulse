import QtQuick
import TerraPulse

Rectangle {
    id: root
    property string label:   ""
    property bool   active:  false
    property string iconChar: ""

    signal clicked()

    width: parent.width; height: 44
    color: active ? Theme.accent : (hover.containsMouse ? "#1A2440" : "transparent")

    Rectangle {
        width: 3; height: parent.height
        color: root.active ? Theme.colorService : "transparent"
    }

    Row {
        anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
        spacing: 10
        Text {
            text: root.iconChar
            color: root.active ? Theme.colorService : Theme.textSecondary
            font.pixelSize: 16
        }
        Text {
            text: root.label
            color: root.active ? Theme.colorService : Theme.textSecondary
            font.pixelSize: Theme.fontSizeNormal
            font.bold: root.active
        }
    }

    HoverHandler { id: hover }
    TapHandler   { onTapped: root.clicked() }
}
