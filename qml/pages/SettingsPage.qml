import QtQuick
import QtQuick.Layouts
import TerraPulse

Item {
    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 20

        Text {
            text: "Settings"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
        }

        // Analysis window size
        Rectangle {
            Layout.fillWidth: true
            color: Theme.surface; radius: Theme.radius
            border.color: Theme.border; border.width: 1
            implicitHeight: winCol.implicitHeight + 32

            Column {
                id: winCol
                anchors { fill: parent; margins: 16 }
                spacing: 10

                Text {
                    text: "Analysis Window"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeNormal; font.bold: true
                }

                Text {
                    text: "Configured in the tpproc daemon (launch with --window N). "
                        + "Remote tuning from the console arrives with TPCtrl."
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap; width: parent.width - 8
                }

                Text {
                    text: "Windows processed: " + appController.windowsProcessed
                    color: Theme.textPrimary; font.pixelSize: Theme.fontSizeNormal
                }
            }
        }

        // Threshold reference (read-only)
        Rectangle {
            Layout.fillWidth: true
            color: Theme.surface; radius: Theme.radius
            border.color: Theme.border; border.width: 1
            implicitHeight: threshCol.implicitHeight + 32

            Column {
                id: threshCol
                anchors { fill: parent; margins: 16 }
                spacing: 10

                Text {
                    text: "Active Thresholds"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeNormal; font.bold: true
                }

                GridLayout {
                    columns: 3; columnSpacing: 24; rowSpacing: 8
                    Repeater {
                        model: [
                            "Metric",         "WARNING",    "CRITICAL",
                            "RMS factor",     "× 2.0",      "× 4.0",
                            "Energy factor",  "× 3.0",      "× 6.0",
                            "Max amplitude",  "> 50 mg",    "> 100 mg",
                            "Freq. shift",    "> 5 Hz",     "> 15 Hz",
                            "Health index",   "< 0.6",      "< 0.3"
                        ]
                        Text {
                            text: modelData
                            color: index < 3 ? Theme.textSecondary : Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSmall
                            font.bold: index < 3
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
