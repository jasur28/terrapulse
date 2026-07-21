import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Panel {
    id: root

    property var eventData: null
    property string reviewText: "AUTO"
    property color reviewColor: Theme.textSecondary

    signal confirmRequested()
    signal rejectRequested()

    title: "Operator review"
    subtitle: eventData ? ("Event #" + eventData.shfId) : "Select an event"

    ColumnLayout {
        anchors {
            fill: parent
            margins: 12
            topMargin: 48
        }
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: root.eventData ? root.eventData.evType : "No event selected"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
            elide: Text.ElideRight
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 6
            columnSpacing: 10

            Repeater {
                model: root.eventData ? [
                    "Object", root.eventData.objId,
                    "Sensor", root.eventData.senId,
                    "Severity", root.eventData.severity,
                    "Status", root.eventData.evStatus,
                    "Start", root.eventData.evTime,
                    "Duration", root.eventData.durText,
                    "Confidence", root.eventData.conf,
                    "Review", root.reviewText
                ] : []

                Text {
                    Layout.fillWidth: true
                    text: modelData
                    color: index % 2 === 0 ? Theme.textSecondary
                                            : (modelData === root.reviewText ? root.reviewColor : Theme.textPrimary)
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: index % 2 === 1 && modelData === root.reviewText
                    elide: Text.ElideRight
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                Layout.fillWidth: true
                enabled: root.eventData !== null
                text: "Confirm"
                onClicked: root.confirmRequested()
            }

            Button {
                Layout.fillWidth: true
                enabled: root.eventData !== null
                text: "Reject"
                onClicked: root.rejectRequested()
            }
        }
    }
}
