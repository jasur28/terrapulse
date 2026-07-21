import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import TerraPulse

Item {
    id: root

    ListModel { id: safModel }

    // Numeric trend history (Z axis = component 2 → one clean series per sensor).
    property var    hist: []
    property real   t0: -1
    property string selKey: "All"
    property var sensorKeys: {
        var a = ["All"]
        var s = inventory.sensors
        for (var i = 0; i < s.length; i++) a.push(s[i].objectId + "." + s[i].sensorId)
        return a
    }

    // Rebuild the three trend series from history, filtered by the selected sensor,
    // and hand them to the reusable TrendPlot components.
    function rebuildTrends() {
        var hp = [], fp = [], rp = []
        for (var i = 0; i < root.hist.length; i++) {
            var p = root.hist[i]
            if (root.selKey === "All" || p.key === root.selKey) {
                hp.push({ t: p.t, v: p.h }); fp.push({ t: p.t, v: p.f }); rp.push({ t: p.t, v: p.r })
            }
        }
        healthPlot.points = hp; freqPlot.points = fp; rmsPlot.points = rp
    }

    Connections {
        target: appController
        function onSafReceived(saf) {
            var ts = new Date(saf.timestamp).toLocaleTimeString(Qt.locale(), "hh:mm:ss")
            safModel.insert(0, {
                "time": ts, "objectId": saf.objectId, "sensorId": saf.sensorId, "axis": saf.componentName,
                "rms": saf.rms.toFixed(2), "maxAmp": saf.maxAmplitude.toFixed(2),
                "domFreq": saf.dominantFrequency.toFixed(1) + " Hz", "health": saf.healthIndex.toFixed(3),
                "status": saf.warningLevelName, "statusColor": saf.warningLevelColor
            })
            if (safModel.count > 1000) safModel.remove(safModel.count - 1)

            if (saf.component === 2) {
                var t = saf.timestamp / 1000.0
                if (root.t0 < 0) root.t0 = t
                root.hist.push({ t: t - root.t0, key: saf.objectId + "." + saf.sensorId,
                                 h: saf.healthIndex, f: saf.dominantFrequency, r: saf.rms })
                if (root.hist.length > 3000) root.hist.shift()
            }
        }
    }

    Timer { interval: 500; running: true; repeat: true; onTriggered: root.rebuildTrends() }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Analysis"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeTitle; font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text { text: "Sensor:"; color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall }
            ComboBox {
                model: root.sensorKeys
                implicitWidth: 130
                onActivated: { root.selKey = currentText; root.rebuildTrends() }
            }
            Text {
                text: safModel.count + " records"
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
            }
        }

        // Trend charts — reusable TrendPlot components (tpgui)
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 170
            spacing: 14

            TrendPlot {
                id: healthPlot
                Layout.fillWidth: true; Layout.fillHeight: true
                title: "Health index"; lineColor: "#00E5FF"
                fixedMin: 0; fixedMax: 1; bands: true; decimals: 2
            }
            TrendPlot {
                id: freqPlot
                Layout.fillWidth: true; Layout.fillHeight: true
                title: "Dominant frequency (Hz)"; lineColor: "#FFD600"; decimals: 1
            }
            TrendPlot {
                id: rmsPlot
                Layout.fillWidth: true; Layout.fillHeight: true
                title: "RMS"; lineColor: "#00C853"; decimals: 1
            }
        }

        // Records table
        Rectangle {
            Layout.fillWidth: true; height: 30
            color: Theme.navBg; radius: Theme.radiusSmall
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Repeater {
                    model: ["Time", "Obj", "Sensor", "Axis", "RMS", "MaxAmp", "Dom.Freq", "Health", "Status"]
                    Text {
                        Layout.fillWidth: true; text: modelData
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                }
            }
        }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            model: safModel; clip: true; spacing: 1
            delegate: Rectangle {
                width: ListView.view.width; height: 30
                color: index % 2 === 0 ? Theme.surface : Theme.surfaceAlt
                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    Repeater {
                        model: [time, objectId, sensorId, axis, rms, maxAmp, domFreq, health]
                        Text {
                            Layout.fillWidth: true; text: modelData
                            color: Theme.textPrimary; font.pixelSize: Theme.fontSizeSmall; elide: Text.ElideRight
                        }
                    }
                    Rectangle {
                        Layout.preferredWidth: 72; height: 18; radius: 3; color: statusColor
                        Text {
                            anchors.centerIn: parent; text: status
                            color: status === "WARNING" ? "#000" : "#fff"
                            font.pixelSize: 10; font.bold: true
                        }
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
