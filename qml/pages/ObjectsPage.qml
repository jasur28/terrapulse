import QtQuick
import QtQuick.Layouts
import TerraPulse

Item {
    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        Text {
            text: "Monitored Objects"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
        }

        // Header row
        Rectangle {
            Layout.fillWidth: true; height: 32
            color: Theme.navBg; radius: Theme.radiusSmall
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Repeater {
                    model: ["ID", "Name", "Type", "Location", "Status", "Health Index"]
                    Text {
                        Layout.fillWidth: true
                        text: modelData; color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                }
            }
        }

        // Static demo row for Bridge #1
        Rectangle {
            Layout.fillWidth: true; height: 36
            color: Theme.surface; radius: Theme.radiusSmall
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Repeater {
                    model: ["1", "Bridge #1", "Bridge", "Site A", "NORMAL", "—"]
                    Text {
                        Layout.fillWidth: true
                        text: modelData; color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
