import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property int currentTab: 0
    property int selectedIndex: 0
    property var events: []
    property var groupedEvents: []
    property var arrivals: []
    property var wfparams: ({})
    property var selectedEvent: events.length > 0 ? events[Math.max(0, Math.min(selectedIndex, events.length - 1))] : null

    readonly property var tabNames: ["Location", "Magnitudes", "Event", "Events (" + Math.max(events.length, groupedEvents.length) + ")"]

    function severityColor(level) {
        return level >= 3 ? Theme.colorCritical
             : level >= 2 ? Theme.colorHigh
             : level >= 1 ? Theme.colorWarning
             : Theme.colorNormal
    }

    function ensureDemoEvents() {
        if (events.length > 0) return
        var now = Date.now()
        events = [
            { id: "tp20260723a", objectId: 1, sensorId: 1, type: "Vibration", severity: 2, status: "automatic", time: now - 480000, lat: 41.3111, lon: 69.2797, depth: 18.4, rms: 0.18, gap: 96, minDist: 0.42, phases: 9, agency: "TerraPulse", author: "tpproc@local", method: "TPLOC", region: "Tashkent Structure Region", pga: 0.24, pgv: 1.7, psa: 0.42 },
            { id: "tp20260723b", objectId: 2, sensorId: 1, type: "Resonance", severity: 1, status: "preliminary", time: now - 920000, lat: 41.3260, lon: 69.2430, depth: 11.2, rms: 0.27, gap: 118, minDist: 0.76, phases: 7, agency: "TerraPulse", author: "tpwfparam@local", method: "TPLOC", region: "Industrial District", pga: 0.11, pgv: 0.9, psa: 0.31 }
        ]
        rebuildArrivals()
    }

    function eventTime(e) {
        return e ? new Date(e.time).toLocaleString(Qt.locale(), "yyyy-MM-dd hh:mm:ss") : "-"
    }

    // Which sensors detected this structural event, their trigger time and the
    // strong motion they saw. Accelerometer domain — sensor detections, not
    // seismic P/S phase arrivals.
    function rebuildArrivals() {
        var e = selectedEvent || events[0]
        if (!e) {
            arrivals = []
            return
        }
        var out = []
        var names = ["Bridge-A", "Tower-2", "Dam-3", "Tunnel-4", "Bldg-5", "Bridge-6", "Tower-7", "Dam-8"]
        var axis = ["X", "Y", "Z"]
        for (var i = 0; i < names.length; i++) {
            var dist = 12 + i * 9 + (e.objectId || 0) * 2          // metres from the event
            out.push({
                used: i < 6,
                status: i % 3 === 0 ? "trigger" : "auto",
                phase: axis[i % 3],                                // dominant axis
                net: "TP",
                sta: names[i],
                loccha: "01/HN" + axis[i % 3],
                timeres: ((i % 5) - 2) * 0.04,                     // onset residual (s)
                dis: dist,
                az: (42 + i * 31) % 360,
                pga: Math.max(0.02, 0.30 - i * 0.03),              // gal
                pgv: Math.max(0.1, 1.9 - i * 0.18),                // cm/s
                snr: Math.max(1.2, 9.0 - i * 0.7),
                time: new Date(e.time + 200 + i * 420).toLocaleTimeString(Qt.locale(), "hh:mm:ss.zzz")
            })
        }
        arrivals = out
    }

    function eventMarkerList() {
        var out = []
        var s = inventory.structures || []
        for (var i = 0; i < s.length; i++) {
            out.push({
                lon: s[i].lon,
                lat: s[i].lat,
                color: s[i].warning >= 2 ? Theme.colorCritical : s[i].warning >= 1 ? Theme.colorWarning : Theme.colorNormal,
                label: s[i].name || ("Object " + s[i].objectId),
                pulse: selectedEvent && Number(s[i].objectId) === Number(selectedEvent.objectId)
            })
        }
        if (selectedEvent) {
            out.push({
                lon: selectedEvent.lon,
                lat: selectedEvent.lat,
                color: Theme.colorWarning,
                label: selectedEvent.id,
                pulse: true
            })
        }
        return out
    }

    function selectEvent(index) {
        selectedIndex = Math.max(0, Math.min(index, events.length - 1))
        rebuildArrivals()
    }

    Component.onCompleted: ensureDemoEvents()

    Connections {
        target: appController
        function onShfReceived(shf) {
            var row = {
                id: "shf-" + shf.shfId,
                objectId: shf.objectId,
                sensorId: shf.sensorId,
                type: shf.anomalyTypeName || "Anomaly",
                severity: shf.severity || 1,
                status: shf.statusName || "automatic",
                time: shf.anomalyStartTime || Date.now(),
                lat: 41.3111 + (Number(shf.objectId || 0) % 10) * 0.004,
                lon: 69.2797 + (Number(shf.sensorId || 0) % 10) * 0.004,
                depth: 0.0,
                rms: 1.0 - Math.min(0.95, shf.confidenceLevel || 0.5),
                gap: 0,
                minDist: 0,
                phases: 3,
                agency: "TerraPulse",
                author: "tpproc",
                method: "TPLOC",
                region: "Structure " + shf.objectId,
                pga: 0,
                pgv: 0,
                psa: 0
            }
            events = [row].concat(events).slice(0, 200)
            selectedIndex = 0
            rebuildArrivals()
        }

        function onEventReceived(e) {
            groupedEvents = [{
                id: String(e.eventId),
                objectId: e.objectId,
                time: e.tStart || Date.now(),
                type: e.anomalyTypes || "Grouped",
                severity: e.severity || 1,
                status: e.statusName || "active",
                sensors: e.sensorCount || 0,
                detections: e.shfCount || 0
            }].concat(groupedEvents).slice(0, 200)
        }

        function onWfparamReceived(w) {
            wfparams = w
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 0
            visible: false
            color: "#eeeeee"
            border.color: "#b6b6b6"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                ClassicToolButton { text: "\u21b6"; enabled: root.selectedIndex < root.events.length - 1; onClicked: root.selectEvent(root.selectedIndex + 1) }
                ClassicToolButton { text: "\u21b7"; enabled: root.selectedIndex > 0; onClicked: root.selectEvent(root.selectedIndex - 1) }
                ClassicToolButton { text: "Previous event"; enabled: root.selectedIndex < root.events.length - 1; onClicked: root.selectEvent(root.selectedIndex + 1) }
                ClassicToolButton { text: "Next event"; enabled: root.selectedIndex > 0; onClicked: root.selectEvent(root.selectedIndex - 1) }
                Item { Layout.fillWidth: true }
                Text {
                    text: selectedEvent ? selectedEvent.id + "  " + eventTime(selectedEvent) : "No event selected"
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            currentIndex: root.currentTab
            onCurrentIndexChanged: root.currentTab = currentIndex
            Repeater {
                model: root.tabNames
                TabButton {
                    id: reviewTab
                    text: modelData
                    width: Math.max(88, label.implicitWidth + 22)
                    height: 28
                    contentItem: Text {
                        id: label
                        text: reviewTab.text
                        color: reviewTab.checked ? "#101010" : "#333333"
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        color: reviewTab.checked ? "#ffffff" : "#d0d0d0"
                        border.color: "#a8a8a8"
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentTab

            Item {
                id: locationTab

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 320
                        spacing: 6

                        ColumnLayout {
                            Layout.preferredWidth: 545
                            Layout.fillHeight: true
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                text: selectedEvent ? selectedEvent.region : "Structural Event Region"
                                color: "#202020"
                                font.pixelSize: 16
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                border.color: "#a8a8a8"
                                color: "#d8d8d8"
                                clip: true

                                MapView {
                                    anchors.fill: parent
                                    provider: "osm"
                                    markers: root.eventMarkerList()
                                    tilesUrl: (typeof mapsUrl !== "undefined" && mapsUrl !== "") ? mapsUrl + "/osm" : ""
                                }
                            }
                        }

                        GridLayout {
                            Layout.preferredWidth: 250
                            Layout.fillHeight: true
                            columns: 2
                            rowSpacing: 7
                            columnSpacing: 8

                            Repeater {
                                model: selectedEvent ? [
                                    "Time:", root.eventTime(selectedEvent),
                                    "Depth:", selectedEvent.depth.toFixed(1) + " m",
                                    "Lat:", selectedEvent.lat.toFixed(4),
                                    "Lon:", selectedEvent.lon.toFixed(4),
                                    "Sensors:", "" + selectedEvent.phases,
                                    "Detection RMS:", selectedEvent.rms.toFixed(2),
                                    "Sensor gap:", selectedEvent.gap + " deg",
                                    "Nearest:", (selectedEvent.minDist * 1000).toFixed(0) + " m",
                                    "EventID:", selectedEvent.id,
                                    "Agency:", selectedEvent.agency,
                                    "Author:", selectedEvent.author,
                                    "Evaluation:", selectedEvent.status,
                                    "Method:", selectedEvent.method,
                                    "Updated:", root.eventTime(selectedEvent)
                                ] : []

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData
                                    color: index % 2 === 0 ? "#202020" : "#111111"
                                    font.pixelSize: 12
                                    font.bold: index % 2 === 1 && (modelData === selectedEvent.id || modelData === selectedEvent.agency)
                                    horizontalAlignment: index % 2 === 0 ? Text.AlignRight : Text.AlignLeft
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            border.color: "#a8a8a8"
                            color: "#ffffff"

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0

                                TabBar {
                                    Layout.fillWidth: true
                                    currentIndex: 2
                                    TabButton { text: "Distance" }
                                    TabButton { text: "Azimuth" }
                                    TabButton { text: "Onset" }
                                    TabButton { text: "Amplitude" }
                                    TabButton { text: "Polar" }
                                }

                                ResidualPlot {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    arrivals: root.arrivals
                                }
                            }
                        }
                    }

                    ArrivalTable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        arrivals: root.arrivals
                    }
                }
            }

            MagnitudesTab {
                selectedEvent: root.selectedEvent
                wfparams: root.wfparams
                arrivals: root.arrivals
            }

            EventTab {
                selectedEvent: root.selectedEvent
                events: root.events
            }

            EventsTab {
                events: root.events
                groupedEvents: root.groupedEvents
                onSelectRequested: function(index) {
                    root.currentTab = 0
                    root.selectEvent(index)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 66
            color: "#eeeeee"
            border.color: "#b6b6b6"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 5
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    ComboBox { Layout.preferredWidth: 140; model: ["TPLOC", "Grid search", "Least squares"] }
                    ClassicToolButton { text: "\u2699" }
                    Text { text: "Profile:"; color: "#202020"; font.pixelSize: 12 }
                    ComboBox { Layout.preferredWidth: 130; model: ["structure", "bridge", "building", "tower"] }
                    CheckBox { text: "Fix depth" }
                    TextField { Layout.preferredWidth: 150; text: selectedEvent ? selectedEvent.depth.toFixed(1) : "0"; enabled: false }
                    Text { text: "m"; color: "#202020"; font.pixelSize: 12 }
                    CheckBox { text: "Distance cutoff" }
                    TextField { Layout.preferredWidth: 90; text: "500"; enabled: false }
                    Text { text: "m"; color: "#202020"; font.pixelSize: 12 }
                    CheckBox { text: "Ignore initial location" }
                    Item { Layout.fillWidth: true }
                    ClassicToolButton { text: "\u2630" }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    ClassicToolButton { text: "Relocate" }
                    ClassicToolButton { text: "\u25b6 Run..." }
                    ComboBox { Layout.preferredWidth: 300; enabled: false; model: ["depth type set by locator"] }
                    Item { Layout.fillWidth: true }
                    ClassicToolButton { text: "Picker"; onClicked: root.currentTab = 0 }
                    ClassicToolButton { text: "Import picks" }
                    ClassicToolButton { text: "Compute magnitudes"; enabled: root.selectedEvent !== null }
                    ClassicToolButton {
                        text: "Confirm"
                        success: true
                        enabled: root.selectedEvent !== null
                        onClicked: {
                            if (root.selectedEvent && typeof journalController !== "undefined")
                                journalController.sendAction(root.selectedEvent.id, "confirm")
                        }
                    }
                }
            }
        }
    }

    component ResidualPlot: Canvas {
        id: plot
        property var arrivals: []

        onArrivalsChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#ffffff"
            ctx.fillRect(0, 0, width, height)
            var left = 46, right = 16, top = 22, bottom = 34
            var pw = width - left - right
            var ph = height - top - bottom
            if (pw <= 0 || ph <= 0) return

            ctx.strokeStyle = "#bdbdbd"
            ctx.lineWidth = 1
            ctx.font = "11px sans-serif"
            ctx.fillStyle = "#202020"
            ctx.textAlign = "right"
            for (var y = 0; y <= 4; y++) {
                var gy = top + y * ph / 4
                ctx.beginPath(); ctx.moveTo(left, gy); ctx.lineTo(left + pw, gy); ctx.stroke()
                ctx.fillText((1 - y * 0.5).toFixed(1), left - 6, gy + 4)
            }
            ctx.textAlign = "center"
            for (var x = 0; x <= 5; x++) {
                var gx = left + x * pw / 5
                ctx.beginPath(); ctx.moveTo(gx, top); ctx.lineTo(gx, top + ph); ctx.stroke()
                ctx.fillText((x * 16).toFixed(0), gx, top + ph + 18)
            }

            for (var i = 0; i < arrivals.length; i++) {
                var a = arrivals[i]
                var px = left + Math.min(1, a.dis / 80) * pw
                var py = top + (1 - ((a.timeres + 1) / 2)) * ph
                ctx.fillStyle = a.phase === "X" ? Theme.seriesX : a.phase === "Y" ? Theme.seriesY : Theme.seriesZ
                ctx.beginPath(); ctx.arc(px, py, 4, 0, Math.PI * 2); ctx.fill()
            }

            ctx.strokeStyle = "#202020"
            ctx.strokeRect(left, top, pw, ph)
            ctx.fillStyle = "#202020"
            ctx.fillText("Distance (m)", left + pw / 2, height - 6)
        }
    }

    component ArrivalTable: Rectangle {
        id: table
        property var arrivals: []
        color: "#ffffff"
        border.color: "#b6b6b6"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: "#f5f5f5"
                Row {
                    anchors.fill: parent
                    Repeater {
                        model: ["Used", "Status", "Sensor", "Cha", "Trigger (UTC)", "PGA gal", "PGV", "SNR", "Dist m", "Resid s", "Axis"]
                        Text {
                            width: parent.width / 11
                            height: parent.height
                            text: modelData
                            color: "#202020"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: table.arrivals
                clip: true
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 30
                    color: index % 2 === 0 ? "#ffffff" : "#f0f0f0"
                    border.color: "#c8c8c8"
                    Row {
                        anchors.fill: parent
                        property var cells: [modelData.used ? "\u2611" : "\u2610", modelData.status, modelData.sta, modelData.loccha, modelData.time, modelData.pga.toFixed(3), modelData.pgv.toFixed(2), modelData.snr.toFixed(1), modelData.dis.toFixed(0), modelData.timeres.toFixed(2), modelData.phase]
                        Repeater {
                            model: parent.cells
                            Text {
                                width: parent.width / 11
                                height: parent.height
                                text: modelData
                                color: index === 0 ? "#006fb8" : "#202020"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    component MagnitudesTab: Item {
        property var selectedEvent: null
        property var wfparams: ({})
        property var arrivals: []

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                color: "#eeeeee"
                border.color: "#b6b6b6"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    ComboBox { Layout.preferredWidth: 90; model: ["Default", "Strong", "Raw"] }
                    CheckBox { text: "Min SNR:" }
                    TextField { Layout.preferredWidth: 80; text: "1.50"; enabled: false }
                    ComboBox { Layout.preferredWidth: 160; model: ["AbsMax", "PGA", "PGV", "PSA"] }
                    Item { Layout.fillWidth: true }
                    ClassicToolButton { text: "\u2714 Apply all"; success: true }
                }
            }

            WaveformPickerCanvas {
                Layout.fillWidth: true
                Layout.fillHeight: true
                selectedEvent: parent.selectedEvent
                arrivals: parent.arrivals
                amplitudeMode: true
            }
        }
    }

    component EventTab: Item {
        property var selectedEvent: null
        property var events: []

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            ArrivalTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                arrivals: root.arrivals
            }

            Rectangle {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                color: "#f5f5f5"
                border.color: "#b6b6b6"

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 10
                    Repeater {
                        model: selectedEvent ? [
                            "Time:", root.eventTime(selectedEvent),
                            "Region:", selectedEvent.region,
                            "Type:", selectedEvent.type,
                            "Depth:", selectedEvent.depth.toFixed(1) + " m",
                            "Latitude:", selectedEvent.lat.toFixed(4),
                            "Longitude:", selectedEvent.lon.toFixed(4),
                            "Sensors:", selectedEvent.phases,
                            "Detection RMS:", selectedEvent.rms.toFixed(2),
                            "Agency:", selectedEvent.agency,
                            "Origin Status:", selectedEvent.status
                        ] : []
                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: index % 2 === 0 ? "#202020" : "#111111"
                            font.pixelSize: 12
                            font.bold: index % 2 === 1
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    component EventsTab: Item {
        property var events: []
        property var groupedEvents: []
        signal selectRequested(int index)

        ArrivalTableLikeEvents {
            anchors.fill: parent
            anchors.margins: 8
            events: parent.events
            onSelectRequested: function(index) { parent.selectRequested(index) }
        }
    }

    component ArrivalTableLikeEvents: Rectangle {
        id: eventTable
        property var events: []
        signal selectRequested(int index)
        color: "#ffffff"
        border.color: "#b6b6b6"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: "#f5f5f5"
                Row {
                    anchors.fill: parent
                    Repeater {
                        model: ["Created (UTC)", "OT", "Sensors", "Lat", "Lon", "Depth", "RMS", "Stat", "Method", "Agency"]
                        Text {
                            width: parent.width / 10
                            height: parent.height
                            text: modelData
                            color: "#202020"
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: eventTable.events
                clip: true
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 28
                    color: index === root.selectedIndex ? "#b9ddf4" : (index % 2 === 0 ? "#ffffff" : "#f0f0f0")
                    MouseArea { anchors.fill: parent; onClicked: eventTable.selectRequested(index) }
                    Row {
                        anchors.fill: parent
                        property var cells: [root.eventTime(modelData), new Date(modelData.time).toLocaleTimeString(Qt.locale(), "hh:mm:ss"), modelData.phases, modelData.lat.toFixed(3), modelData.lon.toFixed(3), modelData.depth.toFixed(1), modelData.rms.toFixed(2), "A", modelData.method, modelData.agency]
                        Repeater {
                            model: parent.cells
                            Text {
                                width: parent.width / 10
                                height: parent.height
                                text: modelData
                                color: "#202020"
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }

    component WaveformPickerCanvas: Canvas {
        property var selectedEvent: null
        property var arrivals: []
        property bool amplitudeMode: false

        onSelectedEventChanged: requestPaint()
        onArrivalsChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#ffffff"
            ctx.fillRect(0, 0, width, height)

            var left = 122, right = 24, top = 32, bottom = 42
            var rows = Math.max(8, arrivals.length)
            var rowH = Math.max(34, (height - top - bottom) / rows)
            var plotW = width - left - right
            var visibleRows = Math.min(rows, Math.floor((height - top - bottom) / rowH))

            ctx.fillStyle = "#202020"
            ctx.font = "bold 12px sans-serif"
            ctx.fillText(selectedEvent ? ("Sensor detections · nearest " + (selectedEvent.minDist * 1000).toFixed(0) + " m") : "No selected event", 8, 18)

            for (var i = 0; i < visibleRows; i++) {
                var a = arrivals[i % Math.max(1, arrivals.length)] || { sta: "PB" + i, net: "TP", loccha: "HHZ", timeres: 0, phase: "P" }
                var y0 = top + i * rowH
                var mid = y0 + rowH / 2
                ctx.fillStyle = i % 2 === 0 ? "#eeeeee" : "#ffffff"
                ctx.fillRect(0, y0, width, rowH)
                ctx.fillStyle = "#202020"
                ctx.font = "bold 12px sans-serif"
                ctx.fillText(a.sta, 8, mid)
                ctx.font = "12px sans-serif"
                ctx.fillText(a.net, 52, mid)

                ctx.strokeStyle = "#d0d0d0"
                ctx.beginPath()
                ctx.moveTo(left, mid)
                ctx.lineTo(left + plotW, mid)
                ctx.stroke()

                var pickX = left + plotW * (0.20 + (i % 5) * 0.04)
                var arrivalX = left + plotW * (0.28 + (i % 7) * 0.035)
                ctx.strokeStyle = "#ff0000"
                ctx.beginPath(); ctx.moveTo(pickX, y0); ctx.lineTo(pickX, y0 + rowH); ctx.stroke()
                ctx.fillStyle = "#ff0000"; ctx.fillText("T", pickX + 3, y0 + 15)
                ctx.strokeStyle = "#0048ff"
                ctx.beginPath(); ctx.moveTo(arrivalX, y0); ctx.lineTo(arrivalX, y0 + rowH); ctx.stroke()
                ctx.fillStyle = "#0048ff"; ctx.fillText(a.phase, arrivalX + 3, y0 + 28)

                ctx.strokeStyle = "#8a8a8a"
                ctx.lineWidth = 1
                ctx.beginPath()
                for (var x = 0; x < plotW; x++) {
                    var t = x / plotW * Math.PI * 18
                    var env = Math.exp(-Math.max(0, x - plotW * 0.25) / Math.max(1, plotW * 0.18))
                    var amp = Math.sin(t + i) * (8 + i % 5) * env
                    if (amplitudeMode && x > plotW * 0.18 && x < plotW * 0.80) {
                        ctx.fillStyle = i % 3 === 0 ? "rgba(164,200,164,0.55)" : "rgba(210,180,180,0.45)"
                        ctx.fillRect(left + x, y0 + rowH - 10, 1, 9)
                    }
                    if (x === 0) ctx.moveTo(left + x, mid - amp)
                    else ctx.lineTo(left + x, mid - amp)
                }
                ctx.stroke()
            }

            ctx.strokeStyle = "#202020"
            ctx.beginPath()
            ctx.moveTo(left, top + visibleRows * rowH)
            ctx.lineTo(left + plotW, top + visibleRows * rowH)
            ctx.stroke()
        }
    }
}
