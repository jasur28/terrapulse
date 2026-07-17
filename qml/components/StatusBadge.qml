import QtQuick
import TerraPulse

Rectangle {
    id: root
    property int    level: 0        // 0=NORMAL 1=WARNING 2=CRITICAL -1=OFFLINE
    property string customText: ""
    width: 90; height: 22
    radius: Theme.radiusSmall

    color: {
        if (level === 2)  return Theme.colorCritical
        if (level === 1)  return Theme.colorWarning
        if (level === -1) return Theme.colorOffline
        return Theme.colorNormal
    }

    Text {
        anchors.centerIn: parent
        text: root.customText !== "" ? root.customText
              : (root.level === 2 ? "CRITICAL"
              : root.level === 1 ? "WARNING"
              : root.level === -1 ? "OFFLINE"
              : "NORMAL")
        color:     root.level === 1 ? "#000" : "#fff"
        font.pixelSize: Theme.fontSizeSmall
        font.bold: true
    }
}
