import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import TerraPulse

Item {
    id: root

    function statusText(w)  { return w >= 2 ? "CRITICAL" : w >= 1 ? "WARNING" : "NORMAL" }
    function statusColor(w) { return w >= 2 ? "#FF1744"  : w >= 1 ? "#FFD600"  : "#00C853" }

    // Latest data-quality report per sensor, keyed "<object>/<sensor>" (tpqc).
    property var qcByKey: ({})
    property int qcRevision: 0          // bump to re-evaluate the bindings below

    function qcFor(obj, sen) {
        void root.qcRevision                       // depend on the revision counter
        var k = String(obj) + "/" + String(sen)
        return root.qcByKey.hasOwnProperty(k) ? root.qcByKey[k] : null
    }

    Connections {
        target: appController
        function onQcReceived(qc) {
            var k = String(qc.objectId) + "/" + String(qc.sensorId)
            root.qcByKey[k] = qc
            root.qcRevision++
        }
    }

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Sensors"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeTitle; font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: inventory.sensorCount + " sensors"
                color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
            }
        }

        Rectangle {
            Layout.fillWidth: true; height: 32
            color: Theme.navBg; radius: Theme.radiusSmall
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Repeater {
                    model: ["Object", "Sensor", "Model", "Location", "Health", "Status", "RMS", "Freq Hz", "Last Seen", "Data", "Avail"]
                    Text {
                        Layout.fillWidth: true
                        text: modelData; color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                }
            }
        }

        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true
            model: inventory.sensors; clip: true; spacing: 2

            delegate: Rectangle {
                width: ListView.view.width; height: 36
                color: index % 2 === 0 ? Theme.surface : Theme.surfaceAlt
                radius: Theme.radiusSmall
                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    Repeater {
                        model: [
                            String(modelData.objectId),
                            String(modelData.sensorId),
                            modelData.model !== undefined ? modelData.model : "—",
                            modelData.location !== undefined ? modelData.location : "—",
                            modelData.hasData ? Math.round(modelData.health * 100) + "%" : "—"
                        ]
                        Text {
                            Layout.fillWidth: true
                            text: modelData; color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSmall; elide: Text.ElideRight
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.hasData ? statusText(modelData.warning) : "—"
                        color: modelData.hasData ? statusColor(modelData.warning) : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                    Repeater {
                        model: [
                            modelData.hasData ? Number(modelData.rms).toFixed(2) : "—",
                            modelData.hasData ? Number(modelData.dominantFrequency).toFixed(1) : "—",
                            modelData.lastSeen !== undefined ? modelData.lastSeen : "—"
                        ]
                        Text {
                            Layout.fillWidth: true
                            text: modelData; color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    // Data quality from tpqc — an analysis is only as good as
                    // the data behind it, so the verdict sits next to the health.
                    Text {
                        Layout.fillWidth: true
                        property var qc: root.qcFor(modelData.objectId, modelData.sensorId)
                        text: qc ? qc.verdict : "—"
                        color: !qc ? Theme.textSecondary
                             : qc.verdict === "GOOD" ? Theme.colorNormal
                             : qc.verdict === "DEGRADED" ? Theme.colorWarning : Theme.colorCritical
                        font.pixelSize: Theme.fontSizeSmall; font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        property var qc: root.qcFor(modelData.objectId, modelData.sensorId)
                        text: qc ? Number(qc.availability).toFixed(0) + "%"
                                   + (qc.gaps > 0 ? "  " + qc.gaps + "g" : "") : "—"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }

        Text {
            visible: inventory.sensorCount === 0
            text: "No sensors in inventory. Load it with:  tpinv --file config/inventory.example.json"
            color: Theme.textSecondary; font.pixelSize: Theme.fontSizeSmall
        }
    }
}
