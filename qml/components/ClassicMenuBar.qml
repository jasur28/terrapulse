import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Rectangle {
    id: root

    property string moduleName: ""
    property var menus: ["File", "View", "Settings", "Help"]
    property string connectionText: "localhost"
    default property alias tools: toolRow.data

    height: 48
    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            spacing: 0

            Repeater {
                model: root.menus
                Rectangle {
                    Layout.preferredWidth: Math.max(54, label.implicitWidth + 18)
                    Layout.fillHeight: true
                    color: mouse.containsMouse ? "#dbe8f6" : "transparent"

                    Text {
                        id: label
                        anchors.centerIn: parent
                        text: modelData
                        color: Theme.textPrimary
                        font.pixelSize: 12
                    }

                    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                Layout.rightMargin: 8
                text: root.moduleName + "  " + root.connectionText
                color: Theme.textSecondary
                font.pixelSize: 11
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            spacing: 5

            RowLayout {
                id: toolRow
                spacing: 5
            }

            Item { Layout.fillWidth: true }
        }
    }
}
