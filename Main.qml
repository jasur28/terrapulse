import QtQuick
import QtQuick.Layouts
import TerraPulse

Window {
    id: root
    width: 1280; height: 760
    minimumWidth: 960; minimumHeight: 600
    visible: true
    title: "TerraPulse — Structural Health Monitor"
    color: Theme.background

    // ── Navigation labels + page mapping ────────────────────────────────────
    readonly property var navItems: [
        { label: "Dashboard",  icon: "⬡" },
        { label: "Monitoring", icon: "📈" },
        { label: "Objects",    icon: "🏗" },
        { label: "Sensors",    icon: "📡" },
        { label: "Analysis",   icon: "🔬" },
        { label: "Events",     icon: "⚠" },
        { label: "Settings",   icon: "⚙" }
    ]
    property int currentPage: 0

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Sidebar ─────────────────────────────────────────────────────────
        Rectangle {
            width: Theme.navWidth
            Layout.fillHeight: true
            color: Theme.navBg

            // Right border
            Rectangle {
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                width: 1; color: Theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Logo
                Rectangle {
                    Layout.fillWidth: true
                    height: 56
                    color: "transparent"

                    Rectangle {
                        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                        height: 1; color: Theme.border
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "TerraPulse"
                        color: Theme.colorService
                        font.pixelSize: 16
                        font.bold: true
                        font.letterSpacing: 1.2
                    }
                }

                // Nav buttons
                Repeater {
                    model: root.navItems
                    NavButton {
                        Layout.fillWidth: true
                        label:    modelData.label
                        iconChar: modelData.icon
                        active:   root.currentPage === index
                        onClicked: root.currentPage = index
                    }
                }

                Item { Layout.fillHeight: true }

                // Status indicator
                Rectangle {
                    Layout.fillWidth: true
                    height: 48
                    color: "transparent"

                    Rectangle {
                        anchors { top: parent.top; left: parent.left; right: parent.right }
                        height: 1; color: Theme.border
                    }

                    Row {
                        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                        spacing: 8
                        Rectangle {
                            width: 8; height: 8; radius: 4
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

        // ── Page content area ────────────────────────────────────────────────
        StackLayout {
            id: pageStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPage

            DashboardPage  {}
            MonitoringPage {}
            ObjectsPage    {}
            SensorsPage    {}
            AnalysisPage   {}
            EventsPage     {}
            SettingsPage   {}
        }
    }
}
