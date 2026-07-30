import QtQuick
import TerraPulse

Item {
    id: control
    clip: true

    property var markers: []
    property string tilesUrl: ""
    property real minSpanLon: 30

    // "seiscomp" = offline world relief (world<quad>.png, equirectangular)
    // "osm"      = OpenStreetMap street tiles (z/x/y, Web Mercator)
    property string provider: "osm"
    property int osmMinZoom: 0
    property int osmMaxZoom: 18
    property int maxVisibleTiles: 180
    property bool showLabels: markers.length <= 100
    property bool drawGrid: true
    property bool grayscale: false

    signal markerClicked(int index)

    property real centerLon: 0
    property real centerLat: 0
    property real spanLon: 360
    property bool initialized: false
    property int lastCount: -1

    property real vLonMin: -180
    property real vLonMax: 180
    property real vLatMin: -90
    property real vLatMax: 90
    property int zoomLevel: 0
    property var tiles: []

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v))
    }

    function minSpan() { return control.provider === "osm" ? 0.003 : 8 }
    function maxSpan() { return control.provider === "osm" ? 360 : 360 }

    // Web-Mercator helpers (OSM mode).
    function mercN(lat) {                       // normalized Y: 0 = north, 1 = south
        var r = control.clamp(lat, -85.05, 85.05) * Math.PI / 180
        return (1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2
    }
    function tileY2lat(y, n) {
        var m = Math.PI * (1 - 2 * y / n)
        return 180 / Math.PI * Math.atan(0.5 * (Math.exp(m) - Math.exp(-m)))
    }

    function tileBounds(quad) {
        var lonMin = -180
        var lonMax = 180
        var latMin = -90
        var latMax = 90

        for (var i = 0; i < quad.length; i++) {
            var ch = quad.charAt(i)
            var lonMid = (lonMin + lonMax) / 2
            var latMid = (latMin + latMax) / 2

            if (ch === "0") {          // NE  (SeisComp tile order: 0=NE 1=NW 2=SW 3=SE)
                lonMin = lonMid
                latMin = latMid
            } else if (ch === "1") {   // NW
                lonMax = lonMid
                latMin = latMid
            } else if (ch === "2") {
                lonMax = lonMid
                latMax = latMid
            } else {
                lonMin = lonMid
                latMax = latMid
            }
        }

        return { lonMin: lonMin, lonMax: lonMax, latMin: latMin, latMax: latMax }
    }

    function buildTiles() {
        var out = []

        if (control.provider === "osm") {
            var z = control.zoomLevel
            var n = Math.pow(2, z)
            var x1 = control.clamp(Math.floor((vLonMin + 180) / 360 * n), 0, n - 1)
            var x2 = control.clamp(Math.floor((vLonMax + 180) / 360 * n), 0, n - 1)
            var y1 = control.clamp(Math.floor(control.mercN(vLatMax) * n), 0, n - 1)
            var y2 = control.clamp(Math.floor(control.mercN(vLatMin) * n), 0, n - 1)
            while ((x2 - x1 + 1) * (y2 - y1 + 1) > control.maxVisibleTiles && z > control.osmMinZoom) {
                z--
                n = Math.pow(2, z)
                x1 = control.clamp(Math.floor((vLonMin + 180) / 360 * n), 0, n - 1)
                x2 = control.clamp(Math.floor((vLonMax + 180) / 360 * n), 0, n - 1)
                y1 = control.clamp(Math.floor(control.mercN(vLatMax) * n), 0, n - 1)
                y2 = control.clamp(Math.floor(control.mercN(vLatMin) * n), 0, n - 1)
            }
            control.zoomLevel = z
            for (var x = x1; x <= x2; x++) {
                for (var y = y1; y <= y2; y++) {
                    out.push({
                        src: "/" + z + "/" + x + "/" + y + ".png",
                        lonMin: x / n * 360 - 180,
                        lonMax: (x + 1) / n * 360 - 180,
                        latMax: control.tileY2lat(y, n),
                        latMin: control.tileY2lat(y + 1, n)
                    })
                }
            }
        } else {
            var level = control.zoomLevel
            var count = Math.pow(4, level)
            for (var k = 0; k < count; k++) {
                var q = ""
                var qx = k
                for (var j = 0; j < level; j++) {
                    q = (qx % 4) + q
                    qx = Math.floor(qx / 4)
                }
                var b = tileBounds(q)
                if (b.lonMax < vLonMin || b.lonMin > vLonMax ||
                    b.latMax < vLatMin || b.latMin > vLatMax) {
                    continue
                }
                out.push({
                    src: "/world" + q + ".png",
                    lonMin: b.lonMin, lonMax: b.lonMax,
                    latMin: b.latMin, latMax: b.latMax
                })
            }
        }

        control.tiles = out
    }

    function applyView() {
        if (width <= 0 || height <= 0) return

        var aspect = width / height
        spanLon = clamp(spanLon, minSpan(), maxSpan())
        var spanLat = spanLon / Math.max(0.1, aspect)

        if (spanLat > 180) {
            spanLat = 180
            spanLon = Math.min(maxSpan(), spanLat * aspect)
        }

        centerLon = clamp(centerLon, -180 + spanLon / 2, 180 - spanLon / 2)
        if (spanLat >= 170) {
            centerLat = 0
        } else {
            centerLat = clamp(centerLat, -85 + spanLat / 2, 85 - spanLat / 2)
        }

        vLonMin = centerLon - spanLon / 2
        vLonMax = centerLon + spanLon / 2
        vLatMin = centerLat - spanLat / 2
        vLatMax = centerLat + spanLat / 2

        if (control.provider === "osm") {
            var zz = Math.round(Math.log(width * 360 / (spanLon * 256)) / Math.log(2))
            zoomLevel = clamp(zz, osmMinZoom, osmMaxZoom)
        } else {
            zoomLevel = clamp(Math.round(Math.log(720 / spanLon) / Math.log(2)), 0, 4)
        }
        buildTiles()
    }

    function fit() {
        var m = control.markers || []

        if (m.length > 0) {
            var lo0 = 1e9
            var lo1 = -1e9
            var la0 = 1e9
            var la1 = -1e9

            for (var i = 0; i < m.length; i++) {
                var lo = Number(m[i].lon)
                var la = Number(m[i].lat)
                if (!isFinite(lo) || !isFinite(la)) continue
                lo0 = Math.min(lo0, lo)
                lo1 = Math.max(lo1, lo)
                la0 = Math.min(la0, la)
                la1 = Math.max(la1, la)
            }

            var defSpan = control.provider === "osm" ? 0.25 : control.minSpanLon
            if (lo0 < lo1 || la0 < la1) {
                centerLon = (lo0 + lo1) / 2
                centerLat = (la0 + la1) / 2
                spanLon = Math.max((lo1 - lo0) * 1.8, defSpan)
            } else if (lo0 < 1e9) {
                centerLon = lo0
                centerLat = la0
                spanLon = defSpan
            } else {
                centerLon = control.provider === "osm" ? 69.28 : 0
                centerLat = control.provider === "osm" ? 41.31 : 0
                spanLon = control.provider === "osm" ? 0.3 : 360
            }
        } else {
            centerLon = control.provider === "osm" ? 69.28 : 0
            centerLat = control.provider === "osm" ? 41.31 : 0
            spanLon = control.provider === "osm" ? 0.3 : 360
        }

        initialized = true
        applyView()
    }

    function zoomAt(deltaY, mx, my) {
        var lon = vLonMin + mx / width * (vLonMax - vLonMin)
        var lat = vLatMax - my / height * (vLatMax - vLatMin)

        spanLon = clamp(spanLon * (deltaY > 0 ? 0.83 : 1.20), minSpan(), maxSpan())
        var spanLat = spanLon / (width / height)
        centerLon = lon - (mx / width - 0.5) * spanLon
        centerLat = lat + (my / height - 0.5) * spanLat
        applyView()
    }

    function pan(dx, dy) {
        centerLon -= dx / width * spanLon
        centerLat += dy / height * (spanLon / (width / height))
        applyView()
    }

    function xOf(lon) {
        return (lon - vLonMin) / (vLonMax - vLonMin) * width
    }

    function yOf(lat) {
        if (control.provider === "osm") {
            var n0 = control.mercN(vLatMax)   // top
            var n1 = control.mercN(vLatMin)   // bottom
            return (control.mercN(lat) - n0) / (n1 - n0) * height
        }
        return (vLatMax - lat) / (vLatMax - vLatMin) * height
    }

    function markerAt(mx, my) {
        var best = -1
        var bestD = 999999
        var m = control.markers || []
        for (var i = 0; i < m.length; i++) {
            var x = control.xOf(Number(m[i].lon))
            var y = control.yOf(Number(m[i].lat))
            var d = Math.sqrt((mx - x) * (mx - x) + (my - y) * (my - y))
            var r = m[i].shape === "event" ? 14 : 11
            if (d <= r && d < bestD) {
                best = i
                bestD = d
            }
        }
        return best
    }

    function repaintOverlays() {
        gridCanvas.requestPaint()
        markerCanvas.requestPaint()
    }

    onMarkersChanged: {
        if (!initialized || (lastCount <= 0 && markers.length > 0)) fit()
        lastCount = markers.length
        repaintOverlays()
    }
    onWidthChanged: { initialized ? applyView() : fit(); repaintOverlays() }
    onHeightChanged: { initialized ? applyView() : fit(); repaintOverlays() }
    onVLonMinChanged: repaintOverlays()
    onVLonMaxChanged: repaintOverlays()
    onVLatMinChanged: repaintOverlays()
    onVLatMaxChanged: repaintOverlays()
    Component.onCompleted: { fit(); repaintOverlays() }

    Rectangle {
        anchors.fill: parent
        color: "#0b1325"
    }

    Repeater {
        model: control.tiles

        delegate: Image {
            x: control.xOf(modelData.lonMin)
            y: control.yOf(modelData.latMax)
            width: control.xOf(modelData.lonMax) - control.xOf(modelData.lonMin)
            height: control.yOf(modelData.latMin) - control.yOf(modelData.latMax)
            source: control.tilesUrl.length > 0 ? control.tilesUrl + modelData.src : ""
            fillMode: Image.Stretch
            smooth: true
            asynchronous: true
            cache: true
            opacity: control.grayscale ? 0.70 : 1.0
        }
    }

    Canvas {
        id: gridCanvas
        anchors.fill: parent
        visible: control.drawGrid
        z: 2

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = "rgba(255,255,255,0.58)"
            ctx.lineWidth = 1
            ctx.setLineDash([1, 3])
            ctx.fillStyle = "rgba(25,25,25,0.85)"
            ctx.font = "10px sans-serif"

            var lonStep = control.spanLon > 120 ? 30 : control.spanLon > 40 ? 10 : control.spanLon > 10 ? 2 : control.spanLon > 2 ? 0.5 : 0.1
            var latSpan = control.vLatMax - control.vLatMin
            var latStep = latSpan > 80 ? 20 : latSpan > 30 ? 10 : latSpan > 8 ? 2 : latSpan > 2 ? 0.5 : 0.1

            var lonStart = Math.ceil(control.vLonMin / lonStep) * lonStep
            for (var lon = lonStart; lon <= control.vLonMax; lon += lonStep) {
                var x = control.xOf(lon)
                ctx.beginPath()
                ctx.moveTo(x, 0)
                ctx.lineTo(x, height)
                ctx.stroke()
                ctx.fillText(Math.abs(lon).toFixed(lonStep < 1 ? 1 : 0) + (lon < 0 ? " W" : " E"), x + 4, 14)
            }

            var latStart = Math.ceil(control.vLatMin / latStep) * latStep
            for (var lat = latStart; lat <= control.vLatMax; lat += latStep) {
                var y = control.yOf(lat)
                ctx.beginPath()
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
                ctx.stroke()
                ctx.fillText(Math.abs(lat).toFixed(latStep < 1 ? 1 : 0) + (lat < 0 ? " S" : " N"), 5, y - 4)
            }
            ctx.setLineDash([])
        }
    }

    Canvas {
        id: markerCanvas
        anchors.fill: parent
        z: 5

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var m = control.markers || []
            ctx.font = "bold 10px sans-serif"
            ctx.textBaseline = "middle"

            for (var i = 0; i < m.length; i++) {
                var marker = m[i]
                var lon = Number(marker.lon)
                var lat = Number(marker.lat)
                if (!isFinite(lon) || !isFinite(lat)) continue
                if (lon < control.vLonMin || lon > control.vLonMax || lat < control.vLatMin || lat > control.vLatMax) continue

                var x = control.xOf(lon)
                var y = control.yOf(lat)
                var color = marker.color || Theme.colorOffline
                var outline = marker.outline || "#111111"
                var selected = marker.selected === true || marker.pulse === true

                if (selected) {
                    ctx.strokeStyle = marker.shape === "event" ? Theme.colorWarning : Theme.colorCritical
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    ctx.arc(x, y, marker.shape === "event" ? 14 : 12, 0, Math.PI * 2)
                    ctx.stroke()
                }

                ctx.fillStyle = color
                ctx.strokeStyle = outline
                ctx.lineWidth = 1.2

                if (marker.shape === "event") {
                    ctx.beginPath()
                    ctx.arc(x, y, 6, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.stroke()
                    ctx.strokeStyle = "rgba(120,0,120,0.55)"
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    ctx.arc(x, y, 11, 0, Math.PI * 2)
                    ctx.stroke()
                } else {
                    ctx.beginPath()
                    ctx.moveTo(x, y - 8)
                    ctx.lineTo(x - 7, y + 7)
                    ctx.lineTo(x + 7, y + 7)
                    ctx.closePath()
                    if (marker.shape === "disabled") ctx.fillStyle = "#f2f2f2"
                    ctx.fill()
                    ctx.stroke()
                    if (marker.shape === "disabled") {
                        ctx.beginPath()
                        ctx.moveTo(x - 5, y + 5)
                        ctx.lineTo(x + 5, y - 5)
                        ctx.stroke()
                    }
                }

                if (control.showLabels || marker.alwaysLabel) {
                    var text = marker.label || ""
                    ctx.lineWidth = 2
                    ctx.strokeStyle = "rgba(255,255,255,0.72)"
                    ctx.fillStyle = "#111111"
                    ctx.textAlign = "left"
                    ctx.strokeText(text, x + 9, y + 1)
                    ctx.fillText(text, x + 9, y + 1)
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        property real lastX: 0
        property real lastY: 0
        property bool dragging: false
        property bool moved: false
        cursorShape: dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        onPressed: (mouse) => {
            lastX = mouse.x
            lastY = mouse.y
            dragging = true
            moved = false
        }
        onReleased: (mouse) => {
            dragging = false
            if (!moved) {
                var hit = control.markerAt(mouse.x, mouse.y)
                if (hit >= 0) control.markerClicked(hit)
            }
        }
        onPositionChanged: (mouse) => {
            if (!dragging) return
            if (Math.abs(mouse.x - lastX) + Math.abs(mouse.y - lastY) > 2) moved = true
            control.pan(mouse.x - lastX, mouse.y - lastY)
            lastX = mouse.x
            lastY = mouse.y
        }
        onWheel: (wheel) => control.zoomAt(wheel.angleDelta.y, wheel.x, wheel.y)
        onDoubleClicked: control.fit()
    }

    Column {
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 12
        }
        spacing: 6
        z: 10

        Repeater {
            model: [
                { label: "+", action: 1 },
                { label: "-", action: -1 },
                { label: "[]", action: 0 }
            ]

            Rectangle {
                width: 28
                height: 28
                radius: 2
                color: Theme.surface
                border.color: Theme.border
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: Theme.textPrimary
                    font.pixelSize: modelData.action === 0 ? 12 : 18
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: modelData.action === 0
                               ? control.fit()
                               : control.zoomAt(modelData.action, control.width / 2, control.height / 2)
                }
            }
        }
    }
}
