import QtQuick
import TerraPulse

// Response spectrum (5% damped PSA vs period) — the core structural-engineering
// view: it shows how hard the ground motion would drive an oscillator at each
// period, so a peak near a building's own period means resonance. An optional
// design-code curve can be drawn behind it for comparison.
Panel {
    id: root

    property var periods: []       // seconds
    property var psa: []           // gal, same length as periods
    property real peakPeriod: 0    // highlighted period
    property var designCode: []    // optional reference curve (same periods)
    property string codeName: "design code"

    title: "Response spectrum"
    subtitle: "5% damped PSA — resonance check"

    onPeriodsChanged: canvas.requestPaint()
    onPsaChanged: canvas.requestPaint()
    onDesignCodeChanged: canvas.requestPaint()

    Text {
        anchors.centerIn: parent
        visible: !root.psa || root.psa.length < 2
        text: "Waiting for strong-motion parameters (tpwfparam)…"
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSmall
    }

    Canvas {
        id: canvas
        anchors { fill: parent; topMargin: 44; leftMargin: 8; rightMargin: 10; bottomMargin: 8 }
        visible: root.psa && root.psa.length >= 2

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var p = root.psa || [], t = root.periods || []
            if (p.length < 2 || t.length !== p.length) return

            var pL = 46, pR = 8, pT = 8, pB = 26
            var pw = width - pL - pR, ph = height - pT - pB
            if (pw <= 0 || ph <= 0) return

            var tMin = t[0], tMax = t[t.length - 1]
            var vMax = 0
            for (var i = 0; i < p.length; i++) vMax = Math.max(vMax, p[i])
            for (var j = 0; j < (root.designCode || []).length; j++)
                vMax = Math.max(vMax, root.designCode[j])
            vMax = Math.max(vMax * 1.15, 1)

            function tx(v) { return pL + (v - tMin) / Math.max(tMax - tMin, 1e-6) * pw }
            function ty(v) { return pT + ph - (v / vMax) * ph }

            // grid + axis labels
            ctx.fillStyle = Theme.textSecondary
            ctx.font = "10px sans-serif"
            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            for (var g = 0; g <= 4; g++) {
                var gy = pT + g * ph / 4
                ctx.beginPath(); ctx.moveTo(pL, gy); ctx.lineTo(pL + pw, gy); ctx.stroke()
                ctx.textAlign = "right"
                ctx.fillText((vMax - g * vMax / 4).toFixed(0), pL - 5, gy + 4)
            }
            ctx.textAlign = "center"
            for (var k = 0; k < t.length; k += Math.max(1, Math.floor(t.length / 5)))
                ctx.fillText(t[k] + "s", tx(t[k]), pT + ph + 16)
            ctx.textAlign = "left"
            ctx.fillText("gal", pL, pT - 1)

            // optional design-code reference
            if (root.designCode && root.designCode.length === p.length) {
                ctx.beginPath()
                ctx.strokeStyle = Theme.colorWarning
                ctx.lineWidth = 1.5
                ctx.setLineDash([5, 4])
                for (var d = 0; d < p.length; d++) {
                    var xd = tx(t[d]), yd = ty(root.designCode[d])
                    if (d === 0) ctx.moveTo(xd, yd); else ctx.lineTo(xd, yd)
                }
                ctx.stroke()
                ctx.setLineDash([])
            }

            // spectrum curve + filled area
            ctx.beginPath()
            ctx.moveTo(tx(t[0]), pT + ph)
            for (var m = 0; m < p.length; m++) ctx.lineTo(tx(t[m]), ty(p[m]))
            ctx.lineTo(tx(t[t.length - 1]), pT + ph)
            ctx.closePath()
            ctx.fillStyle = "#1b3a5c"
            ctx.fill()

            ctx.beginPath()
            ctx.strokeStyle = Theme.colorService
            ctx.lineWidth = 2
            for (var n = 0; n < p.length; n++) {
                var xn = tx(t[n]), yn = ty(p[n])
                if (n === 0) ctx.moveTo(xn, yn); else ctx.lineTo(xn, yn)
            }
            ctx.stroke()

            // mark the resonant peak
            if (root.peakPeriod > 0) {
                var xp = tx(root.peakPeriod)
                ctx.strokeStyle = Theme.colorCritical
                ctx.lineWidth = 1
                ctx.setLineDash([3, 3])
                ctx.beginPath(); ctx.moveTo(xp, pT); ctx.lineTo(xp, pT + ph); ctx.stroke()
                ctx.setLineDash([])
                ctx.fillStyle = Theme.colorCritical
                ctx.textAlign = "center"
                ctx.fillText("peak " + root.peakPeriod + "s", xp, pT + 10)
            }
        }
    }
}
