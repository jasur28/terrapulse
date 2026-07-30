import QtQuick
import QtQuick.Controls
import TerraPulse

TpGuiApplication {
    id: app

    moduleName: "tpolv"
    menus: ["File", "Edit", "View", "Settings", "Help"]
    connectionText: review.selectedEvent ? review.selectedEvent.id + "  " + review.eventTime(review.selectedEvent) : sessionQueue
    statusLeft: sessionQueue === "playback" ? "Playback queue" : "Production queue"
    statusCenter: "origin/location review"
    statusRight: "relocate / picker / magnitudes / confirm"

    toolBarData: [
        ClassicToolButton { text: "\u21b6"; enabled: review.selectedIndex < review.events.length - 1; onClicked: review.selectEvent(review.selectedIndex + 1) },
        ClassicToolButton { text: "\u21b7"; enabled: review.selectedIndex > 0; onClicked: review.selectEvent(review.selectedIndex - 1) },
        ClassicToolButton { text: "Previous event"; enabled: review.selectedIndex < review.events.length - 1; onClicked: review.selectEvent(review.selectedIndex + 1) },
        ClassicToolButton { text: "Next event"; enabled: review.selectedIndex > 0; onClicked: review.selectEvent(review.selectedIndex - 1) },
        ClassicToolButton { text: "Picker"; onClicked: review.currentTab = 0 },
        ClassicToolButton { text: "Compute magnitudes"; enabled: review.selectedEvent !== null },
        ClassicToolButton { text: "\u2611 Confirm"; success: true; enabled: review.selectedEvent !== null }
    ]

    OperatorReviewWorkbench {
        id: review
        anchors.fill: parent
    }
}
