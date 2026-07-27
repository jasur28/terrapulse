import QtQuick
import QtQuick.Controls
import TerraPulse

TpGuiApplication {
    id: app

    moduleName: "tpolv"
    menus: ["File", "Edit", "View", "Settings", "Help"]
    connectionText: sessionQueue
    statusLeft: sessionQueue === "playback" ? "Playback queue" : "Production queue"
    statusCenter: "origin/location review"
    statusRight: "relocate / picker / magnitudes / confirm"

    toolBarData: [
        ClassicToolButton { text: "\u21b6" },
        ClassicToolButton { text: "\u21b7" },
        ClassicToolButton { text: "Previous event" },
        ClassicToolButton { text: "Next event" },
        ClassicToolButton { text: "Location"; checkable: true; checked: review.currentTab === 0; onClicked: review.currentTab = 0 },
        ClassicToolButton { text: "Magnitudes"; checkable: true; checked: review.currentTab === 1; onClicked: review.currentTab = 1 },
        ClassicToolButton { text: "Event"; checkable: true; checked: review.currentTab === 2; onClicked: review.currentTab = 2 },
        ClassicToolButton { text: "Events"; checkable: true; checked: review.currentTab === 3; onClicked: review.currentTab = 3 },
        ClassicToolButton { text: "Picker" },
        ClassicToolButton { text: "Compute magnitudes" },
        ClassicToolButton { text: "\u2611 Confirm"; success: true }
    ]

    OperatorReviewWorkbench {
        id: review
        anchors.fill: parent
    }
}
