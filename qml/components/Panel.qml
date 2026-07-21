import QtQuick
import TerraPulse

Rectangle {
    id: root

    property string title: ""
    property string subtitle: ""
    property bool highlighted: false

    color: Theme.surface
    radius: Theme.radius
    border.color: highlighted ? Theme.colorService : Theme.border
    border.width: 1

    Text {
        id: titleText
        visible: root.title.length > 0
        x: 12
        y: 8
        text: root.title
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeNormal
        font.bold: true
        elide: Text.ElideRight
        width: parent.width - 24
    }

    Text {
        visible: root.subtitle.length > 0
        anchors {
            left: parent.left
            leftMargin: 12
            top: titleText.visible ? titleText.bottom : parent.top
            topMargin: titleText.visible ? 2 : 8
        }
        text: root.subtitle
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSmall
        elide: Text.ElideRight
        width: parent.width - 24
    }
}
