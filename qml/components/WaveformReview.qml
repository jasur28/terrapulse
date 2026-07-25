import QtQuick
import TerraPulse

// Reads the recorded waveform around a selected anomaly from the TDS archive
// (via the `waveform` service) and draws X/Y/Z as three auto-scaled lanes.
Item {
    id: root

    property int  object: 0
    property int  sensor: 0
    property real tStart: 0      // anomaly start (Unix ms)
    property real tEnd: 0        // anomaly end (Unix ms, 0 = ongoing)
    property real padBeforeMs: 5000
    property real padAfterMs: 10000
    property real maxWindowMs: 120000

    property var _data: null

    onObjectChanged: reload()
    onSensorChanged: reload()
    onTStartChanged: reload()

    function reload() {
        if (typeof waveform === "undefined" || !waveform.available || root.object <= 0) {
            root._data = null; canvas.requestPaint(); return
        }
        var s = root.tStart - root.padBeforeMs
        var e = (root.tEnd > root.tStart ? root.tEnd : root.tStart) + root.padAfterMs
        if (e - s > root.maxWindowMs) e = s + root.maxWindowMs
        root._data = waveform.load(root.object, root.sensor, s, e, 1000)
        canvas.requestPaint()
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 40
        visible: !root._data || !root._data.ok
        text: (typeof waveform !== "undefined" && !waveform.available)
              ? "No TDS archive configured (set TP_TDS or run tpacq --archive var/tds)."
              : (root.object > 0
                 ? "No recorded waveform in the archive for this window."
                 : "Select an event to inspect waveform context.")
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeNormal
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    Text {
        anchors { right: parent.right; top: parent.top; margins: 6 }
        visible: root._data && root._data.ok
        text: root._data
              ? (root._data.rate + " Hz · " + root._data.n + " pts")
              : ""
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSmall
    }

    Canvas {
        id: canvas
        anchors { fill: parent; topMargin: 22; margins: 6 }
        visible: root._data && root._data.ok

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var d = root._data
            if (!d || !d.ok) return

            var lanes = [
                { pts: d.x || [], mn: d.xMin, mx: d.xMax, label: "X", color: Theme.seriesX },
                { pts: d.y || [], mn: d.yMin, mx: d.yMax, label: "Y", color: Theme.seriesY },
                { pts: d.z || [], mn: d.zMin, mx: d.zMax, label: "Z", color: Theme.seriesZ }
            ]

            var pL = 8, pR = 8, pT = 2, pB = 2
            var laneH = (height - pT - pB) / lanes.length
            var pw = width - pL - pR
            if (pw <= 0 || laneH <= 4) return

            for (var li = 0; li < lanes.length; li++) {
                var lane = lanes[li]
                var y0 = pT + li * laneH
                var yc = y0 + laneH / 2
                var pts = lane.pts

                // lane separator + mid line
                ctx.strokeStyle = Theme.border
                ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(pL, yc); ctx.lineTo(pL + pw, yc); ctx.stroke()

                // axis label
                ctx.fillStyle = lane.color
                ctx.font = "bold 11px sans-serif"
                ctx.textAlign = "left"
                ctx.fillText(lane.label, pL + 2, y0 + 12)

                if (!pts || pts.length < 2) continue
                var mn = lane.mn, mx = lane.mx
                var span = Math.max(mx - mn, 1e-6)
                var amp = (laneH / 2) * 0.82

                ctx.strokeStyle = lane.color
                ctx.lineWidth = 1.3
                ctx.beginPath()
                for (var i = 0; i < pts.length; i++) {
                    var x = pL + (pts.length === 1 ? 0 : i / (pts.length - 1) * pw)
                    var norm = (pts[i] - (mn + mx) / 2) / span   // -0.5..0.5
                    var y = yc - norm * 2 * amp
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
            }
        }
    }
}
