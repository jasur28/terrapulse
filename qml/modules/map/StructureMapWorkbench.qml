import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    // Independent layers (SeisComp scmv model): one active colour layer + additive
    // overlays, instead of mutually-exclusive modes.
    property int colorLayer: 0            // 0 = network health, 1 = ground motion, 2 = QC
    property bool layerEvents: true       // overlay structural events on the map
    property bool eventTableVisible: false
    property int selected: -1
    property bool legendVisible: true
    property bool annotations: true
    property bool grayscaleMap: false
    property var events: []
    readonly property var layerNames: ["Network health", "Ground motion", "Quality control"]

    function gmColor(value, hasData, disabled) {
        if (disabled) return "#f2f2f2"
        if (!hasData) return "#000000"
        if (value < 0.2) return "#002bff"
        if (value < 0.4) return "#00a7ff"
        if (value < 0.8) return "#00e5ff"
        if (value < 1.5) return "#00ff3b"
        if (value < 4.0) return "#fff000"
        if (value < 12.0) return "#ffc400"
        if (value < 30.0) return "#ff9400"
        if (value < 60.0) return "#ff0000"
        return "#a00040"
    }

    function statusColor(m) {
        if (!m || !m.hasData) return colorLayer === 0 ? "#000000" : Theme.colorOffline
        if (colorLayer === 2) {
            return m.warning >= 2 ? Theme.colorCritical
                 : m.warning >= 1 ? Theme.colorWarning
                 : Theme.colorNormal
        }
        var gm = Math.max(Math.abs(Number(m.lastX || 0)), Math.abs(Number(m.lastY || 0)), Math.abs(Number(m.lastZ || 0)))
        if (colorLayer === 1) return gmColor(gm, m.hasData, false)
        return m.warning >= 2 ? Theme.colorCritical
             : m.warning >= 1 ? Theme.colorWarning
             : "#00e676"
    }

    function stationMarkers() {
        var out = []
        var s = inventory.structures || []
        for (var i = 0; i < s.length; i++) {
            var item = s[i]
            out.push({
                lon: item.lon,
                lat: item.lat,
                color: statusColor(item),
                outline: item.warning >= 2 ? "#b00000" : "#111111",
                label: annotations ? (item.name !== undefined ? item.name : ("TP." + item.objectId)) : "",
                alwaysLabel: annotations && i < 40,
                shape: !item.hasData && colorLayer === 0 ? "disabled" : "station",
                selected: selected === i,
                objectId: item.objectId,
                station: item.name || ("OBJ" + item.objectId),
                type: "station"
            })
        }
        if (out.length === 0) {
            out = [
                { lon: 69.2797, lat: 41.3111, color: "#00ff3b", label: "TP01", alwaysLabel: true, shape: "station", type: "station", objectId: 1 },
                { lon: 69.2430, lat: 41.3260, color: "#002bff", label: "TP02", alwaysLabel: true, shape: "station", type: "station", objectId: 2 },
                { lon: 69.3380, lat: 41.2990, color: "#fff000", label: "TP03", alwaysLabel: true, shape: "station", type: "station", objectId: 3 }
            ]
        }
        return out
    }

    function eventMarkers() {
        var out = []
        for (var i = 0; i < events.length; i++) {
            var e = events[i]
            out.push({
                lon: e.lon,
                lat: e.lat,
                color: severityColor(e.severity),
                outline: "#111111",
                label: e.id,
                alwaysLabel: i < 8,
                shape: "event",
                pulse: i === 0,
                type: "event",
                eventIndex: i
            })
        }
        return out
    }

    function markerList() {
        var out = stationMarkers()
        if (layerEvents) out = out.concat(eventMarkers())
        return out
    }

    function severityColor(level) {
        return level >= 3 ? Theme.colorCritical
             : level >= 2 ? Theme.colorHigh
             : level >= 1 ? Theme.colorWarning
             : Theme.colorNormal
    }

    function createDemoEvents() {
        if (events.length > 0) return
        var now = Date.now()
        events = [
            { id: "tp20260723a", time: now - 65000, age: "1m 5s", certainty: "", type: "Vibration", m: "2.4", mtype: "PGA", phases: 9, rms: 0.2, azgap: 96, lat: 41.3111, lon: 69.2797, depth: "18 m", dtype: "from sensors", stat: "C", region: "Tashkent Structure Region", severity: 2 },
            { id: "tp20260723b", time: now - 1180000, age: "19m", certainty: "suspected", type: "Resonance", m: "1.1", mtype: "PSA", phases: 7, rms: 0.4, azgap: 118, lat: 41.3260, lon: 69.2430, depth: "11 m", dtype: "from sensors", stat: "A", region: "Industrial District", severity: 1 }
        ]
    }

    Component.onCompleted: createDemoEvents()

    Connections {
        target: appController
        function onEventReceived(e) {
            var row = {
                id: String(e.eventId),
                time: e.tStart || Date.now(),
                age: "now",
                certainty: "",
                type: e.anomalyTypes || "Grouped",
                m: String(e.severity || 0),
                mtype: "TP",
                phases: e.sensorCount || 0,
                rms: 0.0,
                azgap: 0,
                lat: 41.3111 + (Number(e.objectId || 0) % 10) * 0.004,
                lon: 69.2797 + (Number(e.objectId || 0) % 10) * 0.004,
                depth: "0 m",
                dtype: "operator assigned",
                stat: e.statusName || "C",
                region: "Structure " + e.objectId,
                severity: e.severity || 1
            }
            events = [row].concat(events).slice(0, 300)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 0
            visible: false
            color: Theme.surface
            border.color: Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Text { text: "Colour:"; color: Theme.textSecondary; font.pixelSize: 12 }
                ClassicToolButton { text: "Network"; checkable: true; checked: root.colorLayer === 0; onClicked: root.colorLayer = 0 }
                ClassicToolButton { text: "QC"; checkable: true; checked: root.colorLayer === 2; onClicked: root.colorLayer = 2 }
                ClassicToolButton { text: "Ground motion"; checkable: true; checked: root.colorLayer === 1; onClicked: root.colorLayer = 1 }

                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 22; color: Theme.border }

                Text { text: "Layers:"; color: Theme.textSecondary; font.pixelSize: 12 }
                ClassicToolButton { text: "Events (" + root.events.length + ")"; checkable: true; checked: root.layerEvents; onClicked: root.layerEvents = checked }
                ClassicToolButton { text: "Annotations"; checkable: true; checked: root.annotations; onClicked: root.annotations = checked }

                Item { Layout.fillWidth: true }
                ClassicToolButton { text: "Legend"; checkable: true; checked: root.legendVisible; onClicked: root.legendVisible = checked }
                ClassicToolButton { text: "Grayscale"; checkable: true; checked: root.grayscaleMap; onClicked: root.grayscaleMap = checked }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Map with the active layers drawn on the OSM base.
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#d0d0d0"
                border.color: "#8d8d8d"

                MapView {
                    id: mapView
                    anchors.fill: parent
                    anchors.margins: 2
                    provider: "osm"
                    grayscale: root.grayscaleMap
                    drawGrid: true
                    markers: root.markerList()
                    showLabels: root.annotations
                    tilesUrl: (typeof mapsUrl !== "undefined" && mapsUrl !== "") ? mapsUrl + "/osm" : ""
                    onMarkerClicked: function(index) {
                        var m = markers[index]
                        if (!m) return
                        if (m.type === "event") { root.layerEvents = true }
                        else { root.selected = index }
                    }
                }

                MapLegend {
                    visible: root.legendVisible
                    mode: root.colorLayer
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 14
                }

                StationInspector {
                    visible: root.selected >= 0
                    marker: root.markerList()[root.selected]
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    onCloseRequested: root.selected = -1
                }
            }

            Rectangle {
                visible: root.eventTableVisible
                Layout.preferredWidth: visible ? 360 : 0
                Layout.fillHeight: true
                color: Theme.surface
                border.color: Theme.border

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        color: Theme.navBg
                        border.color: Theme.border
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: "Events (" + root.events.length + ")"
                            font.pixelSize: 11; font.bold: true; color: Theme.textPrimary
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        color: Theme.background
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            spacing: 0
                            property var w: [92, 196, 42]
                            Repeater {
                                model: ["Time", "Type", "Sev"]
                                Text {
                                    width: parent.w[index]; height: parent.height
                                    text: modelData; font.pixelSize: 10; color: Theme.textSecondary
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.events

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 24
                            color: index % 2 === 0 ? Theme.surfaceAlt : Theme.surface
                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                spacing: 0
                                property var w: [92, 196, 42]
                                Text {
                                    width: parent.w[0]; height: parent.height
                                    text: new Date(modelData.time).toLocaleTimeString(Qt.locale(), "hh:mm:ss")
                                    font.pixelSize: 11; color: Theme.textPrimary; verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    width: parent.w[1]; height: parent.height
                                    text: (modelData.type || "") + "  " + (modelData.region || "")
                                    font.pixelSize: 11; color: Theme.textSecondary
                                    verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                                }
                                Item {
                                    width: parent.w[2]; height: parent.height
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 9; height: 9; radius: 4.5
                                        color: root.severityColor(modelData.severity)
                                    }
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                }
            }
        }
    }

    component MapLegend: Rectangle {
        property int mode: 0
        width: 160
        height: mode === 1 ? 354 : mode === 2 ? 158 : 178
        color: Qt.rgba(1, 1, 1, 0.90)
        border.color: "#202020"
        z: 10

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Text {
                width: parent.width
                text: mode === 1 ? "Velocity in mm/s" : mode === 2 ? "Quality control" : "Network"
                color: "#000000"
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Repeater {
                model: mode === 1 ? [
                    { c: "#f2f2f2", t: "Station is disabled", shape: "disabled" },
                    { c: "#000000", t: "Not Set" },
                    { c: "#002bff", t: "[0,200]" },
                    { c: "#00a7ff", t: "400" },
                    { c: "#00e5ff", t: "800" },
                    { c: "#00ff3b", t: "1500" },
                    { c: "#fff000", t: "4000" },
                    { c: "#ffc400", t: "12000" },
                    { c: "#ff9400", t: "30000" },
                    { c: "#ff0000", t: "60000" },
                    { c: "#a00040", t: "> 150000" }
                ] : mode === 2 ? [
                    { c: Theme.colorNormal, t: "QC ok" },
                    { c: Theme.colorWarning, t: "Latency / gaps" },
                    { c: Theme.colorCritical, t: "QC error" },
                    { c: Theme.colorOffline, t: "No data" }
                ] : [
                    { c: "#00e676", t: "Station ok" },
                    { c: "#000000", t: "No bindings/data" },
                    { c: Theme.colorWarning, t: "Warning" },
                    { c: Theme.colorCritical, t: "Critical" },
                    { c: Theme.colorWarning, t: "Event", shape: "event" }
                ]

                Row {
                    spacing: 8
                    width: parent.width
                    height: 20

                    Canvas {
                        width: 20
                        height: 20
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.fillStyle = modelData.c
                            ctx.strokeStyle = "#000000"
                            if (modelData.shape === "event") {
                                ctx.beginPath(); ctx.arc(10, 10, 6, 0, Math.PI * 2); ctx.fill(); ctx.stroke()
                            } else {
                                ctx.beginPath(); ctx.moveTo(10, 2); ctx.lineTo(2, 17); ctx.lineTo(18, 17); ctx.closePath(); ctx.fill(); ctx.stroke()
                            }
                        }
                    }

                    Text {
                        width: parent.width - 30
                        text: modelData.t
                        color: "#000000"
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    component StationInspector: Rectangle {
        id: inspector
        property var marker: null
        signal closeRequested()
        width: 270
        height: 180
        color: Qt.rgba(1, 1, 1, 0.94)
        border.color: "#8d8d8d"
        z: 11

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: marker ? marker.station : "Station"
                    color: "#111111"
                    font.pixelSize: 15
                    font.bold: true
                    elide: Text.ElideRight
                }
                ClassicToolButton {
                    text: "x"
                    implicitWidth: 28
                    onClicked: inspector.closeRequested()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                Repeater {
                    model: marker ? [
                        "Object:", marker.objectId,
                        "Network:", "TP",
                        "Stream:", "TP." + marker.station + ".01.HN?",
                        "Colour layer:", root.layerNames[root.colorLayer],
                        "Status:", marker.shape === "disabled" ? "configuration/data issue" : "available"
                    ] : []
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: index % 2 === 0 ? "#555555" : "#111111"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#f4f4f4"
                border.color: "#c8c8c8"

                Canvas {
                    anchors.fill: parent
                    anchors.margins: 4
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = "#777777"
                        ctx.beginPath()
                        for (var x = 0; x < width; x++) {
                            var y = height / 2 + Math.sin(x * 0.22) * 8 * Math.exp(-Math.max(0, x - 80) / 90)
                            if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                        }
                        ctx.stroke()
                    }
                }
            }
        }
    }

    component EventsView: Rectangle {
        id: eventView
        property var events: []
        signal showOnMap(int index)
        color: "#eeeeee"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#ffffff"
                border.color: "#3aa9ff"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        color: "#f7f7f7"
                        Row {
                            anchors.fill: parent
                            property var widths: [180, 70, 100, 90, 60, 70, 70, 80, 75, 75, 80, 130, 140, 180]
                            Repeater {
                                model: ["OT (UTC)", "TimeAgo", "Certainty", "Type", "M", "MType", "Phases", "RMS (s)", "AzGap", "Lat", "Lon", "Depth", "DType", "Region", "ID"]
                                Text {
                                    width: index < parent.widths.length ? parent.widths[index] : 120
                                    height: parent.height
                                    text: modelData
                                    color: "#111111"
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 6
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: eventView.events
                        clip: true

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 24
                            color: index % 2 === 0 ? "#ffffff" : "#f3f3f3"
                            MouseArea {
                                anchors.fill: parent
                                onDoubleClicked: eventView.showOnMap(index)
                            }

                            Row {
                                anchors.fill: parent
                                property var widths: [180, 70, 100, 90, 60, 70, 70, 80, 75, 75, 80, 130, 140, 180]
                                property var cells: [
                                    new Date(modelData.time).toISOString().replace("T", " ").slice(0, 23),
                                    modelData.age,
                                    modelData.certainty,
                                    modelData.type,
                                    modelData.m,
                                    modelData.mtype,
                                    modelData.phases,
                                    modelData.rms,
                                    modelData.azgap,
                                    Number(modelData.lat).toFixed(2),
                                    Number(modelData.lon).toFixed(2),
                                    modelData.depth,
                                    modelData.dtype,
                                    modelData.region,
                                    modelData.id
                                ]
                                Repeater {
                                    model: parent.cells
                                    Text {
                                        width: index < parent.widths.length ? parent.widths[index] : 120
                                        height: parent.height
                                        text: modelData
                                        color: index === 4 || index === 5 ? "#004e9a" : index === 8 ? "#d00000" : "#111111"
                                        font.pixelSize: 12
                                        font.bold: index === 4
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 6
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                spacing: 8
                ClassicToolButton { text: "Clear list" }
                ClassicToolButton { text: "\u25bc" }
                Item { Layout.fillWidth: true }
                Text { text: "Last days:"; color: "#111111"; font.pixelSize: 12 }
                SpinBox { from: 1; to: 365; value: 1 }
                ClassicToolButton { text: "Read" }
                Text { text: "From:"; color: "#111111"; font.pixelSize: 12 }
                TextField { Layout.preferredWidth: 160; text: new Date(Date.now() - 86400000).toISOString().replace("T", " ").slice(0, 19) }
                Text { text: "To:"; color: "#111111"; font.pixelSize: 12 }
                TextField { Layout.preferredWidth: 160; text: new Date().toISOString().replace("T", " ").slice(0, 19) }
                ClassicToolButton { text: "Read" }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                spacing: 10
                CheckBox { checked: true; text: "Hide other/fake events" }
                CheckBox { text: "Show only own events" }
                CheckBox { text: "Hide events" }
                ComboBox { Layout.preferredWidth: 90; model: ["outside", "inside"] }
                ComboBox { Layout.preferredWidth: 120; model: ["- custom -", "city", "site"] }
                ClassicToolButton { text: "..." }
                CheckBox { text: "Hide new events" }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
