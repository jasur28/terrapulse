import QtQuick
import TerraPulse

Panel {
    id: root

    property var pointsX: []
    property var pointsY: []
    property var pointsZ: []
    property real latestTime: 0
    property real windowSecs: 60
    property string unit: "gal"

    title: "Waveform"
    subtitle: "X / Y / Z acceleration"

    function requestPaint() { canvas.requestPaint() }
    onPointsXChanged: canvas.requestPaint()
    onPointsYChanged: canvas.requestPaint()
    onPointsZChanged: canvas.requestPaint()
    onLatestTimeChanged: canvas.requestPaint()

    Row {
        anchors {
            right: parent.right
            top: parent.top
            margins: 12
        }
        spacing: 14

        Repeater {
            model: [
                { label: "X", color: Theme.seriesX },
                { label: "Y", color: Theme.seriesY },
                { label: "Z", color: Theme.seriesZ }
            ]

            Row {
                spacing: 5
                Rectangle {
                    width: 18
                    height: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: modelData.color
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
        id: canvas
        anchors {
            fill: parent
            topMargin: 40
            leftMargin: 6
            rightMargin: 8
            bottomMargin: 6
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var pL = 48, pR = 8, pT = 8, pB = 24
            var pw = width - pL - pR
            var ph = height - pT - pB
            if (pw <= 0 || ph <= 0) return

            var tEnd = root.latestTime + 1
            var tStart = Math.max(0, root.latestTime - root.windowSecs)
            var tRange = Math.max(tEnd - tStart, 1)
            var arrays = [root.pointsX, root.pointsY, root.pointsZ]
            var yMin = -10.0
            var yMax = 10.0

            for (var ai = 0; ai < arrays.length; ai++) {
                var a = arrays[ai] || []
                for (var pi = 0; pi < a.length; pi++) {
                    if (a[pi].t >= tStart) {
                        yMin = Math.min(yMin, a[pi].v)
                        yMax = Math.max(yMax, a[pi].v)
                    }
                }
            }

            var pad = Math.max((yMax - yMin) * 0.10, 1.0)
            yMin -= pad
            yMax += pad
            var yRange = Math.max(yMax - yMin, 1.0)

            function tx(t) { return pL + (t - tStart) / tRange * pw }
            function ty(v) { return pT + ph - (v - yMin) / yRange * ph }

            ctx.fillStyle = Theme.textSecondary
            ctx.font = "10px sans-serif"

            for (var g = 0; g <= 4; g++) {
                var gy = pT + g * ph / 4
                var gv = yMax - g * yRange / 4
                ctx.strokeStyle = Theme.border
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(pL, gy)
                ctx.lineTo(pL + pw, gy)
                ctx.stroke()
                ctx.textAlign = "right"
                ctx.fillText(gv.toFixed(1), pL - 5, gy + 4)
            }

            ctx.textAlign = "center"
            for (var xi = 0; xi <= 5; xi++) {
                var xt = tStart + xi * tRange / 5
                ctx.fillText(Math.round(xt) + "s", tx(xt), pT + ph + 17)
            }

            ctx.save()
            ctx.beginPath()
            ctx.rect(pL, pT, pw, ph)
            ctx.clip()

            var series = [
                { pts: root.pointsX || [], color: Theme.seriesX },
                { pts: root.pointsY || [], color: Theme.seriesY },
                { pts: root.pointsZ || [], color: Theme.seriesZ }
            ]

            for (var si = 0; si < series.length; si++) {
                var pts = series[si].pts
                if (pts.length < 2) continue
                ctx.beginPath()
                ctx.strokeStyle = series[si].color
                ctx.lineWidth = 1.4
                var moved = false
                for (var i = 0; i < pts.length; i++) {
                    if (pts[i].t < tStart) continue
                    var x = tx(pts[i].t)
                    var y = ty(pts[i].v)
                    if (!moved) {
                        ctx.moveTo(x, y)
                        moved = true
                    } else {
                        ctx.lineTo(x, y)
                    }
                }
                ctx.stroke()
            }

            ctx.restore()

            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(pL, pT)
            ctx.lineTo(pL, pT + ph)
            ctx.lineTo(pL + pw, pT + ph)
            ctx.stroke()

            ctx.textAlign = "left"
            ctx.fillText(root.unit, pL, pT - 1)
        }
    }
}
