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

    height: 42
    color: "#eeeeee"
    border.color: "#a6a6a6"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            spacing: 0

            Repeater {
                model: root.menus
                Rectangle {
                    Layout.preferredWidth: Math.max(48, label.implicitWidth + 16)
                    Layout.fillHeight: true
                    color: mouse.containsMouse ? "#d8eafa" : "transparent"

                    Text {
                        id: label
                        anchors.centerIn: parent
                        text: modelData
                        color: Theme.textPrimary
                        font.pixelSize: 11
                    }

                    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                Layout.rightMargin: 8
                text: root.moduleName + "  " + root.connectionText
                color: Theme.textSecondary
                font.pixelSize: 10
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#b9b9b9" }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            spacing: 3

            RowLayout {
                id: toolRow
                spacing: 3
            }

            Item { Layout.fillWidth: true }
        }
    }
}
