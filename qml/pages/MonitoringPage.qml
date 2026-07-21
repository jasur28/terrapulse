import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property real baseTime: -1
    property real latestRelT: 0
    property int maxPoints: 1600
    property real windowSecs: 60

    property var ptsX: []
    property var ptsY: []
    property var ptsZ: []
    property var ptsH: []
    property var spectrum: []
    property real domFreq: 0
    property bool filterEnabled: false

    function streamName() {
        return acq.station + "." + acq.object + "." + acq.sensor
    }

    function trimPoints() {
        while (ptsX.length > maxPoints) ptsX.shift()
        while (ptsY.length > maxPoints) ptsY.shift()
        while (ptsZ.length > maxPoints) ptsZ.shift()
        while (ptsH.length > maxPoints) ptsH.shift()
    }

    function computeSpectrum() {
        var pts = root.ptsX
        var N = Math.min(128, pts.length)
        if (N < 16) {
            root.spectrum = []
            root.domFreq = 0
            return
        }

        var v = []
        var mean = 0
        var i
        for (i = pts.length - N; i < pts.length; i++) {
            v.push(pts[i].v)
            mean += pts[i].v
        }
        mean /= N
        for (i = 0; i < N; i++) v[i] -= mean

        var fs = acq.sampleRate > 0 ? acq.sampleRate : 200
        var bins = Math.floor(N / 2)
        var spec = []
        var maxMag = 1e-9
        var domF = 0

        for (var k = 1; k < bins; k++) {
            var re = 0
            var im = 0
            for (var n = 0; n < N; n++) {
                var ang = -2 * Math.PI * k * n / N
                re += v[n] * Math.cos(ang)
                im += v[n] * Math.sin(ang)
            }
            var mag = Math.sqrt(re * re + im * im) / N
            var f = k * fs / N
            spec.push({ f: f, mag: mag })
            if (mag > maxMag) {
                maxMag = mag
                domF = f
            }
        }

        root.spectrum = spec
        root.domFreq = domF
    }

    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: root.computeSpectrum()
    }

    Connections {
        target: appController
        function onSafReceived(saf) {
            if (saf.component !== 2 || root.baseTime < 0) return
            var relT = saf.timestamp / 1000.0 - root.baseTime
            root.ptsH.push({ t: relT, v: saf.healthIndex })
            root.trimPoints()
            healthPlot.points = root.ptsH
        }
    }

    Connections {
        target: acq

        function onConnectedChanged() {
            if (!acq.connected) return
            root.ptsX = []
            root.ptsY = []
            root.ptsZ = []
            root.ptsH = []
            root.spectrum = []
            root.baseTime = -1
            root.latestRelT = 0
            healthPlot.points = []
        }

        function onSampleReceived(sample) {
            var t = sample.timestampMs / 1000.0
            if (root.baseTime < 0) root.baseTime = t
            var relT = t - root.baseTime

            root.ptsX.push({ t: relT, v: sample.x })
            root.ptsY.push({ t: relT, v: sample.y })
            root.ptsZ.push({ t: relT, v: sample.z })
            root.latestRelT = relT
            root.trimPoints()

            traceView.pointsX = root.ptsX
            traceView.pointsY = root.ptsY
            traceView.pointsZ = root.ptsZ
            traceView.latestTime = root.latestRelT
        }
    }

    ColumnLayout {
        anchors {
            fill: parent
            margins: 20
        }
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: "tprttv"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }

                Text {
                    text: "Real-time trace view"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            StatusPill {
                text: acq.connected ? "LIVE" : "OFFLINE"
                fill: acq.connected ? Theme.colorNormal : Theme.colorOffline
                textColor: acq.connected ? "#001d0b" : "#ffffff"
            }

            Button {
                text: root.filterEnabled ? "Filter on" : "Filter off"
                checkable: true
                checked: root.filterEnabled
                onToggled: root.filterEnabled = checked
            }

            ComboBox {
                model: ["60 s", "120 s", "300 s"]
                currentIndex: 0
                onActivated: {
                    root.windowSecs = currentIndex === 0 ? 60 : currentIndex === 1 ? 120 : 300
                    traceView.windowSecs = root.windowSecs
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            spacing: 10

            MetricTile {
                Layout.fillWidth: true
                label: "Stream"
                value: root.streamName()
                detail: acq.endpoint
                accent: Theme.colorService
            }

            MetricTile {
                Layout.fillWidth: true
                label: "Sample rate"
                value: acq.sampleRate.toFixed(0) + " Hz"
                detail: acq.packetCount + " samples"
                accent: Theme.colorNormal
            }

            MetricTile {
                Layout.fillWidth: true
                label: "Current X"
                value: acq.lastX.toFixed(2)
                detail: "gal"
                accent: Theme.seriesX
            }

            MetricTile {
                Layout.fillWidth: true
                label: "Current Y"
                value: acq.lastY.toFixed(2)
                detail: "gal"
                accent: Theme.seriesY
            }

            MetricTile {
                Layout.fillWidth: true
                label: "Current Z"
                value: acq.lastZ.toFixed(2)
                detail: "gal"
                accent: Theme.seriesZ
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 520
                spacing: 12

                TraceView {
                    id: traceView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 360
                    windowSecs: root.windowSecs
                    latestTime: root.latestRelT
                    pointsX: root.ptsX
                    pointsY: root.ptsY
                    pointsZ: root.ptsZ
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    spacing: 12

                    SpectrumPlot {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spectrum: root.spectrum
                        dominantFrequency: root.domFreq
                        sampleRate: acq.sampleRate > 0 ? acq.sampleRate : 200
                    }

                    TrendPlot {
                        id: healthPlot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "Health index"
                        lineColor: Theme.colorNormal
                        fixedMin: 0
                        fixedMax: 1
                        bands: true
                        decimals: 2
                    }
                }
            }

            Panel {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                title: "Streams"
                subtitle: "Inventory channels"

                ListView {
                    anchors {
                        fill: parent
                        margins: 10
                        topMargin: 46
                    }
                    model: inventory.sensors
                    clip: true
                    spacing: 4

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 52
                        radius: Theme.radiusSmall
                        color: modelData.hasData ? Theme.surfaceAlt : Theme.navBg
                        border.color: modelData.hasData ? Theme.border : "transparent"
                        border.width: 1

                        Column {
                            anchors {
                                fill: parent
                                margins: 8
                            }
                            spacing: 2

                            Text {
                                width: parent.width
                                text: modelData.objectId + "." + modelData.sensorId
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                width: parent.width
                                text: (modelData.model !== undefined ? modelData.model : "sensor") +
                                      "  health " + (modelData.hasData ? Math.round(modelData.health * 100) + "%" : "n/a")
                                color: modelData.hasData ? Theme.textSecondary : Theme.colorOffline
                                font.pixelSize: Theme.fontSizeSmall
                                elide: Text.ElideRight
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }
    }
}
