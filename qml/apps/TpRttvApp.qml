import QtQuick
import QtQuick.Controls
import TerraPulse

TpGuiApplication {
    id: app

    moduleName: "tprttv"
    menus: ["File", "Interaction", "Help"]
    connectionText: liveWaveform.endpoint !== "" ? liveWaveform.endpoint : "no SeedLink backbone"
    statusLeft: trace.filterEnabled ? "Filter ON : hp:0.3, lp:20 (structural)" : "Filter OFF"
    statusCenter: liveWaveform.connected ? "Receiving records" : "Loading records"
    statusRight: liveWaveform.records + " samples  " +
                 (liveWaveform.streams.length > 0 ? liveWaveform.streams[0].rate.toFixed(0) : "0") + " Hz"

    toolBarData: [
        ClassicToolButton { text: "\u2714  Enabled"; success: true; checkable: true; checked: trace.activeTab === 0; onClicked: trace.activeTab = 0 },
        ClassicToolButton { text: "\u2716  Disabled"; danger: true; checkable: true; checked: trace.activeTab === 1; onClicked: trace.activeTab = 1 },
        ClassicToolButton { text: "Filter"; checkable: true; checked: trace.filterEnabled; onClicked: trace.filterEnabled = checked },
        ClassicToolButton { text: "Associate picks"; checkable: true; checked: trace.manualAssociatorVisible; onClicked: trace.manualAssociatorVisible = checked },
        ClassicToolButton { text: "Origin"; onClicked: trace.openArtificialOrigin() },
        ClassicToolButton { text: "Auto scale"; checkable: true; checked: true }
    ]

    RealTimeTraceWorkbench {
        id: trace
        anchors.fill: parent
    }
}
