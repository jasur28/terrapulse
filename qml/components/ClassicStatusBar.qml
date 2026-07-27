import QtQuick
import QtQuick.Layouts
import TerraPulse

Rectangle {
    id: root

    property string leftText: ""
    property string centerText: ""
    property string rightText: ""
    property bool live: false

    height: 23
    color: "#eeeeee"
    border.color: "#b0b0b0"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
            radius: 5
            color: root.live ? Theme.colorNormal : Theme.colorOffline
        }

        Text {
            Layout.preferredWidth: 280
            text: root.leftText
            color: Theme.textPrimary
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: root.centerText
            color: "#333333"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            Layout.preferredWidth: 320
            text: root.rightText
            color: Theme.textPrimary
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }
    }
}
