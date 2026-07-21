import QtQuick
import QtQuick.Layouts
import TerraPulse

Panel {
    id: root

    property string label: ""
    property string value: ""
    property string detail: ""
    property color accent: Theme.colorService

    implicitWidth: 150
    implicitHeight: 70

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            radius: 2
            color: root.accent
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.label
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.value
                color: root.accent
                font.pixelSize: 22
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: root.detail.length > 0
                text: root.detail
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideRight
            }
        }
    }
}
