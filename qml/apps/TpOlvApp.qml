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
        Button { text: "\u21b6" },
        Button { text: "\u21b7" },
        Button { text: "Previous event" },
        Button { text: "Next event" },
        Button { text: "Location"; checkable: true; checked: review.currentTab === 0; onClicked: review.currentTab = 0 },
        Button { text: "Magnitudes"; checkable: true; checked: review.currentTab === 1; onClicked: review.currentTab = 1 },
        Button { text: "Event"; checkable: true; checked: review.currentTab === 2; onClicked: review.currentTab = 2 },
        Button { text: "Events"; checkable: true; checked: review.currentTab === 3; onClicked: review.currentTab = 3 },
        Button { text: "Picker" },
        Button { text: "Compute magnitudes" },
        Button { text: "\u2611 Confirm" }
    ]

    OperatorReviewWorkbench {
        id: review
        anchors.fill: parent
    }
}
