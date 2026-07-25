import QtQuick
import QtQuick.Controls
import TerraPulse

TpGuiApplication {
    id: app

    moduleName: "tprttv"
    menus: ["File", "Interaction", "Help"]
    connectionText: acq.endpoint
    statusLeft: trace.filterEnabled ? "Filter ON : BW(0.5, 8.0)" : "Filter OFF"
    statusCenter: acq.connected ? "Receiving records" : "Loading records"
    statusRight: acq.packetCount + " samples  " + acq.sampleRate.toFixed(0) + " Hz"

    toolBarData: [
        Button { text: "\u2714  Enabled"; checkable: true; checked: trace.activeTab === 0; onClicked: trace.activeTab = 0 },
        Button { text: "\u2716  Disabled"; checkable: true; checked: trace.activeTab === 1; onClicked: trace.activeTab = 1 },
        Button { text: "Filter"; checkable: true; checked: trace.filterEnabled; onClicked: trace.filterEnabled = checked },
        Button { text: "Associate picks" },
        Button { text: "Origin" },
        Button { text: "Auto scale"; checkable: true; checked: true }
    ]

    RealTimeTraceWorkbench {
        id: trace
        anchors.fill: parent
    }
}
