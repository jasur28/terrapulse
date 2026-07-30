import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property bool filterEnabled: true
    property int activeTab: 0
    property bool manualAssociatorVisible: false
    property int maxRows: 80
    property int maxPoints: 1400
    property real windowSecs: 900
    property real baseTime: -1
    property real latestRelT: 0
    property bool dirty: false
    property var liveSeries: ({})
    property var picks: []
    property var artificialOrigin: ({ time: "", lat: "41.3111", lon: "69.2797", depth: "10" })

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
        var rows = []
        // Prefer the streams actually arriving over the SeedLink backbone (the
        // RecordStream model): the view reflects live records, not static inventory.
        var live = liveWaveform.streams || []
        if (live.length > 0) {
            for (var k = 0; k < live.length && rows.length < root.maxRows; k++) {
                var ls = live[k]
                var lsta = String(ls.station !== undefined ? ls.station : ls.object)
                var lsen = String(ls.sensor !== undefined ? ls.sensor : 1)
                var lnet = ls.network !== undefined ? String(ls.network) : "TP"
                rows.push({ station: lsta, network: lnet, sensor: lsen, channel: "HNX", component: "x", enabled: true, color: Theme.seriesX })
                rows.push({ station: lsta, network: lnet, sensor: lsen, channel: "HNY", component: "y", enabled: true, color: Theme.seriesY })
                rows.push({ station: lsta, network: lnet, sensor: lsen, channel: "HNZ", component: "z", enabled: true, color: Theme.seriesZ })
            }
            return rows.filter(function(r) { return root.activeTab === 0 ? r.enabled : !r.enabled })
        }
        var sensors = inventory.sensors || []
        for (var i = 0; i < sensors.length && rows.length < root.maxRows; i++) {
            var s = sensors[i]
            var sta = String(s.objectId !== undefined ? s.objectId : "OBJ")
            var sen = String(s.sensorId !== undefined ? s.sensorId : i + 1)
            var net = s.network !== undefined ? String(s.network) : "TP"
            var enabled = s.hasData !== false
            rows.push({ station: sta, network: net, sensor: sen, channel: "HNX", component: "x", enabled: enabled, color: Theme.seriesX })
            rows.push({ station: sta, network: net, sensor: sen, channel: "HNY", component: "y", enabled: enabled, color: Theme.seriesY })
            rows.push({ station: sta, network: net, sensor: sen, channel: "HNZ", component: "z", enabled: enabled, color: Theme.seriesZ })
        }
        if (rows.length === 0) {
            rows = [
                { station: "Bridge-A", network: "TP", sensor: "1", channel: "HNZ", component: "z", enabled: true, color: Theme.seriesZ },
                { station: "Tower-2",  network: "TP", sensor: "1", channel: "HNZ", component: "z", enabled: true, color: Theme.seriesZ },
                { station: "Dam-3",    network: "TP", sensor: "1", channel: "HNZ", component: "z", enabled: true, color: Theme.seriesZ },
                { station: "Tunnel-4", network: "TP", sensor: "1", channel: "HNZ", component: "z", enabled: true, color: Theme.seriesZ },
                { station: "Bldg-5",   network: "TP", sensor: "1", channel: "HNZ", component: "z", enabled: true, color: Theme.seriesZ },
                { station: "Bridge-6", network: "TP", sensor: "1", channel: "HNZ", component: "z", enabled: true, color: Theme.seriesZ }
            ]
        }
        return rows.filter(function(r) { return root.activeTab === 0 ? r.enabled : !r.enabled })
    }

    function rowKey(row) {
        return row.station + "." + row.sensor + "." + row.channel
    }

    // Structural-health status of an object (drives the left status strip): from
    // the inventory warning level, not computed here — the UI only displays.
    function healthColor(objId) {
        var s = inventory.structures || []
        for (var i = 0; i < s.length; i++)
            if (Number(s[i].objectId) === Number(objId))
                return Theme.statusColor(s[i].warning !== undefined ? s[i].warning : 0)
        return Theme.colorNormal
    }

    // Short badge for a structural anomaly type (accelerometer domain — these are
    // structural triggers, not seismic P/S phase picks).
    function anomalyShort(typeName) {
        var t = String(typeName || "").toLowerCase()
        if (t.indexOf("reson") >= 0) return "R"
        if (t.indexOf("crack") >= 0) return "C"
        if (t.indexOf("settle") >= 0) return "S"
        if (t.indexOf("overload") >= 0) return "O"
        if (t.indexOf("vibr") >= 0) return "V"
        return "!"
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

    function openArtificialOrigin() {
        originTime.text = new Date().toISOString().replace("T", " ").slice(0, 19) + " UTC"
        originLat.text = artificialOrigin.lat
        originLon.text = artificialOrigin.lon
        originDepth.text = artificialOrigin.depth
        artificialOriginDialog.open()
    }

    Connections {
        target: liveWaveform

        function onStateChanged() {
            // Keep the trace across brief reconnects — don't wipe on a transient
            // disconnect (connected can flip between record bursts over the network).
        }

        function onSampleReceived(sample) {
            var t = sample.timestampMs / 1000.0
            if (root.baseTime < 0) root.baseTime = t
            root.latestRelT = t - root.baseTime

            var sta = String(sample.object !== undefined ? sample.object : sample.station)
            var sen = String(sample.sensor !== undefined ? sample.sensor : 1)
            appendPoint(sta + "." + sen + ".HNX", root.latestRelT, sample.x || 0)
            appendPoint(sta + "." + sen + ".HNY", root.latestRelT, sample.y || 0)
            appendPoint(sta + "." + sen + ".HNZ", root.latestRelT, sample.z || 0)
            root.dirty = true
        }
    }

    // Trigger markers come from the analysis modules (shf. anomalies over the bus),
    // not from thresholds guessed in the UI. Placed on the trace time line by the
    // anomaly's onset time; labelled by structural anomaly type.
    Connections {
        target: appController
        function onShfReceived(shf) {
            if (root.baseTime < 0) return
            if (shf.anomalyStatus !== undefined && shf.anomalyStatus !== 0) return   // only onsets
            var ts = (shf.anomalyStartTime || Date.now()) / 1000.0
            addPick(ts - root.baseTime, anomalyShort(shf.anomalyTypeName),
                    (shf.severity !== undefined ? shf.severity : 1) >= 2)
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
                    text: liveWaveform.connected ? "LIVE  " + liveWaveform.endpoint
                          : (liveWaveform.endpoint !== "" ? "Waiting for records  " + liveWaveform.endpoint : "No SeedLink backbone")
                    color: liveWaveform.connected ? "#126b22" : "#777777"
                    font.pixelSize: 12
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

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
                    var rowH = Math.max(22, plotH / Math.max(1, rows.length))
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

                        // Left status strip: structural health of the object.
                        ctx.fillStyle = root.healthColor(row.station)
                        ctx.fillRect(0, y0, 5, rowH)

                        ctx.strokeStyle = "#c7c7c7"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(left, y0 + rowH)
                        ctx.lineTo(left + plotW, y0 + rowH)
                        ctx.stroke()

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
                        var scale = rowH * 0.40 / Math.max(maxAbs, 1)

                        // Left column: stream / net / cha, and live amax (gal) right-aligned.
                        ctx.fillStyle = "#111111"
                        ctx.font = "bold 11px sans-serif"
                        ctx.textAlign = "left"
                        ctx.fillText(row.station, 11, mid)
                        ctx.fillStyle = "#4f4f4f"
                        ctx.font = "10px sans-serif"
                        ctx.fillText(row.network, 84, mid)
                        ctx.fillText(row.channel, 110, mid)
                        ctx.fillStyle = "#6f6f75"
                        ctx.font = "10px monospace"
                        ctx.textAlign = "right"
                        ctx.fillText(maxAbs.toExponential(1), left - 6, mid)
                        ctx.textAlign = "left"

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
                        var pc = pick.associated ? Theme.colorCritical : Theme.colorHigh
                        ctx.strokeStyle = pc
                        ctx.lineWidth = pick.associated ? 2 : 1.5
                        ctx.beginPath()
                        ctx.moveTo(px, top)
                        ctx.lineTo(px, top + visibleRows * rowH)
                        ctx.stroke()
                        ctx.fillStyle = pc
                        ctx.font = "bold 11px sans-serif"
                        ctx.textAlign = "left"
                        ctx.fillText(pick.label, px + 3, top + 14 + (pk % 8) * 16)
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
                id: associatorPanel
                visible: root.manualAssociatorVisible
                Layout.preferredWidth: visible ? 340 : 0
                Layout.fillHeight: true
                color: "#efefef"
                border.color: "#b6b6b6"
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Manual associator"
                            color: "#202020"
                            font.pixelSize: 13
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        ClassicToolButton {
                            text: "x"
                            implicitWidth: 28
                            onClicked: root.manualAssociatorVisible = false
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 118
                        color: "#ffffff"
                        border.color: "#c8c8c8"
                        clip: true

                        GridView {
                            anchors.fill: parent
                            anchors.margins: 6
                            cellWidth: 150
                            cellHeight: 32
                            model: Math.min(root.picks.length, 24)
                            delegate: Rectangle {
                                width: 142
                                height: 26
                                color: "#e9e9e9"
                                border.color: "#b90000"
                                border.width: 2
                                Text {
                                    anchors.centerIn: parent
                                    text: "TP." + (index + 1) + "..HH" + (index % 3 === 0 ? "Z" : index % 3 === 1 ? "N" : "E") + " - " + root.picks[index].label
                                    color: "#202020"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 220
                        color: "#d8d8d8"
                        border.color: "#a8a8a8"
                        clip: true

                        MapView {
                            anchors.fill: parent
                            provider: "osm"
                            tilesUrl: (typeof mapsUrl !== "undefined" && mapsUrl !== "") ? mapsUrl + "/osm" : ""
                            showLabels: true
                            markers: [
                                { lon: 69.2797, lat: 41.3111, color: "#f0e000", outline: "#8a008a", shape: "event", pulse: true, label: "Origin" },
                                { lon: 69.2430, lat: 41.3260, color: "#0048ff", outline: "#ffffff", shape: "station", label: "TP02" },
                                { lon: 69.3380, lat: 41.2990, color: "#a00000", outline: "#ffffff", shape: "station", label: "TP03" }
                            ]
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#ffffff"
                        border.color: "#c8c8c8"

                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            columns: 2
                            rowSpacing: 7
                            columnSpacing: 8

                            Repeater {
                                model: [
                                    "Time:", new Date().toISOString().replace("T", " ").slice(0, 19),
                                    "Depth:", "10 m",
                                    "Lat:", "41.3111 N",
                                    "Lon:", "69.2797 E",
                                    "Phases:", Math.max(1, root.picks.length),
                                    "RMS Res.:", "0.0 s",
                                    "Agency:", "TerraPulse",
                                    "Author:", "tprttv"
                                ]
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData
                                    color: index % 2 === 0 ? "#555555" : "#202020"
                                    font.pixelSize: 12
                                    font.bold: index % 2 === 1
                                    horizontalAlignment: index % 2 === 0 ? Text.AlignRight : Text.AlignLeft
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        ClassicToolButton { text: "Inspect" }
                        ClassicToolButton { text: "Show origin" }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.picks.length > 0 ? "Status: picks ready" : "Status: waiting"
                            color: "#202020"
                            font.pixelSize: 11
                        }
                    }
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
                    model: ["Filter ON : hp:0.3, lp:20 (structural)", "Filter ON : hp:0.1", "Filter OFF"]
                    currentIndex: root.filterEnabled ? 0 : 2
                    onActivated: root.filterEnabled = currentIndex !== 2
                }

                ComboBox {
                    Layout.preferredWidth: 150
                    model: ["Associate picks", "Create artificial origin", "Show amplitudes"]
                }

                Text {
                    Layout.fillWidth: true
                    text: liveWaveform.connected
                          ? "Receiving records from " + liveWaveform.endpoint + "    " + liveWaveform.records + " samples"
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

    Dialog {
        id: artificialOriginDialog
        modal: true
        title: "Artificial Origin"
        standardButtons: Dialog.NoButton
        width: 320
        height: 252
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)

        background: Rectangle {
            color: "#efefef"
            border.color: "#8f8f8f"
            radius: 2
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "Origin"
                horizontalAlignment: Text.AlignHCenter
                color: "#202020"
                font.pixelSize: 13
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 8
                columnSpacing: 8

                Text { text: "Time:"; color: "#202020"; font.pixelSize: 12 }
                TextField { id: originTime; Layout.fillWidth: true; font.pixelSize: 12; selectByMouse: true }
                Text { text: "Lat:"; color: "#202020"; font.pixelSize: 12 }
                TextField { id: originLat; Layout.fillWidth: true; font.pixelSize: 12; horizontalAlignment: TextInput.AlignRight }
                Text { text: "Lon:"; color: "#202020"; font.pixelSize: 12 }
                TextField { id: originLon; Layout.fillWidth: true; font.pixelSize: 12; horizontalAlignment: TextInput.AlignRight }
                Text { text: "Depth:"; color: "#202020"; font.pixelSize: 12 }
                TextField { id: originDepth; Layout.fillWidth: true; font.pixelSize: 12; horizontalAlignment: TextInput.AlignRight }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ClassicToolButton {
                    text: "Create"
                    onClicked: {
                        root.artificialOrigin = ({ time: originTime.text, lat: originLat.text, lon: originLon.text, depth: originDepth.text })
                        artificialOriginDialog.close()
                    }
                }
                ClassicToolButton { text: "Cancel"; onClicked: artificialOriginDialog.close() }
            }
        }
    }
}
