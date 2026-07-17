import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property real baseTime:   -1
    property real latestRelT:  0
    readonly property real windowSecs: 60
    readonly property int  maxPoints:  1200

    // Raw point arrays: [{t: relativeSeconds, v: value}, ...]
    property var ptsX: []
    property var ptsY: []
    property var ptsZ: []
    property var ptsH: []

    // Analysis output feeds only the Health chart. The acceleration chart is
    // driven purely by the raw stream below.
    Connections {
        target: appController
        function onSafReceived(saf) {
            if (saf.component !== 2) return     // health logged once per window (Z tick)
            if (root.baseTime < 0) return       // wait for the raw stream to anchor the timeline
            var relT = saf.timestamp / 1000.0 - root.baseTime
            root.ptsH.push({t: relT, v: saf.healthIndex})
            if (root.ptsH.length > root.maxPoints) root.ptsH.shift()
            healthCanvas.requestPaint()
        }
    }

    Connections {
        target: acq

        function onConnectedChanged() {
            if (acq.connected) {
                // Fresh stream — clear any stale chart data
                root.ptsX = []; root.ptsY = []; root.ptsZ = []
                root.ptsH = []; root.baseTime = -1; root.latestRelT = 0
                rmsCanvas.requestPaint(); healthCanvas.requestPaint()
            }
        }

        function onSampleReceived(sample) {
            var t = sample.timestampMs / 1000.0
            if (root.baseTime < 0) root.baseTime = t
            var relT = t - root.baseTime

            root.ptsX.push({t: relT, v: sample.x})
            root.ptsY.push({t: relT, v: sample.y})
            root.ptsZ.push({t: relT, v: sample.z})
            while (root.ptsX.length > root.maxPoints) root.ptsX.shift()
            while (root.ptsY.length > root.maxPoints) root.ptsY.shift()
            while (root.ptsZ.length > root.maxPoints) root.ptsZ.shift()

            root.latestRelT = relT
            rmsCanvas.requestPaint()
        }
    }

    // Repaint when navigating back to this page
    onVisibleChanged: {
        if (visible) {
            rmsCanvas.requestPaint()
            healthCanvas.requestPaint()
        }
    }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        Text {
            text: "Live Monitoring"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeTitle; font.bold: true
        }

        // ── Acquisition status (read-only; tpacq owns the serial port) ──────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: streamPanel.implicitHeight + 24
            color: Theme.surface
            radius: Theme.radius
            border.color: acq.connected ? Theme.colorService : Theme.border
            border.width: 1

            ColumnLayout {
                id: streamPanel
                anchors { fill: parent; margins: 12 }
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Acquisition (tpacq)"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                    }

                    Rectangle {
                        width: 9; height: 9; radius: 4.5
                        color: acq.connected ? Theme.colorNormal : Theme.colorOffline
                    }

                    Text {
                        text: acq.connected ? "Streaming" : "No data"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: acq.endpoint
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 18

                    Text {
                        text: "Sensor " + acq.station + "." + acq.object + "." + acq.sensor
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    }
                    Text {
                        text: acq.sampleRate.toFixed(0) + " Hz"
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    }
                    Text {
                        text: "Samples: " + acq.packetCount
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: acq.lastTimestamp
                        color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 18

                    Text { text: "X: " + acq.lastX.toFixed(4) + " gal"; color: Theme.seriesX; font.pixelSize: Theme.fontSizeSmall }
                    Text { text: "Y: " + acq.lastY.toFixed(4) + " gal"; color: Theme.seriesY; font.pixelSize: Theme.fontSizeSmall }
                    Text { text: "Z: " + acq.lastZ.toFixed(4) + " gal"; color: Theme.seriesZ; font.pixelSize: Theme.fontSizeSmall }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ── RMS chart ──────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            color: Theme.surface; radius: Theme.radius
            border.color: Theme.border; border.width: 1

            // Title + legend
            Row {
                x: 52; y: 8; spacing: 16
                Text {
                    text: "Real-time Acceleration (gal)"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall; font.bold: true
                }
                Repeater {
                    model: [
                        {label: "X", clr: Theme.seriesX},
                        {label: "Y", clr: Theme.seriesY},
                        {label: "Z", clr: Theme.seriesZ}
                    ]
                    Row {
                        spacing: 4
                        Rectangle {
                            width: 16; height: 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: modelData.clr
                        }
                        Text {
                            text: modelData.label
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }

            Canvas {
                id: rmsCanvas
                anchors { fill: parent; topMargin: 28; leftMargin: 4; rightMargin: 8; bottomMargin: 4 }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var pL = 44, pR = 4, pT = 6, pB = 22
                    var pw = width - pL - pR
                    var ph = height - pT - pB
                    if (pw <= 0 || ph <= 0) return

                    var tEnd   = root.latestRelT + 2
                    var tStart = Math.max(0, root.latestRelT - root.windowSecs)
                    var tRange = Math.max(tEnd - tStart, 1)

                    // Auto Y-scale around signed acceleration.
                    var yMin = -10.0
                    var yMax =  10.0
                    var allArr = [root.ptsX, root.ptsY, root.ptsZ]
                    for (var ai = 0; ai < 3; ai++) {
                        var a = allArr[ai]
                        for (var pi = 0; pi < a.length; pi++) {
                            if (a[pi].t >= tStart) {
                                yMin = Math.min(yMin, a[pi].v)
                                yMax = Math.max(yMax, a[pi].v)
                            }
                        }
                    }
                    var padY = Math.max((yMax - yMin) * 0.10, 1.0)
                    yMin -= padY
                    yMax += padY
                    var yRange = Math.max(yMax - yMin, 1.0)

                    function tx(t) { return pL + (t - tStart) / tRange * pw }
                    function ty(v) { return pT + ph - (v - yMin) / yRange * ph }

                    // Grid
                    var gridN = 4
                    for (var gi = 0; gi <= gridN; gi++) {
                        var gy = pT + gi * ph / gridN
                        var gv = yMax - gi * yRange / gridN
                        ctx.strokeStyle = Theme.border
                        ctx.lineWidth = 1
                        ctx.beginPath(); ctx.moveTo(pL, gy); ctx.lineTo(pL + pw, gy); ctx.stroke()
                        ctx.fillStyle = Theme.textSecondary
                        ctx.font = "10px sans-serif"
                        ctx.textAlign = "right"
                        ctx.fillText(gv.toFixed(1), pL - 4, gy + 4)
                    }

                    // X labels
                    ctx.fillStyle = Theme.textSecondary
                    ctx.font = "10px sans-serif"
                    ctx.textAlign = "center"
                    for (var xi = 0; xi <= 4; xi++) {
                        var xt = tStart + xi * tRange / 4
                        ctx.fillText(Math.round(xt) + "s", tx(xt), pT + ph + 15)
                    }

                    // Clip and draw series
                    ctx.save()
                    ctx.beginPath(); ctx.rect(pL, pT, pw, ph); ctx.clip()

                    var series = [
                        {pts: root.ptsX, color: Theme.seriesX},
                        {pts: root.ptsY, color: Theme.seriesY},
                        {pts: root.ptsZ, color: Theme.seriesZ}
                    ]
                    for (var si = 0; si < 3; si++) {
                        var pts = series[si].pts
                        if (pts.length < 2) continue
                        ctx.beginPath()
                        ctx.strokeStyle = series[si].color
                        ctx.lineWidth = 1.5
                        var moved = false
                        for (var pi2 = 0; pi2 < pts.length; pi2++) {
                            var px = tx(pts[pi2].t), py = ty(pts[pi2].v)
                            if (!moved) { ctx.moveTo(px, py); moved = true }
                            else        ctx.lineTo(px, py)
                        }
                        ctx.stroke()
                    }
                    ctx.restore()

                    // Axis frame
                    ctx.strokeStyle = Theme.border; ctx.lineWidth = 1
                    ctx.beginPath(); ctx.moveTo(pL, pT); ctx.lineTo(pL, pT + ph); ctx.stroke()
                    ctx.beginPath(); ctx.moveTo(pL, pT + ph); ctx.lineTo(pL + pw, pT + ph); ctx.stroke()
                }
            }
        }

        // ── Health index chart ─────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface; radius: Theme.radius
            border.color: Theme.border; border.width: 1

            Text {
                x: 52; y: 8
                text: "Health Index"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall; font.bold: true
            }

            Canvas {
                id: healthCanvas
                anchors { fill: parent; topMargin: 28; leftMargin: 4; rightMargin: 8; bottomMargin: 4 }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var pL = 44, pR = 4, pT = 6, pB = 22
                    var pw = width - pL - pR
                    var ph = height - pT - pB
                    if (pw <= 0 || ph <= 0) return

                    var tEnd   = root.latestRelT + 2
                    var tStart = Math.max(0, root.latestRelT - root.windowSecs)
                    var tRange = Math.max(tEnd - tStart, 1)

                    function tx(t) { return pL + (t - tStart) / tRange * pw }
                    function ty(v) { return pT + ph - v * ph }   // 0..1 maps to ph..0

                    // Threshold bands
                    ctx.globalAlpha = 0.08
                    ctx.fillStyle = Theme.colorNormal;   ctx.fillRect(pL, pT,         pw, ty(0.6) - pT)
                    ctx.fillStyle = Theme.colorWarning;  ctx.fillRect(pL, ty(0.6),    pw, ty(0.3) - ty(0.6))
                    ctx.fillStyle = Theme.colorCritical; ctx.fillRect(pL, ty(0.3),    pw, pT + ph - ty(0.3))
                    ctx.globalAlpha = 1.0

                    // Grid
                    var yLabels = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0]
                    for (var li = 0; li < yLabels.length; li++) {
                        var lv = yLabels[li]
                        var gy = ty(lv)
                        ctx.strokeStyle = Theme.border; ctx.lineWidth = 1
                        ctx.beginPath(); ctx.moveTo(pL, gy); ctx.lineTo(pL + pw, gy); ctx.stroke()
                        ctx.fillStyle = Theme.textSecondary
                        ctx.font = "10px sans-serif"; ctx.textAlign = "right"
                        ctx.fillText(lv.toFixed(1), pL - 4, gy + 4)
                    }

                    // X labels
                    ctx.fillStyle = Theme.textSecondary
                    ctx.font = "10px sans-serif"; ctx.textAlign = "center"
                    for (var xi = 0; xi <= 4; xi++) {
                        var xt = tStart + xi * tRange / 4
                        ctx.fillText(Math.round(xt) + "s", tx(xt), pT + ph + 15)
                    }

                    ctx.save()
                    ctx.beginPath(); ctx.rect(pL, pT, pw, ph); ctx.clip()

                    // Health line — colour changes with value
                    var pts = root.ptsH
                    if (pts.length >= 2) {
                        ctx.lineWidth = 2
                        for (var i = 1; i < pts.length; i++) {
                            var h = pts[i].v
                            ctx.strokeStyle = (h >= 0.6) ? Theme.colorNormal
                                            : (h >= 0.3) ? Theme.colorWarning
                                            :               Theme.colorCritical
                            ctx.beginPath()
                            ctx.moveTo(tx(pts[i-1].t), ty(pts[i-1].v))
                            ctx.lineTo(tx(pts[i].t),   ty(pts[i].v))
                            ctx.stroke()
                        }
                    }

                    ctx.restore()

                    // Axis frame
                    ctx.strokeStyle = Theme.border; ctx.lineWidth = 1
                    ctx.beginPath(); ctx.moveTo(pL, pT); ctx.lineTo(pL, pT + ph); ctx.stroke()
                    ctx.beginPath(); ctx.moveTo(pL, pT + ph); ctx.lineTo(pL + pw, pT + ph); ctx.stroke()
                }
            }
        }
    }
}
