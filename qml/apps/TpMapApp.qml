import QtQuick
import QtQuick.Controls
import TerraPulse

TpGuiApplication {
    id: app

    moduleName: "tpmap"
    menus: ["File", "View", "Settings", "Help"]
    connectionText: sessionQueue + "  " + inventory.structureCount + " objects"
    statusLeft: acq.connected ? "Connected to tpmaster" : "No waveform stream"
    statusCenter: map.modeNames[map.mode] + " / OpenStreetMap"
    statusRight: inventory.sensorCount + " sensors / " + inventory.structureCount + " objects"

    toolBarData: [
        ClassicToolButton { text: "Network"; checkable: true; checked: map.mode === 0; onClicked: map.mode = 0 },
        ClassicToolButton { text: "Ground motion"; checkable: true; checked: map.mode === 1; onClicked: map.mode = 1 },
        ClassicToolButton { text: "Quality control"; checkable: true; checked: map.mode === 2; onClicked: map.mode = 2 },
        ClassicToolButton { text: "Events (" + map.events.length + ")"; checkable: true; checked: map.mode === 3; onClicked: map.mode = 3 },
        ClassicToolButton { text: "Legend"; checkable: true; checked: map.legendVisible; onClicked: map.legendVisible = checked },
        ClassicToolButton { text: "Annotations"; checkable: true; checked: map.annotations; onClicked: map.annotations = checked },
        ClassicToolButton { text: "Gray"; checkable: true; checked: map.grayscaleMap; onClicked: map.grayscaleMap = checked },
        ClassicToolButton { text: "Reset"; onClicked: map.mode = 0 }
    ]

    StructureMapWorkbench {
        id: map
        anchors.fill: parent
    }
}
