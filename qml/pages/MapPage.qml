import QtQuick
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    property int selected: -1

    function statusColor(m) {
        if (!m || !m.hasData) return Theme.colorOffline
        return m.warning >= 2 ? Theme.colorCritical : m.warning >= 1 ? Theme.colorWarning : Theme.colorNormal
    }

    function statusText(m) {
        if (!m || !m.hasData) return "NO DATA"
        return m.warning >= 2 ? "CRITICAL" : m.warning >= 1 ? "WARNING" : "NORMAL"
    }

    property var markerList: {
        var out = []
        var s = inventory.structures
        for (var i = 0; i < s.length; i++) {
            out.push({
                lon: s[i].lon,
                lat: s[i].lat,
                color: statusColor(s[i]),
                label: (s[i].name !== undefined ? s[i].name : ("Object " + s[i].objectId)),
                pulse: s[i].hasData && s[i].warning >= 1
            })
        }
        return out
    }

    ColumnLayout {
        anchors {
            fill: parent
            margins: 20
        }
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: "tpmap"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }

                Text {
                    text: "Structures and sensors"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            Repeater {
                model: [
                    { c: Theme.colorNormal, t: "Normal" },
                    { c: Theme.colorWarning, t: "Warning" },
                    { c: Theme.colorCritical, t: "Critical" },
                    { c: Theme.colorOffline, t: "No data" }
                ]

                Row {
                    spacing: 5
                    rightPadding: 10
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: modelData.c
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData.t
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "Map canvas"
                subtitle: mapsUrl !== "" ? "Offline SeisComp-style tile set" : "No tile folder configured"

                Item {
                    anchors {
                        fill: parent
                        margins: 10
                        topMargin: 44
                    }
                    clip: true

                    MapView {
                        id: map
                        anchors.fill: parent
                        provider: "osm"
                        markers: root.markerList
                        tilesUrl: (typeof mapsUrl !== "undefined" && mapsUrl !== "") ? mapsUrl + "/osm" : ""
                        onMarkerClicked: function(index) { root.selected = index }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: inventory.structureCount === 0
                        text: "No objects on the map.\nLoad inventory with tpinv."
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeNormal
                    }
                }
            }

            Panel {
                id: detailPanel
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                title: "Object detail"
                subtitle: root.selected >= 0 ? "Selected marker" : "No selection"

                property var sel: root.selected >= 0 && root.selected < inventory.structures.length
                                  ? inventory.structures[root.selected]
                                  : null

                ColumnLayout {
                    anchors {
                        fill: parent
                        margins: 12
                        topMargin: 48
                    }
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: detailPanel.sel ? (detailPanel.sel.name !== undefined ? detailPanel.sel.name : "Object") : "Select an object"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: detailPanel.sel && detailPanel.sel.description !== undefined
                              ? detailPanel.sel.description
                              : "Click a marker to inspect structure state."
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }

                    StatusPill {
                        visible: detailPanel.sel !== null
                        text: root.statusText(detailPanel.sel)
                        fill: root.statusColor(detailPanel.sel)
                        textColor: detailPanel.sel && detailPanel.sel.warning === 1 ? "#071018" : "#ffffff"
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 10

                        Repeater {
                            model: detailPanel.sel ? [
                                "Object", detailPanel.sel.objectId,
                                "Sensors", detailPanel.sel.sensors,
                                "Channels", detailPanel.sel.channels,
                                "Health", detailPanel.sel.hasData ? Math.round(detailPanel.sel.health * 100) + "%" : "n/a",
                                "Lon", Number(detailPanel.sel.lon).toFixed(4),
                                "Lat", Number(detailPanel.sel.lat).toFixed(4)
                            ] : []

                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                color: index % 2 === 0 ? Theme.textSecondary : Theme.textPrimary
                                font.pixelSize: Theme.fontSizeSmall
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
