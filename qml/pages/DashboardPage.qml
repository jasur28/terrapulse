import QtQuick
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property int activeAlerts:  0
    property int criticalCount: 0

    // Alerts come from anomaly history (SHF); object health comes from the
    // inventory model (merged with live SAF).
    Connections {
        target: appController
        function onShfReceived(shf) {
            if (shf.anomalyStatus === 0) {
                root.activeAlerts++
                if (shf.severityLevel >= 2) root.criticalCount++
            } else if (shf.anomalyStatus === 1) {
                root.activeAlerts = Math.max(0, root.activeAlerts - 1)
            }
        }
    }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 20

        Text {
            text: "Dashboard"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeTitle; font.bold: true
        }

        // Summary tiles
        RowLayout {
            spacing: 12
            Repeater {
                model: [
                    { label: "Structures",    val: inventory.structureCount,    clr: "#00E5FF" },
                    { label: "Active Alerts", val: root.activeAlerts,            clr: "#FFD600" },
                    { label: "Critical",      val: root.criticalCount,           clr: "#FF1744" },
                    { label: "Windows",       val: appController.windowsProcessed, clr: "#A0A0A0" }
                ]
                Rectangle {
                    width: 160; height: 72
                    color: Theme.surface; radius: Theme.radius
                    border.color: Theme.border; border.width: 1
                    Column {
                        anchors.centerIn: parent; spacing: 4
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.val; color: modelData.clr
                            font.pixelSize: 26; font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label; color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        Text {
            text: "Monitored Structures"
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeNormal
        }

        // Object cards from the inventory model
        Flow {
            Layout.fillWidth: true; spacing: 16
            Repeater {
                model: inventory.structures
                ObjectCard {
                    objectId:     modelData.objectId
                    objectName:   modelData.name !== undefined ? modelData.name : ("Object " + modelData.objectId)
                    healthIndex:  modelData.hasData ? modelData.health : 1.0
                    warningLevel: modelData.warning
                    anomalyText:  ""
                }
            }
        }

        Text {
            visible: inventory.structureCount === 0
            text: "No structures configured. Load inventory:  tpinv --file config/inventory.example.json"
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
        }

        Item { Layout.fillHeight: true }
    }
}
