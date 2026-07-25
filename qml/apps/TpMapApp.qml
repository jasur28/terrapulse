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
        Button { text: "Network"; checkable: true; checked: map.mode === 0; onClicked: map.mode = 0 },
        Button { text: "Ground motion"; checkable: true; checked: map.mode === 1; onClicked: map.mode = 1 },
        Button { text: "Quality control"; checkable: true; checked: map.mode === 2; onClicked: map.mode = 2 },
        Button { text: "Events (" + map.events.length + ")"; checkable: true; checked: map.mode === 3; onClicked: map.mode = 3 },
        Button { text: "Legend"; checkable: true; checked: map.legendVisible; onClicked: map.legendVisible = checked },
        Button { text: "Annotations"; checkable: true; checked: map.annotations; onClicked: map.annotations = checked },
        Button { text: "Gray"; checkable: true; checked: map.grayscaleMap; onClicked: map.grayscaleMap = checked },
        Button { text: "Reset"; onClicked: map.mode = 0 }
    ]

    StructureMapWorkbench {
        id: map
        anchors.fill: parent
    }
}
