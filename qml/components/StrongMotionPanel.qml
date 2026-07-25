import QtQuick
import QtQuick.Layouts
import TerraPulse

// Strong-motion summary: how hard the structure was shaken. PGA/PGV are the
// engineering measures, and the JMA instrumental intensity (計測震度) is the
// single number an operator can judge at a glance — the structural analog of a
// magnitude, on the Japanese shindo scale used for strong-motion monitoring.
Panel {
    id: root

    property real pga: 0        // gal
    property real pgv: 0        // cm/s
    property real psaMax: 0     // gal
    property real psaPeriod: 0  // s
    property real jma: 0        // instrumental intensity
    property string jmaScale: "0"
    property bool hasData: false

    title: "Strong motion"
    subtitle: "PGA / PGV / seismic intensity"

    // Shindo colouring: green through to red as shaking becomes damaging.
    function shindoColor(i) {
        if (i < 2.5) return Theme.colorNormal
        if (i < 4.5) return Theme.colorService
        if (i < 5.5) return Theme.colorWarning
        return Theme.colorCritical
    }

    Text {
        anchors.centerIn: parent
        visible: !root.hasData
        text: "Waiting for tpwfparam…"
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSmall
    }

    RowLayout {
        anchors { fill: parent; topMargin: 46; leftMargin: 14; rightMargin: 14; bottomMargin: 12 }
        spacing: 16
        visible: root.hasData

        // Intensity is the headline figure.
        Rectangle {
            Layout.preferredWidth: 118
            Layout.fillHeight: true
            radius: Theme.radiusSmall
            color: Theme.navBg
            border.color: root.shindoColor(root.jma)
            border.width: 2

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 0
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "震度 " + root.jmaScale
                    color: root.shindoColor(root.jma)
                    font.pixelSize: 30
                    font.bold: true
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "JMA " + root.jma.toFixed(2)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "seismic intensity"
                    color: Theme.textSecondary
                    font.pixelSize: 10
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            rowSpacing: 8
            columnSpacing: 12

            MetricTile {
                Layout.fillWidth: true; Layout.fillHeight: true
                label: "PGA"; value: root.pga.toFixed(1); detail: "gal"
                accent: root.pga > 100 ? Theme.colorCritical
                       : root.pga > 50  ? Theme.colorWarning : Theme.colorNormal
            }
            MetricTile {
                Layout.fillWidth: true; Layout.fillHeight: true
                label: "PGV"; value: root.pgv.toFixed(2); detail: "cm/s"
                accent: Theme.colorService
            }
            MetricTile {
                Layout.fillWidth: true; Layout.fillHeight: true
                label: "Peak PSA"; value: root.psaMax.toFixed(0); detail: "gal"
                accent: Theme.colorService
            }
            MetricTile {
                Layout.fillWidth: true; Layout.fillHeight: true
                label: "at period"; value: root.psaPeriod.toFixed(2); detail: "s"
                accent: Theme.colorService
            }
        }
    }
}
