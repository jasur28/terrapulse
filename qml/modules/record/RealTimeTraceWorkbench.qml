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
    // Visible time span. The trace itself is decimated in C++ (liveWaveform
    // envelopes) — QML keeps no raw samples, only this window and the axis time.
    property real windowSecs: 60
    property real baseTime: -1
    property real latestRelT: 0
    property bool dirty: false
    // Debounced "receiving" flag: liveWaveform.connected flickers between record
    // bursts over the network, so derive liveness from when a sample last arrived.
    property double lastSampleMs: 0
    property bool receiving: false
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

    // FDSN identity from the inventory (display only): map a live stream's numeric
    // object/sensor to the real network / station code / sensor location, and derive
    // the SEED channel code from the sensor kind + rate (StreamId rules) so an SM-3
    // seismometer reads EHZ, not HNZ.
    function structFor(objId) {
        var s = inventory.structures || []
        for (var i = 0; i < s.length; i++)
            if (Number(s[i].objectId) === Number(objId)) return s[i]
        return null
    }
    function sensorFor(objId, senId) {
        var s = inventory.sensors || []
        for (var i = 0; i < s.length; i++)
            if (Number(s[i].objectId) === Number(objId) && Number(s[i].sensorId) === Number(senId)) return s[i]
        return null
    }
    function channelCode(sensor, rate, comp) {
        var kind = sensor && sensor.kind ? String(sensor.kind) : "accelerometer"
        var corner = (sensor && sensor.cornerPeriod !== undefined) ? Number(sensor.cornerPeriod)
                     : (kind === "seismometer" ? 2.0 : 1e9)
        var r = rate > 0 ? rate : 200
        var broadband = corner >= 10.0
        var band = r >= 80 ? (broadband ? "H" : "E")
                 : r >= 10 ? (broadband ? "B" : "S")
                 : r >= 1  ? "M" : "L"
        var instr = kind === "seismometer" ? "H" : "N"
        var orient = kind === "seismometer" ? ["Z", "N", "E"][comp] : ["X", "Y", "Z"][comp]
        return band + instr + orient
    }

    // Live rows use the REAL identity carried by the stream (net/stationCode/location
    // and per-axis channel code) — computed once in C++/inventory on the backend, not
    // re-derived here, so the GUI and backend can never disagree.
    function rowsFromStream(ls) {
        var lobj = ls.object !== undefined ? ls.object : ls.station
        var lsen = ls.sensor !== undefined ? ls.sensor : 1
        var net = ls.network ? String(ls.network) : "TP"
        var name = ls.stationCode ? String(ls.stationCode) : String(lobj)
        var loc = ls.location ? String(ls.location) : ""
        var chs = ls.channels || []
        var colors = [Theme.seriesX, Theme.seriesY, Theme.seriesZ]
        var comps = ["x", "y", "z"]
        var defc = ["HNX", "HNY", "HNZ"]
        var out = []
        for (var c = 0; c < 3; c++)
            out.push({ station: String(lobj), object: Number(lobj), stationName: name,
                       network: net, sensor: String(lsen), location: loc,
                       channel: chs[c] ? String(chs[c]) : defc[c], component: comps[c], compIndex: c,
                       enabled: true, color: colors[c] })
        return out
    }

    // Inventory-derived rows — used only as an offline preview when no stream is live
    // yet. channelCode() here mirrors the backend StreamId rules for that preview.
    function makeRows(objId, senId, network, rate) {
        var st = structFor(objId)
        var sn = sensorFor(objId, senId)
        var net = st && st.network ? String(st.network) : (network ? String(network) : "TP")
        var name = st && st.stationCode ? String(st.stationCode) : String(objId)
        var loc = sn && sn.location ? String(sn.location) : ""
        var colors = [Theme.seriesX, Theme.seriesY, Theme.seriesZ]
        var comps = ["x", "y", "z"]
        var out = []
        for (var c = 0; c < 3; c++)
            out.push({ station: String(objId), object: Number(objId), stationName: name,
                       network: net, sensor: String(senId), location: loc,
                       channel: channelCode(sn, rate, c), component: comps[c], compIndex: c,
                       enabled: true, color: colors[c] })
        return out
    }

    function streamRows() {
        var rows = []
        // Prefer the streams actually arriving over the SeedLink backbone (the
        // RecordStream model): the view reflects live records, not static inventory.
        var live = liveWaveform.streams || []
        if (live.length > 0) {
            for (var k = 0; k < live.length && rows.length < root.maxRows; k++)
                rows = rows.concat(rowsFromStream(live[k]))
            return rows.filter(function(r) { return root.activeTab === 0 ? r.enabled : !r.enabled })
        }
        var sensors = inventory.sensors || []
        for (var i = 0; i < sensors.length && rows.length < root.maxRows; i++) {
            var s = sensors[i]
            var oid = s.objectId !== undefined ? s.objectId : i + 1
            var sid = s.sensorId !== undefined ? s.sensorId : i + 1
            var srate = (s.channels && s.channels[0] && s.channels[0].sampleRate) ? s.channels[0].sampleRate : 200
            var made = makeRows(oid, sid, s.network, Number(srate))
            var en = s.hasData !== false
            for (var m = 0; m < made.length; m++) { made[m].enabled = en; rows.push(made[m]) }
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

        // Latest sample drives the time axis and the live/waiting status. The trace
        // itself is drawn from liveWaveform.envelopes (decimated in C++), read fresh
        // in onPaint — so no raw samples are kept or iterated in QML.
        function onSampleReceived(sample) {
            var t = sample.timestampMs / 1000.0
            if (root.baseTime < 0) root.baseTime = t
            root.latestRelT = t - root.baseTime
            root.lastSampleMs = Date.now()
            if (!root.receiving) root.receiving = true
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
        interval: 120
        running: root.visible
        repeat: true
        onTriggered: {
            // Debounce the live/waiting status: still "receiving" if a sample
            // arrived within the last 1.5 s, regardless of connected flicker.
            var live = (Date.now() - root.lastSampleMs) < 1500
            if (root.receiving !== live) root.receiving = live
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
                    id: enabledTab
                    text: "\u2714  Enabled (" + streamRows().length + ")"
                    checked: root.activeTab === 0
                    onClicked: root.activeTab = 0
                    height: 26
                    font.pixelSize: 11
                    contentItem: Text {
                        text: enabledTab.text
                        color: enabledTab.checked ? "#008000" : "#202020"
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        color: enabledTab.checked ? "#ffffff" : "#d0d0d0"
                        border.color: "#a8a8a8"
                    }
                }

                TabButton {
                    id: disabledTab
                    text: "\u2716  Disabled (0)"
                    checked: root.activeTab === 1
                    onClicked: root.activeTab = 1
                    height: 26
                    font.pixelSize: 11
                    contentItem: Text {
                        text: disabledTab.text
                        color: disabledTab.checked ? "#b00000" : "#202020"
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        color: disabledTab.checked ? "#ffffff" : "#d0d0d0"
                        border.color: "#a8a8a8"
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.receiving ? "LIVE  " + liveWaveform.endpoint
                          : (liveWaveform.endpoint !== "" ? "Waiting for records  " + liveWaveform.endpoint : "No SeedLink backbone")
                    color: root.receiving ? "#126b22" : "#777777"
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
                    var top = 4
                    var bottom = 48
                    var plotW = Math.max(1, width - left - right)
                    var plotH = Math.max(1, height - top - bottom)
                    var rowH = Math.max(22, Math.min(34, plotH / Math.max(12, rows.length)))
                    var visibleRows = Math.min(rows.length, Math.floor(plotH / rowH))
                    var tEnd = Math.max(root.windowSecs, root.latestRelT + 1)
                    var tStart = Math.max(0, tEnd - root.windowSecs)
                    var tRange = Math.max(1, tEnd - tStart)

                    // Ask the controller to decimate to our current pixel width, then
                    // index the ready envelopes by stream so each row can look up its
                    // channel's min/max columns. All heavy work already happened in C++.
                    liveWaveform.setViewport(Math.round(plotW), root.windowSecs)
                    var envs = liveWaveform.envelopes
                    var envIdx = {}
                    for (var ei = 0; ei < envs.length; ei++)
                        envIdx[String(envs[ei].station) + "." + String(envs[ei].sensor)] = envs[ei]

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

                    if (rows.length === 0) {
                        ctx.fillStyle = "#777777"
                        ctx.font = "12px sans-serif"
                        ctx.textAlign = "center"
                        ctx.textBaseline = "middle"
                        ctx.fillText("Waiting for waveform streams", left + plotW / 2, top + Math.min(160, plotH / 2))
                    }

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

                        // The envelope channels are ordered by component (0/1/2), so
                        // match by index — the row's real channel code (HNZ vs EHZ) need
                        // not equal the envelope's, only the axis.
                        var envStream = envIdx[row.station + "." + row.sensor]
                        var cd = (envStream && envStream.chans) ? envStream.chans[row.compIndex] : null
                        var mean = cd ? cd.mean : 0
                        var peak = cd ? cd.peak : 1e-9
                        var rms  = cd ? cd.rms : 0
                        // Robust auto-scale: fill the row at ~4x RMS, never past the true
                        // peak; a lone spike clips (samples are clamped to the row band).
                        var span = rms > 0 ? Math.min(peak, 4 * rms) : peak
                        if (span < 1e-9) span = 1e-9
                        var scale = rowH * 0.40 / span

                        // Left column: FDSN identity (net.station · channel), sensor
                        // location, and live amax right-aligned.
                        ctx.fillStyle = "#111111"
                        ctx.font = "bold 11px sans-serif"
                        ctx.textAlign = "left"
                        ctx.fillText(row.network + "." + row.stationName, 11, mid)
                        ctx.fillStyle = "#202020"
                        ctx.font = "bold 10px monospace"
                        ctx.fillText(row.channel, 108, mid)
                        if (row.location) {
                            ctx.fillStyle = "#8a8a8a"
                            ctx.font = "9px sans-serif"
                            ctx.fillText(String(row.location).slice(0, 16), 11, mid + 11)
                        }
                        ctx.fillStyle = "#6f6f75"
                        ctx.font = "10px monospace"
                        ctx.textAlign = "right"
                        ctx.fillText("amax " + peak.toExponential(1), left - 6, mid - 6)
                        ctx.fillText("mean " + mean.toExponential(1), left - 6, mid + 7)
                        ctx.textAlign = "left"

                        // Draw the C++-decimated columns: a min..max bar per pixel plus a
                        // mean line. The controller already reduced the window to `cols`
                        // columns, so there is no raw-sample iteration here.
                        if (cd) {
                            var yTop = mid - rowH * 0.5 + 1
                            var yBot = mid + rowH * 0.5 - 1
                            var mnA = cd.mn, mxA = cd.mx
                            var ncol = mnA.length
                            var colW = plotW / ncol
                            ctx.strokeStyle = row.color
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            for (var b = 0; b < ncol; b++) {
                                var lo = mnA[b]
                                if (lo === undefined || lo === null) continue
                                var xb = left + b * colW
                                var yhi = mid - (mxA[b] - mean) * scale
                                var ylo = mid - (lo - mean) * scale
                                if (yhi < yTop) yhi = yTop; else if (yhi > yBot) yhi = yBot
                                if (ylo < yTop) ylo = yTop; else if (ylo > yBot) ylo = yBot
                                ctx.moveTo(xb, yhi); ctx.lineTo(xb, ylo)
                            }
                            ctx.stroke()
                            ctx.beginPath()
                            var started = false
                            for (var b2 = 0; b2 < ncol; b2++) {
                                var lo2 = mnA[b2]
                                if (lo2 === undefined || lo2 === null) continue
                                var mv = (lo2 + mxA[b2]) * 0.5
                                var xm = left + b2 * colW
                                var ym = mid - (mv - mean) * scale
                                if (ym < yTop) ym = yTop; else if (ym > yBot) ym = yBot
                                if (!started) { ctx.moveTo(xm, ym); started = true } else ctx.lineTo(xm, ym)
                            }
                            if (started) ctx.stroke()
                        }

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
                    ctx.fillText(new Date().toISOString().slice(0, 10), left + 80, top + visibleRows * rowH + 34)
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
                    text: root.receiving
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
