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
            tStart: shf.anomalyStartTime,
            tEnd: shf.anomalyEndTime,
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

    // Structure-level events from tpevent: many detections grouped into one
    // incident, which is what an operator actually reasons about.
    ListModel { id: groupedModel }
    property var evtIndex: ({})        // eventId -> row index

    Connections {
        target: appController
        function onEventReceived(e) {
            var row = {
                eventId: String(e.eventId),
                objId: e.objectId,
                started: new Date(e.tStart).toLocaleTimeString(Qt.locale(), "hh:mm:ss"),
                types: e.anomalyTypes && e.anomalyTypes.length > 0 ? e.anomalyTypes : "—",
                severity: e.severityName,
                sevColor: e.severity >= 3 ? Theme.colorCritical
                        : e.severity >= 2 ? "#FF6D00"
                        : e.severity >= 1 ? Theme.colorWarning : Theme.colorNormal,
                evStatus: e.statusName,
                sensors: e.sensorCount,
                detections: e.shfCount
            }
            var id = row.eventId
            if (root.evtIndex.hasOwnProperty(id) && root.evtIndex[id] < groupedModel.count) {
                groupedModel.set(root.evtIndex[id], row)
            } else {
                groupedModel.insert(0, row)
                if (groupedModel.count > 200) groupedModel.remove(groupedModel.count - 1)
                root.evtIndex = ({})
                for (var k = 0; k < groupedModel.count; ++k)
                    root.evtIndex[groupedModel.get(k).eventId] = k
            }
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

            ColumnLayout {
                Layout.preferredWidth: 560
                Layout.fillHeight: true
                spacing: 12

            // Grouped incidents: what tpevent made of the raw detections below.
            Panel {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                title: "Structure events"
                subtitle: "grouped incidents (tpevent)"

                Text {
                    anchors.centerIn: parent
                    visible: groupedModel.count === 0
                    text: "No grouped events yet."
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                }

                ListView {
                    anchors { fill: parent; margins: 10; topMargin: 46 }
                    model: groupedModel
                    clip: true
                    spacing: 3
                    visible: groupedModel.count > 0

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 30
                        radius: Theme.radiusSmall
                        color: index % 2 === 0 ? Theme.surfaceAlt : Theme.surface

                        Row {
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            spacing: 0
                            property real colW: width / 6

                            Text {
                                width: parent.colW; height: parent.height
                                text: started; color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeSmall
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                width: parent.colW; height: parent.height
                                text: "obj " + objId; color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeSmall
                                verticalAlignment: Text.AlignVCenter
                            }
                            Item {
                                width: parent.colW; height: parent.height
                                StatusPill {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: severity; fill: sevColor
                                    textColor: severity === "LOW" || severity === "MEDIUM" ? "#081018" : "#ffffff"
                                }
                            }
                            Text {
                                width: parent.colW; height: parent.height
                                text: types; color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeSmall
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.colW; height: parent.height
                                text: sensors + " sen / " + detections + " det"
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeSmall
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                width: parent.colW; height: parent.height
                                text: evStatus
                                color: evStatus === "ACTIVE" ? Theme.colorWarning : Theme.colorNormal
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    ScrollBar.vertical: ScrollBar {}
                }
            }

            Panel {
                Layout.fillWidth: true
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
                                    shfId: shfId, evTime: evTime, tStart: tStart, tEnd: tEnd,
                                    objId: objId, senId: senId,
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
            }   // end left column (structure events + anomaly list)

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
                        subtitle: "Recorded X/Y/Z around the selected anomaly (TDS archive)"

                        WaveformReview {
                            anchors {
                                fill: parent
                                margins: 10
                                topMargin: 46
                            }
                            object: root.selectedEvent() ? root.selectedEvent().objId : 0
                            sensor: root.selectedEvent() ? root.selectedEvent().senId : 0
                            tStart: root.selectedEvent() ? root.selectedEvent().tStart : 0
                            tEnd:   root.selectedEvent() ? root.selectedEvent().tEnd : 0
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
