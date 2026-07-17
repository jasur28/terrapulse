import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import TerraPulse

Item {
    id: root

    ListModel { id: safModel }

    Connections {
        target: appController
        function onSafReceived(saf) {
            var dt = new Date(saf.timestamp)
            var ts = dt.toLocaleTimeString(Qt.locale(), "hh:mm:ss")
            safModel.insert(0, {
                "time":     ts,
                "objectId": saf.objectId,
                "sensorId": saf.sensorId,
                "axis":     saf.componentName,
                "rms":      saf.rms.toFixed(2),
                "maxAmp":   saf.maxAmplitude.toFixed(2),
                "domFreq":  saf.dominantFrequency.toFixed(1) + " Hz",
                "health":   saf.healthIndex.toFixed(3),
                "status":   saf.warningLevelName,
                "statusColor": saf.warningLevelColor
            })
            if (safModel.count > 1000) safModel.remove(safModel.count - 1)
        }
    }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        RowLayout {
            Text {
                text: "Analysis History"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: safModel.count + " records"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        // Header
        Rectangle {
            Layout.fillWidth: true
            height: 32
            color: Theme.navBg
            radius: Theme.radiusSmall

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 0
                Repeater {
                    model: ["Time", "Obj", "Sensor", "Axis", "RMS", "MaxAmp", "Dom.Freq", "Health", "Status"]
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: safModel
            clip: true
            spacing: 1

            delegate: Rectangle {
                width: ListView.view.width
                height: 32
                color: index % 2 === 0 ? Theme.surface : Theme.surfaceAlt

                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    spacing: 0

                    Repeater {
                        model: [time, objectId, sensorId, axis, rms, maxAmp, domFreq, health]
                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }

                    // Status badge
                    Rectangle {
                        Layout.preferredWidth: 72
                        height: 18; radius: 3
                        color: statusColor
                        Text {
                            anchors.centerIn: parent
                            text: status
                            color: status === "WARNING" ? "#000" : "#fff"
                            font.pixelSize: 10; font.bold: true
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
