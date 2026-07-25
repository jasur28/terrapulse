import QtQuick
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property int activeAlerts: 0
    property int criticalCount: 0

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
        anchors { fill: parent; margins: 20 }
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: "Dashboard"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }
                Text {
                    text: "System summary and monitored structures"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            spacing: 10

            MetricTile { Layout.fillWidth: true; label: "Structures"; value: inventory.structureCount; detail: inventory.sensorCount + " sensors"; accent: Theme.colorService }
            MetricTile { Layout.fillWidth: true; label: "Active alerts"; value: root.activeAlerts; detail: "open anomalies"; accent: Theme.colorWarning }
            MetricTile { Layout.fillWidth: true; label: "Critical"; value: root.criticalCount; detail: "high severity"; accent: Theme.colorCritical }
            MetricTile { Layout.fillWidth: true; label: "Windows"; value: appController.windowsProcessed; detail: "processed"; accent: Theme.textSecondary }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Monitored structures"
            subtitle: "Virtualized list, safe for hundreds of objects"

            GridView {
                anchors { fill: parent; margins: 10; topMargin: 46 }
                model: inventory.structures
                clip: true
                cellWidth: 220
                cellHeight: 120
                cacheBuffer: 240

                delegate: Item {
                    width: GridView.view.cellWidth
                    height: GridView.view.cellHeight

                    ObjectCard {
                        anchors {
                            fill: parent
                            margins: 6
                        }
                        objectId: modelData.objectId
                        objectName: modelData.name !== undefined ? modelData.name : ("Object " + modelData.objectId)
                        healthIndex: modelData.hasData ? modelData.health : 1.0
                        warningLevel: modelData.warning
                        anomalyText: ""
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: inventory.structureCount === 0
                text: "No structures configured. Load inventory with tpinv."
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}
