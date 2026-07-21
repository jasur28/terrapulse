import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TerraPulse

Item {
    id: root

    ListModel { id: eventsModel }
    property var evIndex: ({})       // shfId -> row index (for list updates)
    property var reviewById: ({})    // shfId -> review status (survives model churn)
    property int selId: -1
    property var selData: null       // decoupled copy of the selected event

    function reviewText(r) {
        return r === 1 ? "CONFIRMED" : r === 2 ? "REJECTED" : "AUTO"
    }

    function reviewColor(r) {
        return r === 1 ? Theme.colorNormal : r === 2 ? Theme.colorCritical : Theme.textSecondary
    }

    // The detail panel reads this copy, never the churning model — a rapid
    // insert/remove can't invalidate an index mid-binding (that segfaulted get()).
    function selectedEvent() { return root.selData }

    function makeRow(shf) {
        var id = shf.shfId
        return {
            shfId: id,
            evTime: new Date(shf.anomalyStartTime).toLocaleTimeString(Qt.locale(), "hh:mm:ss"),
            objId: shf.objectId,
            senId: shf.sensorId,
            evType: shf.anomalyTypeName,
            severity: shf.severityName,
            sevColor: shf.severityColor,
            evStatus: shf.statusName,
            durText: shf.anomalyDuration > 0 ? shf.anomalyDuration + " s" : "ongoing",
            conf: Math.round((shf.confidenceLevel || 0) * 100) + "%",
            review: root.reviewById.hasOwnProperty(id) ? root.reviewById[id] : 0
        }
    }

    function sendReview(action) {
        if (root.selId < 0) return
        journalController.sendAction(root.selId, action)
    }

    Connections {
        target: appController
        function onShfReceived(shf) {
            var id = shf.shfId
            var row = root.makeRow(shf)

            if (root.evIndex.hasOwnProperty(id) && root.evIndex[id] < eventsModel.count) {
                eventsModel.set(root.evIndex[id], row)
            } else {
                eventsModel.insert(0, row)
                if (eventsModel.count > 500) eventsModel.remove(eventsModel.count - 1)
                root.evIndex = ({})
                for (var k = 0; k < eventsModel.count; ++k)
                    root.evIndex[eventsModel.get(k).shfId] = k
                if (root.selId < 0) root.selId = id
            }

            if (id === root.selId) root.selData = row   // keep the detail copy fresh
        }
    }

    Connections {
        target: journalController
        function onJournalReceived(eventId, action, op) {
            var r = action === "confirm" ? 1 : action === "reject" ? 2 : -1
            if (r < 0) return
            root.reviewById[eventId] = r
            if (root.evIndex.hasOwnProperty(eventId)) {
                var i = root.evIndex[eventId]
                if (i >= 0 && i < eventsModel.count) eventsModel.setProperty(i, "review", r)
            }
            if (eventId === root.selId && root.selData) {
                var d = {}
                for (var key in root.selData) d[key] = root.selData[key]
                d.review = r
                root.selData = d
            }
        }
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
                    text: sessionQueue === "playback" ? "tpolv" : "Events"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }

                Text {
                    text: sessionQueue === "playback"
                          ? "Playback review workspace"
                          : "Anomaly list and operator decisions"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            StatusPill {
                text: sessionQueue === "playback" ? "PLAYBACK" : "PRODUCTION"
                fill: sessionQueue === "playback" ? Theme.colorWarning : Theme.colorNormal
                textColor: "#071018"
            }

            Text {
                text: "Operator: " + journalController.operatorName + " / " + eventsModel.count + " events"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Panel {
                Layout.preferredWidth: 560
                Layout.fillHeight: true
                title: "Event list"
                subtitle: "SHF anomaly stream"

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        topMargin: 46
                        margins: 10
                    }
                    height: 28
                    radius: Theme.radiusSmall
                    color: Theme.navBg

                    Row {
                        anchors {
                            fill: parent
                            leftMargin: 10
                            rightMargin: 10
                        }

                        Repeater {
                            model: ["Time", "Object", "Sensor", "Type", "Severity", "Review"]
                            Text {
                                width: parent.width / 6
                                height: parent.height
                                text: modelData
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                ListView {
                    anchors {
                        fill: parent
                        margins: 10
                        topMargin: 80
                    }
                    model: eventsModel
                    clip: true
                    spacing: 3

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 42
                        radius: Theme.radiusSmall
                        color: root.selId === shfId ? "#183354" : (index % 2 === 0 ? Theme.surfaceAlt : Theme.surface)
                        border.color: root.selId === shfId ? Theme.colorService : Theme.border
                        border.width: root.selId === shfId ? 1 : 0

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.selId = shfId
                                root.selData = {
                                    shfId: shfId, evTime: evTime, objId: objId, senId: senId,
                                    evType: evType, severity: severity, sevColor: sevColor,
                                    evStatus: evStatus, durText: durText, conf: conf, review: review
                                }
                            }
                        }

                        Row {
                            anchors {
                                fill: parent
                                leftMargin: 10
                                rightMargin: 10
                            }
                            property real colW: width / 6

                            Repeater {
                                model: [evTime, objId, senId, evType]
                                Text {
                                    width: parent.colW
                                    height: parent.height
                                    text: modelData
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeSmall
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }

                            Item {
                                width: parent.colW
                                height: parent.height
                                StatusPill {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: severity
                                    fill: sevColor
                                    textColor: severity === "LOW" || severity === "MEDIUM" ? "#081018" : "#ffffff"
                                }
                            }

                            Text {
                                width: parent.colW
                                height: parent.height
                                text: root.reviewText(review)
                                color: root.reviewColor(review)
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                EventReviewPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    eventData: root.selectedEvent()
                    reviewText: eventData ? root.reviewText(eventData.review) : "AUTO"
                    reviewColor: eventData ? root.reviewColor(eventData.review) : Theme.textSecondary
                    onConfirmRequested: root.sendReview("confirm")
                    onRejectRequested: root.sendReview("reject")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    Panel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "Waveform window"
                        subtitle: "Raw archive hook for selected anomaly"

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 40
                            text: root.selectedEvent()
                                  ? "Selected anomaly #" + root.selectedEvent().shfId + ". TDS waveform replay will plug in here."
                                  : "Select an event to inspect waveform context."
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeNormal
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }
                    }

                    Panel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "Review notes"
                        subtitle: "Audit trail"

                        TextArea {
                            anchors {
                                fill: parent
                                margins: 12
                                topMargin: 48
                            }
                            placeholderText: "Operator notes"
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            wrapMode: TextArea.Wrap
                            background: Rectangle {
                                color: Theme.navBg
                                radius: Theme.radiusSmall
                                border.color: Theme.border
                            }
                        }
                    }
                }
            }
        }
    }
}
