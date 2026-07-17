import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import TerraPulse

Item {
    id: root

    ListModel { id: eventsModel }

    Connections {
        target: appController
        function onShfReceived(shf) {
            var ts = new Date(shf.anomalyStartTime).toLocaleTimeString(Qt.locale(), "hh:mm:ss")
            eventsModel.insert(0, {
                evTime:    ts,
                objId:     shf.objectId,
                senId:     shf.sensorId,
                axis:      shf.componentName,
                evType:    shf.anomalyTypeName,
                severity:  shf.severityName,
                sevColor:  shf.severityColor,
                duration:  shf.anomalyDuration > 0 ? shf.anomalyDuration + " s" : "ongoing",
                evStatus:  shf.statusName
            })
            if (eventsModel.count > 500) eventsModel.remove(eventsModel.count - 1)
        }
    }

    readonly property var colHeaders: ["Time", "Object", "Sensor", "Axis", "Type", "Severity", "Duration", "Status"]

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        RowLayout {
            Text {
                text: "Event Log"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeTitle; font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: eventsModel.count + " events"
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
            }
        }

        // Header row
        Rectangle {
            Layout.fillWidth: true; height: 32
            color: Theme.navBg; radius: Theme.radiusSmall
            Row {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Repeater {
                    model: root.colHeaders
                    Text {
                        width: parent.width / root.colHeaders.length; height: parent.height
                        text: modelData; verticalAlignment: Text.AlignVCenter
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                }
            }
        }

        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            model: eventsModel; clip: true; spacing: 2

            delegate: Rectangle {
                width: ListView.view.width; height: 36
                color: index % 2 === 0 ? Theme.surface : Theme.surfaceAlt
                radius: Theme.radiusSmall

                Row {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    property real colW: width / root.colHeaders.length

                    // Time, Object, Sensor, Axis, Type
                    Repeater {
                        model: [evTime, objId, senId, axis, evType]
                        Text {
                            width: parent.colW; height: parent.height
                            text: modelData; verticalAlignment: Text.AlignVCenter
                            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }

                    // Severity — colored badge
                    Item {
                        width: parent.colW; height: parent.height
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 72; height: 20; radius: 3
                            color: sevColor
                            Text {
                                anchors.centerIn: parent
                                text: severity
                                color: (severity === "MEDIUM" || severity === "LOW") ? "#000" : "#fff"
                                font.pixelSize: 10; font.bold: true
                            }
                        }
                    }

                    // Duration, Status
                    Repeater {
                        model: [duration, evStatus]
                        Text {
                            width: parent.colW; height: parent.height
                            text: modelData; verticalAlignment: Text.AlignVCenter
                            color: index === 1 && evStatus === "ACTIVE"
                                   ? Theme.colorWarning : Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
