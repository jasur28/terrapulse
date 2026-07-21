import QtQuick
import TerraPulse

Panel {
    id: root

    property var spectrum: []
    property real dominantFrequency: 0
    property real sampleRate: 200
    property color lineColor: Theme.colorService

    title: "Spectrum"
    subtitle: dominantFrequency > 0 ? ("Dominant " + dominantFrequency.toFixed(1) + " Hz") : "Waiting for samples"

    function requestPaint() { canvas.requestPaint() }
    onSpectrumChanged: canvas.requestPaint()
    onDominantFrequencyChanged: canvas.requestPaint()

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

            var pL = 44, pR = 8, pT = 8, pB = 24
            var pw = width - pL - pR
            var ph = height - pT - pB
            if (pw <= 0 || ph <= 0) return

            var spec = root.spectrum || []
            var fMax = Math.max(root.sampleRate / 2, 1)
            var maxMag = 1e-9
            for (var i = 0; i < spec.length; i++) {
                maxMag = Math.max(maxMag, spec[i].mag)
            }

            function fx(f) { return pL + f / fMax * pw }
            function my(m) { return pT + ph - (m / maxMag) * ph }

            ctx.fillStyle = Theme.textSecondary
            ctx.font = "10px sans-serif"
            ctx.textAlign = "center"

            for (var g = 0; g <= 5; g++) {
                var f = g * fMax / 5
                var x = fx(f)
                ctx.strokeStyle = Theme.border
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(x, pT)
                ctx.lineTo(x, pT + ph)
                ctx.stroke()
                ctx.fillText(Math.round(f) + "Hz", x, pT + ph + 17)
            }

            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = 1.4
            ctx.beginPath()
            for (i = 0; i < spec.length; i++) {
                x = fx(spec[i].f)
                ctx.moveTo(x, pT + ph)
                ctx.lineTo(x, my(spec[i].mag))
            }
            ctx.stroke()

            if (root.dominantFrequency > 0) {
                x = fx(root.dominantFrequency)
                ctx.strokeStyle = Theme.colorWarning
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(x, pT)
                ctx.lineTo(x, pT + ph)
                ctx.stroke()
            }

            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(pL, pT)
            ctx.lineTo(pL, pT + ph)
            ctx.lineTo(pL + pw, pT + ph)
            ctx.stroke()
        }
    }
}
