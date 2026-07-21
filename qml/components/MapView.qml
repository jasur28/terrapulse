import QtQuick
import TerraPulse

Item {
    id: control
    clip: true

    property var markers: []
    property string tilesUrl: ""
    property real minSpanLon: 30

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
        var level = control.zoomLevel
        var count = Math.pow(4, level)
        var out = []

        for (var k = 0; k < count; k++) {
            var q = ""
            var x = k
            for (var j = 0; j < level; j++) {
                q = (x % 4) + q
                x = Math.floor(x / 4)
            }

            var b = tileBounds(q)
            if (b.lonMax < vLonMin || b.lonMin > vLonMax ||
                b.latMax < vLatMin || b.latMin > vLatMax) {
                continue
            }
            out.push({
                quad: q,
                lonMin: b.lonMin,
                lonMax: b.lonMax,
                latMin: b.latMin,
                latMax: b.latMax
            })
        }

        control.tiles = out
    }

    function applyView() {
        if (width <= 0 || height <= 0) return

        var aspect = width / height
        spanLon = clamp(spanLon, 8, 360)
        var spanLat = spanLon / aspect

        if (spanLat > 180) {
            spanLat = 180
            spanLon = Math.min(360, spanLat * aspect)
        }

        centerLon = clamp(centerLon, -180 + spanLon / 2, 180 - spanLon / 2)
        centerLat = clamp(centerLat, -90 + spanLat / 2, 90 - spanLat / 2)

        vLonMin = centerLon - spanLon / 2
        vLonMax = centerLon + spanLon / 2
        vLatMin = centerLat - spanLat / 2
        vLatMax = centerLat + spanLat / 2

        zoomLevel = clamp(Math.round(Math.log(720 / spanLon) / Math.log(2)), 0, 4)
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

            if (lo0 < lo1 || la0 < la1) {
                centerLon = (lo0 + lo1) / 2
                centerLat = (la0 + la1) / 2
                spanLon = Math.max((lo1 - lo0) * 1.8, control.minSpanLon)
            } else if (lo0 < 1e9) {
                centerLon = lo0
                centerLat = la0
                spanLon = control.minSpanLon
            } else {
                centerLon = 0
                centerLat = 0
                spanLon = 360
            }
        } else {
            centerLon = 0
            centerLat = 0
            spanLon = 360
        }

        initialized = true
        applyView()
    }

    function zoomAt(deltaY, mx, my) {
        var lon = vLonMin + mx / width * (vLonMax - vLonMin)
        var lat = vLatMax - my / height * (vLatMax - vLatMin)

        spanLon = clamp(spanLon * (deltaY > 0 ? 0.83 : 1.20), 8, 360)
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
        return (vLatMax - lat) / (vLatMax - vLatMin) * height
    }

    onMarkersChanged: {
        if (!initialized || (lastCount <= 0 && markers.length > 0)) fit()
        lastCount = markers.length
    }
    onWidthChanged: initialized ? applyView() : fit()
    onHeightChanged: initialized ? applyView() : fit()
    Component.onCompleted: fit()

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
            source: control.tilesUrl.length > 0 ? control.tilesUrl + "/world" + modelData.quad + ".png" : ""
            fillMode: Image.Stretch
            smooth: true
            asynchronous: true
            cache: true
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        property real lastX: 0
        property real lastY: 0
        property bool dragging: false
        cursorShape: dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        onPressed: (mouse) => {
            lastX = mouse.x
            lastY = mouse.y
            dragging = true
        }
        onReleased: dragging = false
        onPositionChanged: (mouse) => {
            if (!dragging) return
            control.pan(mouse.x - lastX, mouse.y - lastY)
            lastX = mouse.x
            lastY = mouse.y
        }
        onWheel: (wheel) => control.zoomAt(wheel.angleDelta.y, wheel.x, wheel.y)
        onDoubleClicked: control.fit()
    }

    Repeater {
        model: control.markers

        delegate: Item {
            x: control.xOf(Number(modelData.lon)) - 9
            y: control.yOf(Number(modelData.lat)) - 9
            width: 18
            height: 18
            z: 5

            Rectangle {
                anchors.centerIn: parent
                width: 24
                height: 24
                radius: 12
                color: "transparent"
                border.color: modelData.color
                border.width: 2
                visible: modelData.pulse === true

                SequentialAnimation on scale {
                    running: parent.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 2.4; duration: 1100; easing.type: Easing.OutQuad }
                    NumberAnimation { from: 2.4; to: 1.0; duration: 0 }
                }
                OpacityAnimator on opacity {
                    running: parent.visible
                    loops: Animation.Infinite
                    from: 0.75
                    to: 0.0
                    duration: 1100
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 14
                height: 14
                radius: 7
                color: modelData.color
                border.color: "#0b1325"
                border.width: 2
            }

            Text {
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    top: parent.bottom
                    topMargin: 2
                }
                text: modelData.label !== undefined ? modelData.label : ""
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSmall
                style: Text.Outline
                styleColor: "#0b1325"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: control.markerClicked(index)
            }
        }
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
                width: 30
                height: 30
                radius: 4
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
