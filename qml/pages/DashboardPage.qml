import QtQuick
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    // Use properties with explicit change signals for live updates
    property real  bridgeHealth:   1.0
    property int   bridgeLevel:    0
    property string bridgeAnomaly: ""
    property int   activeAlerts:   0
    property int   criticalCount:  0

    Connections {
        target: appController
        function onSafReceived(saf) {
            if (saf.objectId !== 1) return
            // Update card when any axis has anomaly, keep worst health
            if (saf.healthIndex < root.bridgeHealth || saf.warningLevel > root.bridgeLevel) {
                root.bridgeHealth  = saf.healthIndex
                root.bridgeLevel   = saf.warningLevel
                root.bridgeAnomaly = saf.anomalyDetected
                                    ? saf.anomalyTypeName + " · " + saf.componentName
                                    : ""
            }
        }
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
                model: ListModel {
                    ListElement { label: "Structures";     val: "1"; clr: "#00E5FF" }
                    ListElement { label: "Active Alerts";  val: "0"; clr: "#FFD600" }
                    ListElement { label: "Critical";       val: "0"; clr: "#FF1744" }
                }
                Rectangle {
                    width: 160; height: 72
                    color: Theme.surface; radius: Theme.radius
                    border.color: Theme.border; border.width: 1

                    Column {
                        anchors.centerIn: parent; spacing: 4
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: index === 0 ? "1"
                                : index === 1 ? root.activeAlerts
                                : root.criticalCount
                            color: clr
                            font.pixelSize: 26; font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: label; color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }

            Rectangle {
                width: 160; height: 72
                color: Theme.surface; radius: Theme.radius
                border.color: Theme.border; border.width: 1
                Column {
                    anchors.centerIn: parent; spacing: 4
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: appController.windowsProcessed
                        color: Theme.textSecondary; font.pixelSize: 26; font.bold: true
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Windows"; color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        Text {
            text: "Monitored Structures"
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeNormal
        }

        // Object cards
        Flow {
            Layout.fillWidth: true; spacing: 16
            ObjectCard {
                objectId:    1
                objectName:  "Bridge #1"
                healthIndex:  root.bridgeHealth
                warningLevel: root.bridgeLevel
                anomalyText:  root.bridgeAnomaly
            }
        }

        Item { Layout.fillHeight: true }
    }
}
