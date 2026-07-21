import QtQuick
import QtQuick.Layouts
import TerraPulse

Item {
    function statusText(w)  { return w >= 2 ? "CRITICAL" : w >= 1 ? "WARNING" : "NORMAL" }
    function statusColor(w) { return w >= 2 ? "#FF1744"  : w >= 1 ? "#FFD600"  : "#00C853" }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Monitored Objects"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeTitle; font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: inventory.structureCount + " objects · " + inventory.sensorCount + " sensors"
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
            }
        }

        Rectangle {
            Layout.fillWidth: true; height: 32
            color: Theme.navBg; radius: Theme.radiusSmall
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Repeater {
                    model: ["ID", "Name", "Description", "Sensors", "Ch.", "Health", "Status"]
                    Text {
                        Layout.fillWidth: true
                        text: modelData; color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                }
            }
        }

        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 6
            model: inventory.structures

            delegate: Rectangle {
                width: ListView.view.width; height: 36
                color: Theme.surface; radius: Theme.radiusSmall
                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    Repeater {
                        model: [
                            String(modelData.objectId),
                            modelData.name !== undefined ? modelData.name : "—",
                            modelData.description !== undefined ? modelData.description : "—",
                            String(modelData.sensors),
                            String(modelData.channels),
                            modelData.hasData ? Math.round(modelData.health * 100) + "%" : "—"
                        ]
                        Text {
                            Layout.fillWidth: true
                            text: modelData; color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSmall; elide: Text.ElideRight
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.hasData ? statusText(modelData.warning) : "—"
                        color: modelData.hasData ? statusColor(modelData.warning) : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                }
            }
        }

        Text {
            visible: inventory.structureCount === 0
            text: "No inventory yet. Load it with:  tpinv --file config/inventory.example.json"
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
        }
    }
}
