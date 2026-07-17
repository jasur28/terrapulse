import QtQuick
import TerraPulse

Rectangle {
    id: root
    property int    objectId:    1
    property string objectName:  "Object"
    property real   healthIndex: 1.0
    property int    warningLevel: 0
    property string anomalyText: ""

    width: 200; height: 220
    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    border.width: 1

    Column {
        anchors { fill: parent; margins: 16 }
        spacing: 10

        Text {
            text: root.objectName
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
        }

        Text {
            text: "ID: " + root.objectId
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeSmall
        }

        HealthRing {
            anchors.horizontalCenter: parent.horizontalCenter
            value: root.healthIndex
        }

        StatusBadge {
            anchors.horizontalCenter: parent.horizontalCenter
            level: root.warningLevel
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.anomalyText !== "" ? root.anomalyText : "Structural integrity OK"
            color: root.warningLevel > 0 ? Theme.colorWarning : Theme.textSecondary
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }
}
