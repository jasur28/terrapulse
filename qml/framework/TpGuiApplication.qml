import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property string moduleName: ""
    property string connectionText: sessionQueue
    property string statusLeft: acq.connected ? "Connected to tpmaster" : "Disconnected"
    property string statusCenter: ""
    property string statusRight: ""
    property alias toolBarData: menuBar.tools
    property alias menus: menuBar.menus
    default property alias content: body.data

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ClassicMenuBar {
            id: menuBar
            Layout.fillWidth: true
            moduleName: root.moduleName
            connectionText: root.connectionText
        }

        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
        }

        ClassicStatusBar {
            Layout.fillWidth: true
            live: acq.connected
            leftText: root.statusLeft
            centerText: root.statusCenter
            rightText: root.statusRight
        }
    }
}
