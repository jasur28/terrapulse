import QtQuick
import TerraPulse

Canvas {
    id: root
    width: 110; height: 110

    property real value: 1.0   // 0.0 – 1.0
    property int  ringWidth: 10

    onValueChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()

        var cx = width  / 2
        var cy = height / 2
        var r  = Math.min(width, height) / 2 - ringWidth - 2

        // Track arc (background)
        ctx.beginPath()
        ctx.arc(cx, cy, r, 0.75 * Math.PI, 2.25 * Math.PI)
        ctx.strokeStyle = Theme.border
        ctx.lineWidth   = ringWidth
        ctx.lineCap     = "round"
        ctx.stroke()

        // Value arc
        if (value > 0) {
            var sweep = 1.5 * Math.PI * Math.min(value, 1.0)
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0.75 * Math.PI, 0.75 * Math.PI + sweep)
            if      (value >= 0.6) ctx.strokeStyle = Theme.colorNormal
            else if (value >= 0.3) ctx.strokeStyle = Theme.colorWarning
            else                   ctx.strokeStyle = Theme.colorCritical
            ctx.lineWidth = ringWidth
            ctx.lineCap   = "round"
            ctx.stroke()
        }

        // Percentage label
        var pct = Math.round(value * 100)
        ctx.fillStyle = Theme.textPrimary
        ctx.font      = "bold " + Math.round(height * 0.18) + "px sans-serif"
        ctx.textAlign    = "center"
        ctx.textBaseline = "middle"
        ctx.fillText(pct + "%", cx, cy)
    }
}
