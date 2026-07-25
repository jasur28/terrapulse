import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property bool filterEnabled: true
    property int activeTab: 0
    property int maxRows: 80
    property int maxPoints: 1400
    property real windowSecs: 900
    property real baseTime: -1
    property real latestRelT: 0
    property bool dirty: false
    property var liveSeries: ({})
    property var picks: []

    readonly property color traceGray: "#7d7d7d"
    readonly property color traceGreen: "#62b400"
    readonly property color traceBlue: "#143cff"
    readonly property color traceYellow: "#d7f200"

    function pad2(v) {
        return v < 10 ? "0" + v : "" + v
    }

    function formatClock(seconds) {
        var d = new Date(Math.max(0, seconds) * 1000)
        return pad2(d.getUTCHours()) + ":" + pad2(d.getUTCMinutes()) + ":" + pad2(d.getUTCSeconds())
    }

    function streamRows() {
        var sensors = inventory.sensors || []
        var rows = []
        for (var i = 0; i < sensors.length && rows.length < root.maxRows; i++) {
            var s = sensors[i]
            var sta = String(s.objectId !== undefined ? s.objectId : "OBJ")
            var sen = String(s.sensorId !== undefined ? s.sensorId : i + 1)
            var net = s.network !== undefined ? String(s.network) : "TP"
            var enabled = s.hasData !== false
            rows.push({ station: sta, network: net, sensor: sen, channel: "HHE", component: "x", enabled: enabled, color: root.traceGray })
            rows.push({ station: sta, network: net, sensor: sen, channel: "HHN", component: "y", enabled: enabled, color: root.traceGreen })
            rows.push({ station: sta, network: net, sensor: sen, channel: "HHZ", component: "z", enabled: enabled, color: root.traceBlue })
        }
        if (rows.length === 0) {
            rows = [
                { station: "PB02", network: "TP", sensor: "01", channel: "HHZ", component: "z", enabled: true, color: root.traceYellow },
                { station: "PB03", network: "TP", sensor: "01", channel: "HHZ", component: "z", enabled: true, color: root.traceGreen },
                { station: "PB05", network: "TP", sensor: "01", channel: "HHZ", component: "z", enabled: true, color: root.traceGreen },
                { station: "PB06", network: "TP", sensor: "01", channel: "HHZ", component: "z", enabled: true, color: root.traceGreen },
                { station: "PB08", network: "TP", sensor: "01", channel: "HHZ", component: "z", enabled: true, color: root.traceBlue },
                { station: "PB09", network: "TP", sensor: "01", channel: "HHZ", component: "z", enabled: true, color: root.traceBlue }
            ]
        }
        return rows.filter(function(r) { return root.activeTab === 0 ? r.enabled : !r.enabled })
    }

    function rowKey(row) {
        return row.station + "." + row.sensor + "." + row.channel
    }

    function ensureSeries(key) {
        if (root.liveSeries[key] === undefined) root.liveSeries[key] = []
        return root.liveSeries[key]
    }

    function appendPoint(key, t, v) {
        var series = ensureSeries(key)
        series.push({ t: t, v: v })
        while (series.length > root.maxPoints) series.shift()
    }

    function sampleValue(sample, component) {
        if (component === "x") return sample.x || 0
        if (component === "y") return sample.y || 0
        return sample.z || 0
    }

    function addPick(t, label, associated) {
        root.picks.push({ t: t, label: label, associated: associated })
        while (root.picks.length > 80) root.picks.shift()
    }

    Connections {
        target: acq

        function onConnectedChanged() {
            if (!acq.connected) {
                root.liveSeries = ({})
                root.picks = []
                root.baseTime = -1
                root.latestRelT = 0
                canvas.requestPaint()
            }
        }

        function onSampleReceived(sample) {
            var t = sample.timestampMs / 1000.0
            if (root.baseTime < 0) root.baseTime = t
            root.latestRelT = t - root.baseTime

            var sta = String(sample.object !== undefined ? sample.object : acq.object)
            var sen = String(sample.sensor !== undefined ? sample.sensor : acq.sensor)
            appendPoint(sta + "." + sen + ".HHE", root.latestRelT, sample.x || 0)
            appendPoint(sta + "." + sen + ".HHN", root.latestRelT, sample.y || 0)
            appendPoint(sta + "." + sen + ".HHZ", root.latestRelT, sample.z || 0)

            var abs = Math.max(Math.abs(sample.x || 0), Math.abs(sample.y || 0), Math.abs(sample.z || 0))
            if (abs > 0 && root.picks.length < 1 || (abs > 80 && root.latestRelT - root.picks[root.picks.length - 1].t > 8)) {
                addPick(root.latestRelT, abs > 120 ? "S" : "P", abs > 120)
            }
            root.dirty = true
        }
    }

    Timer {
        interval: 80
        running: root.visible
        repeat: true
        onTriggered: {
            if (!root.dirty) return
            root.dirty = false
            canvas.requestPaint()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#eeeeee"
            border.color: "#b6b6b6"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 8
                spacing: 0

                TabButton {
                    text: "\u2714  Enabled (" + streamRows().length + ")"
                    checked: root.activeTab === 0
                    onClicked: root.activeTab = 0
                    height: 30
                }

                TabButton {
                    text: "\u2716  Disabled (0)"
                    checked: root.activeTab === 1
                    onClicked: root.activeTab = 1
                    height: 30
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: acq.connected ? "LIVE  " + acq.endpoint : "No connection to tpmaster"
                    color: acq.connected ? "#126b22" : "#777777"
                    font.pixelSize: 12
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#f4f4f4"
            border.color: "#a8a8a8"
            clip: true

            Canvas {
                id: canvas
                anchors.fill: parent

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = "#f4f4f4"
                    ctx.fillRect(0, 0, width, height)

                    var rows = root.streamRows()
                    var left = 156
                    var right = 18
                    var top = 8
                    var bottom = 64
                    var plotW = Math.max(1, width - left - right)
                    var plotH = Math.max(1, height - top - bottom)
                    var rowH = Math.max(34, plotH / Math.max(1, rows.length))
                    var visibleRows = Math.min(rows.length, Math.floor(plotH / rowH))
                    var tEnd = Math.max(root.windowSecs, root.latestRelT + 1)
                    var tStart = Math.max(0, tEnd - root.windowSecs)
                    var tRange = Math.max(1, tEnd - tStart)

                    function tx(t) { return left + (t - tStart) / tRange * plotW }
                    function rowMid(i) { return top + i * rowH + rowH * 0.5 }

                    ctx.font = "12px sans-serif"
                    ctx.textBaseline = "middle"

                    for (var g = 0; g <= 8; g++) {
                        var gx = left + g * plotW / 8
                        ctx.strokeStyle = "#d3d3d3"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(gx, top)
                        ctx.lineTo(gx, top + visibleRows * rowH)
                        ctx.stroke()
                    }

                    ctx.strokeStyle = "#202020"
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.moveTo(left, top + visibleRows * rowH)
                    ctx.lineTo(left + plotW, top + visibleRows * rowH)
                    ctx.stroke()

                    for (var i = 0; i < visibleRows; i++) {
                        var row = rows[i]
                        var y0 = top + i * rowH
                        var mid = rowMid(i)

                        ctx.fillStyle = i % 2 === 0 ? "#ffffff" : "#eeeeee"
                        ctx.fillRect(0, y0, width, rowH)

                        ctx.strokeStyle = "#c7c7c7"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(left, y0 + rowH)
                        ctx.lineTo(left + plotW, y0 + rowH)
                        ctx.stroke()

                        ctx.fillStyle = "#111111"
                        ctx.font = "bold 12px sans-serif"
                        ctx.textAlign = "left"
                        ctx.fillText(row.station, 14, mid)
                        ctx.font = "12px sans-serif"
                        ctx.fillText(row.network, 72, mid)
                        ctx.fillText(row.channel, 116, mid)

                        var key = root.rowKey(row)
                        var series = root.liveSeries[key] || []
                        var maxAbs = 1e-9
                        var sum = 0
                        var used = 0
                        for (var p = 0; p < series.length; p++) {
                            if (series[p].t < tStart) continue
                            maxAbs = Math.max(maxAbs, Math.abs(series[p].v))
                            sum += series[p].v
                            used++
                        }
                        var mean = used > 0 ? sum / used : 0
                        var scale = rowH * 0.38 / Math.max(maxAbs, 1)

                        ctx.fillStyle = "#202020"
                        ctx.font = "11px sans-serif"
                        ctx.fillText("amax: " + maxAbs.toExponential(4), left + 4, y0 + 13)
                        ctx.fillText("mean: " + mean.toExponential(4), left + 4, y0 + 27)

                        ctx.strokeStyle = row.color
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        var moved = false
                        for (var s = 0; s < series.length; s++) {
                            if (series[s].t < tStart) continue
                            var x = tx(series[s].t)
                            var y = mid - (series[s].v - mean) * scale
                            if (!moved) {
                                ctx.moveTo(x, y)
                                moved = true
                            } else {
                                ctx.lineTo(x, y)
                            }
                        }
                        if (moved) ctx.stroke()

                        ctx.strokeStyle = "#9a9a9a"
                        ctx.lineWidth = 0.7
                        ctx.beginPath()
                        ctx.moveTo(left, mid)
                        ctx.lineTo(left + plotW, mid)
                        ctx.stroke()
                    }

                    for (var pk = 0; pk < root.picks.length; pk++) {
                        var pick = root.picks[pk]
                        if (pick.t < tStart || pick.t > tEnd) continue
                        var px = tx(pick.t)
                        ctx.strokeStyle = pick.associated ? "#0074d9" : "#ff0000"
                        ctx.lineWidth = pick.associated ? 2 : 1.5
                        ctx.beginPath()
                        ctx.moveTo(px, top)
                        ctx.lineTo(px, top + visibleRows * rowH)
                        ctx.stroke()
                        ctx.fillStyle = "#ff0000"
                        ctx.font = "11px sans-serif"
                        ctx.fillText(pick.label, px + 3, top + 16 + (pk % 8) * 18)
                    }

                    ctx.strokeStyle = "#ff2a2a"
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    ctx.moveTo(left + plotW, top)
                    ctx.lineTo(left + plotW, top + visibleRows * rowH)
                    ctx.stroke()

                    ctx.fillStyle = "#202020"
                    ctx.font = "12px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "top"
                    for (var tick = 0; tick <= 6; tick++) {
                        var tt = tStart + tick * tRange / 6
                        var xTick = tx(tt)
                        ctx.strokeStyle = "#555555"
                        ctx.beginPath()
                        ctx.moveTo(xTick, top + visibleRows * rowH)
                        ctx.lineTo(xTick, top + visibleRows * rowH + 8)
                        ctx.stroke()
                        ctx.fillText(root.formatClock(tt), xTick, top + visibleRows * rowH + 12)
                    }
                    ctx.fillText(new Date().toISOString().slice(0, 10), left + 80, top + visibleRows * rowH + 36)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#eeeeee"
            border.color: "#b6b6b6"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 8
                spacing: 8

                ComboBox {
                    Layout.preferredWidth: 238
                    model: ["Filter ON : BW(0.5, 8.0)", "Filter ON : HP(0.1)", "Filter OFF"]
                    currentIndex: root.filterEnabled ? 0 : 2
                    onActivated: root.filterEnabled = currentIndex !== 2
                }

                ComboBox {
                    Layout.preferredWidth: 150
                    model: ["Associate picks", "Create artificial origin", "Show amplitudes"]
                }

                Text {
                    Layout.fillWidth: true
                    text: acq.connected
                          ? "Receiving records from " + acq.endpoint + "    " + acq.packetCount + " samples"
                          : "Loading records"
                    color: "#202020"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Text {
                    text: "Status"
                    color: "#202020"
                    font.pixelSize: 12
                }
            }
        }
    }
}
