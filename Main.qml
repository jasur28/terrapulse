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

    function pageForView(v) {
        if (v === "dashboard" || v === "full") return 0
        if (v === "tprttv" || v === "scrttv") return 1
        if (v === "tpmap" || v === "map") return 2
        if (v === "objects") return 3
        if (v === "sensors") return 4
        if (v === "analysis") return 5
        if (v === "tpolv" || v === "scolv" || v === "events") return 6
        if (v === "settings") return 7
        return 0
    }

    Component.onCompleted: currentPage = pageForView(appView)

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            visible: !singleView
            Layout.preferredWidth: singleView ? 0 : Theme.navWidth
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

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                anchors.fill: parent
                asynchronous: true
                sourceComponent: {
                    if (singleView) {
                        if (appView === "tpmap" || appView === "map") return tpmapAppComponent
                        if (appView === "tprttv" || appView === "scrttv") return tprttvAppComponent
                        if (appView === "tpolv" || appView === "scolv") return tpolvAppComponent
                    }
                    switch (root.currentPage) {
                    case 0: return dashboardComponent
                    case 1: return monitoringComponent
                    case 2: return mapComponent
                    case 3: return objectsComponent
                    case 4: return sensorsComponent
                    case 5: return analysisComponent
                    case 6: return eventsComponent
                    case 7: return settingsComponent
                    default: return dashboardComponent
                    }
                }
            }
        }
    }

    Component { id: dashboardComponent; DashboardPage {} }
    Component { id: monitoringComponent; MonitoringPage {} }
    Component { id: mapComponent; MapPage {} }
    Component { id: objectsComponent; ObjectsPage {} }
    Component { id: sensorsComponent; SensorsPage {} }
    Component { id: analysisComponent; AnalysisPage {} }
    Component { id: eventsComponent; EventsPage {} }
    Component { id: settingsComponent; SettingsPage {} }
    Component { id: tpmapAppComponent; TpMapApp {} }
    Component { id: tprttvAppComponent; TpRttvApp {} }
    Component { id: tpolvAppComponent; TpOlvApp {} }
}
