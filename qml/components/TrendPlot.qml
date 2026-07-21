import QtQuick
import TerraPulse

// TrendPlot — reusable time-series line chart (part of the shared "tpgui"
// component set). Give it points [{t, v}] and it draws itself: auto or fixed
// Y-scale, optional health bands, title, grid. Views compose these instead of
// hand-rolling a Canvas each time.
Item {
    id: control

    property var    points: []            // [{ t: seconds, v: value }]
    property color  lineColor: Theme.colorService
    property var    fixedMin: undefined   // set both for a fixed Y-scale; else auto
    property var    fixedMax: undefined
    property bool   bands: false          // health-style 0.6 / 0.3 background bands
    property string title: ""
    property int    decimals: 1

    function requestPaint() { canvas.requestPaint() }
    onPointsChanged: canvas.requestPaint()

    Rectangle {
        anchors.fill: parent
        color: Theme.surface; radius: Theme.radius
        border.color: Theme.border; border.width: 1

        Text {
            x: 44; y: 6; text: control.title
            visible: control.title.length > 0
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall; font.bold: true
        }

        Canvas {
            id: canvas
            anchors {
                fill: parent
                topMargin: control.title.length > 0 ? 24 : 8
                leftMargin: 2; rightMargin: 6; bottomMargin: 4
            }
            onPaint: {
                var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                var pL = 40, pR = 8, pT = 6, pB = 14
                var pw = width - pL - pR, ph = height - pT - pB
                if (pw <= 0 || ph <= 0) return

                var pts = control.points || []
                var tMin = pts.length ? pts[0].t : 0
                var tMax = pts.length ? pts[pts.length - 1].t : 1
                if (tMax - tMin < 1) tMax = tMin + 1

                var yMin = control.fixedMin, yMax = control.fixedMax
                if (yMin === undefined || yMax === undefined) {
                    yMin = 1e9; yMax = -1e9
                    for (var i = 0; i < pts.length; i++) { if (pts[i].v < yMin) yMin = pts[i].v; if (pts[i].v > yMax) yMax = pts[i].v }
                    if (yMin > yMax) { yMin = 0; yMax = 1 }
                    var pad = Math.max((yMax - yMin) * 0.1, 0.5); yMin -= pad; yMax += pad
                }
                function tx(t) { return pL + (t - tMin) / (tMax - tMin) * pw }
                function ty(v) { return pT + ph - (v - yMin) / (yMax - yMin) * ph }

                if (control.bands) {
                    ctx.globalAlpha = 0.08
                    ctx.fillStyle = "#00C853"; ctx.fillRect(pL, ty(1.0), pw, ty(0.6) - ty(1.0))
                    ctx.fillStyle = "#FFD600"; ctx.fillRect(pL, ty(0.6), pw, ty(0.3) - ty(0.6))
                    ctx.fillStyle = "#FF1744"; ctx.fillRect(pL, ty(0.3), pw, ty(0.0) - ty(0.3))
                    ctx.globalAlpha = 1.0
                }

                ctx.fillStyle = Theme.textSecondary; ctx.font = "9px sans-serif"; ctx.textAlign = "right"
                for (var g = 0; g <= 4; g++) {
                    var gv = yMax - (yMax - yMin) * g / 4
                    var gy = ty(gv)
                    ctx.strokeStyle = Theme.border; ctx.lineWidth = 1
                    ctx.beginPath(); ctx.moveTo(pL, gy); ctx.lineTo(pL + pw, gy); ctx.stroke()
                    ctx.fillText(gv.toFixed(control.decimals), pL - 3, gy + 3)
                }

                if (pts.length >= 2) {
                    ctx.strokeStyle = control.lineColor; ctx.lineWidth = 1.6; ctx.beginPath()
                    for (i = 0; i < pts.length; i++) {
                        var x = tx(pts[i].t), y = ty(pts[i].v)
                        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                    }
                    ctx.stroke()
                }

                ctx.strokeStyle = Theme.border; ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(pL, pT); ctx.lineTo(pL, pT + ph); ctx.lineTo(pL + pw, pT + ph); ctx.stroke()
            }
        }
    }
}
