import QtQuick
import QtQuick.Layouts
import TerraPulse

Window {
    id: root

    width: 1280
    height: 760
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "TerraPulse - Structural Health Monitor"
           + (sessionQueue === "playback" ? "   PLAYBACK REVIEW (tpolv)" : "")
    color: Theme.background

    readonly property var navItems: [
        { label: "Dashboard",  icon: "D" },
        { label: "Monitoring", icon: "R" },
        { label: "Map",        icon: "M" },
        { label: "Objects",    icon: "O" },
        { label: "Sensors",    icon: "S" },
        { label: "Analysis",   icon: "A" },
        { label: "Events",     icon: "E" },
        { label: "Settings",   icon: "C" }
    ]
    property int currentPage: 0

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: Theme.navWidth
            Layout.fillHeight: true
            color: Theme.navBg

            Rectangle {
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: 1
                color: Theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    color: "transparent"

                    Rectangle {
                        anchors {
                            bottom: parent.bottom
                            left: parent.left
                            right: parent.right
                        }
                        height: 1
                        color: Theme.border
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "TerraPulse"
                        color: Theme.colorService
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                Repeater {
                    model: root.navItems

                    NavButton {
                        Layout.fillWidth: true
                        label: modelData.label
                        iconChar: modelData.icon
                        active: root.currentPage === index
                        onClicked: root.currentPage = index
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: "transparent"

                    Rectangle {
                        anchors {
                            top: parent.top
                            left: parent.left
                            right: parent.right
                        }
                        height: 1
                        color: Theme.border
                    }

                    Row {
                        anchors {
                            left: parent.left
                            leftMargin: 16
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: 8

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: acq.connected ? Theme.colorNormal : Theme.colorOffline
                        }

                        Text {
                            text: acq.connected ? "Live" : "Offline"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPage

            DashboardPage {}
            MonitoringPage {}
            MapPage {}
            ObjectsPage {}
            SensorsPage {}
            AnalysisPage {}
            EventsPage {}
            SettingsPage {}
        }
    }
}
