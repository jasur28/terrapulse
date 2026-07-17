import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import TerraPulse

Item {
    id: root

    ListModel { id: sensorModel }
    property var sensorIndex: ({})

    Connections {
        target: appController
        function onSafReceived(saf) {
            var key = saf.sensorId + "_" + saf.componentName
            var ts  = new Date(saf.timestamp).toLocaleTimeString(Qt.locale(), "hh:mm:ss")
            var entry = {
                senId:       saf.sensorId,
                objId:       saf.objectId,
                axis:        saf.componentName,
                statusText:  saf.warningLevelName,
                statusColor: saf.warningLevelColor,
                rms:         saf.rms.toFixed(2),
                battery:     95,
                temperature: "23.0",
                lastSeen:    ts
            }
            if (root.sensorIndex.hasOwnProperty(key)) {
                sensorModel.set(root.sensorIndex[key], entry)
            } else {
                root.sensorIndex[key] = sensorModel.count
                sensorModel.append(entry)
            }
        }
    }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        Text {
            text: "Sensors"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeTitle; font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true; height: 32
            color: Theme.navBg; radius: Theme.radiusSmall
            Row {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Repeater {
                    model: ["Sensor", "Object", "Axis", "Status", "RMS (mg)", "Battery %", "Temp °C", "Last Seen"]
                    Text {
                        width: parent.width / 8; height: parent.height
                        text: modelData; verticalAlignment: Text.AlignVCenter
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                }
            }
        }

        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            model: sensorModel; clip: true; spacing: 2

            delegate: Rectangle {
                width: ListView.view.width; height: 36
                color: index % 2 === 0 ? Theme.surface : Theme.surfaceAlt
                radius: Theme.radiusSmall

                Row {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }

                    Repeater {
                        model: [senId, objId, axis]
                        Text {
                            width: parent.width / 8; height: parent.height
                            text: modelData; verticalAlignment: Text.AlignVCenter
                            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    // Status cell with color
                    Text {
                        width: parent.width / 8; height: parent.height
                        text: statusText; verticalAlignment: Text.AlignVCenter
                        color: statusColor; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }

                    Repeater {
                        model: [rms, battery + "%", temperature + " °C", lastSeen]
                        Text {
                            width: parent.width / 8; height: parent.height
                            text: modelData; verticalAlignment: Text.AlignVCenter
                            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
