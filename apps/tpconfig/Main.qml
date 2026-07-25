import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1240
    height: 820
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: "tpconfig - TerraPulse configuration"
    color: "#efefef"

    readonly property color bg: "#efefef"
    readonly property color panel: "#f8f8f8"
    readonly property color topbar: "#e3e6e8"
    readonly property color line: "#bfc4c9"
    readonly property color text: "#202428"
    readonly property color muted: "#5e666e"
    readonly property color select: "#cfe8ff"
    readonly property color accent: "#1e9bd7"
    readonly property color ok: "#14923f"
    readonly property color warn: "#b77900"
    readonly property color bad: "#b51d1a"

    property int panelIndex: 0
    property string currentModule: "global"
    property var moduleRows: []
    property var systemRows: []
    property var settingsRows: []
    property var bindingRows: []
    property var inventory: ({})

    function refreshAll() {
        moduleRows = editor.moduleRows()
        systemRows = editor.systemModules()
        if (currentModule === "" && moduleRows.length > 0)
            currentModule = moduleRows[0].name
        settingsRows = currentModule === "" ? [] : editor.settings(currentModule)
        bindingRows = editor.bindings()
        inventory = editor.loadInventory()
    }

    function panelTitle() {
        return ["System", "Modules", "Bindings", "Inventory"][panelIndex]
    }

    function panelInfo() {
        if (panelIndex === 0) return "Control TerraPulse modules, inspect status and prepare configuration updates."
        if (panelIndex === 1) return "Edit module configuration files stored below etc/ and compare values with defaults."
        if (panelIndex === 2) return "Assign module profiles to structures and sensors, like station bindings in SeisComp."
        return "Inspect, reload and publish accelerometer inventory for tpmaster."
    }

    Component.onCompleted: refreshAll()

    Connections {
        target: editor
        function onSettingsChanged(module) {
            if (module === win.currentModule) win.settingsRows = editor.settings(module)
            win.moduleRows = editor.moduleRows()
        }
        function onBindingsChanged() { win.bindingRows = editor.bindings() }
        function onStatusChanged() { logModel.insert(0, { text: editor.status }) }
        function onConfigModeChanged() { logModel.insert(0, { text: "Mode switched to " + editor.configMode }) }
    }

    ListModel { id: logModel }

    component ToolButtonLite: Button {
        id: b
        implicitHeight: 30
        padding: 8
        background: Rectangle {
            color: b.down ? "#d2d7dc" : b.hovered ? "#edf4fa" : "#f7f7f7"
            border.color: win.line
            radius: 2
        }
        contentItem: Text {
            text: b.text
            color: b.enabled ? win.text : "#90979e"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component PanelButton: Rectangle {
        id: item
        required property string label
        required property int idx
        width: ListView.view.width
        height: 34
        color: win.panelIndex === idx ? win.select : "transparent"
        border.color: win.panelIndex === idx ? "#8dbddd" : "transparent"
        Text {
            anchors.fill: parent
            anchors.leftMargin: 12
            text: item.label
            color: win.panelIndex === idx ? "#0f4c75" : win.text
            font.pixelSize: 13
            font.bold: win.panelIndex === idx
            verticalAlignment: Text.AlignVCenter
        }
        MouseArea { anchors.fill: parent; onClicked: win.panelIndex = item.idx }
    }

    header: ColumnLayout {
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            height: 34
            color: win.topbar
            border.color: win.line
            RowLayout {
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                spacing: 8
                Text {
                    text: "TerraPulse 0.1 - tpconfig"
                    color: win.text
                    font.pixelSize: 13
                    font.bold: true
                }
                Text {
                    text: editor.configMode + " mode"
                    color: editor.configMode === "System" ? win.bad : win.accent
                    font.pixelSize: 12
                    font.bold: true
                    Layout.leftMargin: 10
                }
                Text {
                    text: editor.root
                    color: win.muted
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 42
            color: "#f5f5f5"
            border.color: win.line
            RowLayout {
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                spacing: 6
                ToolButtonLite {
                    text: "Save"
                    enabled: win.currentModule !== "" && editor.hasChanges(win.currentModule)
                    onClicked: {
                        editor.saveModule(win.currentModule)
                        win.refreshAll()
                    }
                }
                ToolButtonLite {
                    text: "Reset all"
                    onClicked: {
                        editor.resetModule(win.currentModule)
                        win.refreshAll()
                    }
                }
                ToolButtonLite {
                    text: "Wizard"
                    onClicked: logModel.insert(0, { text: "Initial setup wizard will configure agency, tpmaster and database in a later step." })
                }
                ToolButtonLite {
                    text: "Switch mode"
                    onClicked: editor.switchMode()
                }
                ToolButtonLite {
                    text: "Help"
                    onClicked: logModel.insert(0, { text: "scconfig reference: System, Modules, Bindings, Inventory." })
                }
                Item { Layout.fillWidth: true }
                ToolButtonLite { text: "Quit"; onClicked: Qt.quit() }
            }
        }
    }

    footer: Rectangle {
        height: 26
        color: "#f5f5f5"
        border.color: win.line
        Text {
            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
            text: editor.status
            color: win.muted
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Rectangle {
            Layout.preferredWidth: 150
            Layout.fillHeight: true
            color: "#f7f7f7"
            border.color: win.line
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                PanelButton { label: "System"; idx: 0 }
                PanelButton { label: "Modules"; idx: 1 }
                PanelButton { label: "Bindings"; idx: 2 }
                PanelButton { label: "Inventory"; idx: 3 }
                Item { Layout.fillHeight: true }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                height: 56
                color: win.panel
                border.color: win.line
                Column {
                    anchors { fill: parent; margins: 10 }
                    spacing: 3
                    Text { text: win.panelTitle(); color: win.text; font.pixelSize: 18; font.bold: true }
                    Text { text: win.panelInfo(); color: win.muted; font.pixelSize: 12; elide: Text.ElideRight; width: parent.width }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: win.panelIndex

                RowLayout {
                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6
                        RowLayout {
                            ToolButtonLite { text: "Reload"; onClicked: { editor.systemAction("reload"); win.systemRows = editor.systemModules() } }
                            ToolButtonLite { text: "Start"; onClicked: editor.systemAction("start") }
                            ToolButtonLite { text: "Stop"; onClicked: editor.systemAction("stop") }
                            ToolButtonLite { text: "Restart"; onClicked: editor.systemAction("restart") }
                            ToolButtonLite { text: "Check"; onClicked: editor.systemAction("check") }
                            ToolButtonLite { text: "Update config"; onClicked: editor.systemAction("update-config") }
                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: win.topbar
                            border.color: win.line
                            Row {
                                anchors.fill: parent
                                Text { width: parent.width * 0.28; text: "Module"; anchors.verticalCenter: parent.verticalCenter; leftPadding: 8; font.bold: true; font.pixelSize: 12 }
                                Text { width: parent.width * 0.18; text: "Group"; anchors.verticalCenter: parent.verticalCenter; font.bold: true; font.pixelSize: 12 }
                                Text { width: parent.width * 0.18; text: "State"; anchors.verticalCenter: parent.verticalCenter; font.bold: true; font.pixelSize: 12 }
                                Text { width: parent.width * 0.18; text: "Enabled"; anchors.verticalCenter: parent.verticalCenter; font.bold: true; font.pixelSize: 12 }
                                Text { width: parent.width * 0.18; text: "PID"; anchors.verticalCenter: parent.verticalCenter; font.bold: true; font.pixelSize: 12 }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: win.systemRows
                            clip: true
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 28
                                color: index % 2 ? "#f7f7f7" : "#ffffff"
                                Row {
                                    anchors.fill: parent
                                    Text { width: parent.width * 0.28; text: modelData.name; leftPadding: 8; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12; color: win.text }
                                    Text { width: parent.width * 0.18; text: modelData.group; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12; color: win.muted }
                                    Text { width: parent.width * 0.18; text: modelData.state; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12; color: modelData.state === "stopped" ? win.muted : win.ok }
                                    Text { width: parent.width * 0.18; text: modelData.enabled ? "yes" : "no"; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12; color: modelData.enabled ? win.ok : win.muted }
                                    Text { width: parent.width * 0.18; text: modelData.pid; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12; color: win.muted }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 330
                        Layout.fillHeight: true
                        color: "#ffffff"
                        border.color: win.line
                        ColumnLayout {
                            anchors { fill: parent; margins: 8 }
                            Text { text: "Log"; font.pixelSize: 13; font.bold: true; color: win.text }
                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: logModel
                                clip: true
                                delegate: Text {
                                    width: ListView.view.width
                                    text: model.text
                                    wrapMode: Text.WordWrap
                                    color: win.muted
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 240
                        Layout.fillHeight: true
                        color: "#ffffff"
                        border.color: win.line
                        ListView {
                            anchors { fill: parent; margins: 6 }
                            model: win.moduleRows
                            clip: true
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 32
                                color: modelData.name === win.currentModule ? win.select : "transparent"
                                Text {
                                    anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                                    text: modelData.name + "  [" + modelData.category + "]"
                                    color: modelData.changed ? win.accent : win.text
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        win.currentModule = modelData.name
                                        win.settingsRows = editor.settings(modelData.name)
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6
                        RowLayout {
                            Text { text: win.currentModule; font.pixelSize: 15; font.bold: true; color: win.text }
                            Text { text: "etc/" + win.currentModule + ".cfg"; color: win.muted; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            TextField {
                                id: search
                                Layout.preferredWidth: 240
                                placeholderText: "Search parameter"
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: win.topbar
                            border.color: win.line
                            Row {
                                anchors.fill: parent
                                Text { width: parent.width * 0.40; text: "Parameter"; leftPadding: 8; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 12 }
                                Text { width: parent.width * 0.28; text: "Value"; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 12 }
                                Text { width: parent.width * 0.22; text: "Default"; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 12 }
                                Text { width: parent.width * 0.10; text: "State"; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 12 }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: win.settingsRows
                            clip: true
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: (search.text === "" || modelData.key.toLowerCase().indexOf(search.text.toLowerCase()) >= 0) ? 32 : 0
                                visible: height > 0
                                color: index % 2 ? "#f7f7f7" : "#ffffff"
                                Row {
                                    anchors.fill: parent
                                    Text {
                                        width: parent.width * 0.40
                                        text: modelData.key
                                        leftPadding: 8
                                        verticalAlignment: Text.AlignVCenter
                                        color: modelData.overridden ? win.accent : win.text
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    TextField {
                                        width: parent.width * 0.28
                                        height: 24
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.value
                                        font.pixelSize: 12
                                        onEditingFinished: {
                                            if (text !== modelData.value)
                                                editor.setSetting(win.currentModule, modelData.key, text)
                                        }
                                    }
                                    Text {
                                        width: parent.width * 0.22
                                        text: modelData.def === "" ? "-" : modelData.def
                                        verticalAlignment: Text.AlignVCenter
                                        color: win.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width * 0.10
                                        text: modelData.staged ? "edited" : modelData.overridden ? "set" : ""
                                        verticalAlignment: Text.AlignVCenter
                                        color: modelData.staged ? win.warn : win.muted
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 330
                        Layout.fillHeight: true
                        color: "#ffffff"
                        border.color: win.line
                        ColumnLayout {
                            anchors { fill: parent; margins: 8 }
                            Text { text: "Station tree"; color: win.text; font.bold: true; font.pixelSize: 13 }
                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: win.inventory.structures !== undefined ? win.inventory.structures : []
                                delegate: Text {
                                    width: ListView.view.width
                                    text: "Structure " + modelData.objectId + "  " + modelData.name
                                    color: win.text
                                    font.pixelSize: 12
                                    padding: 6
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        RowLayout {
                            Text { text: "Bindings"; color: win.text; font.pixelSize: 15; font.bold: true }
                            Item { Layout.fillWidth: true }
                            SpinBox { id: bObj; from: 1; to: 9999; value: 1; editable: true }
                            SpinBox { id: bSen; from: 1; to: 9999; value: 1; editable: true }
                            ComboBox { id: bMod; model: editor.modules(); Layout.preferredWidth: 130 }
                            ComboBox {
                                id: bProf
                                model: ["(module config)"].concat(editor.profiles(bMod.currentText))
                                Layout.preferredWidth: 160
                            }
                            ToolButtonLite {
                                text: "Apply"
                                onClicked: editor.setBinding(bObj.value, bSen.value, bMod.currentText, bProf.currentIndex === 0 ? "" : bProf.currentText)
                            }
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: win.bindingRows
                            clip: true
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 34 + modelData.entries.length * 20
                                color: index % 2 ? "#f7f7f7" : "#ffffff"
                                border.color: win.line
                                Column {
                                    anchors { fill: parent; margins: 7 }
                                    Text { text: "object " + modelData.object + " / sensor " + modelData.sensor + "   " + modelData.file; color: win.text; font.bold: true; font.pixelSize: 12 }
                                    Repeater {
                                        model: modelData.entries
                                        Text { text: "  " + modelData.module + (modelData.profile === "" ? "  (module config)" : " -> " + modelData.profile); color: win.muted; font.pixelSize: 12 }
                                    }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    spacing: 6
                    RowLayout {
                        Text { text: editor.inventoryPath; color: win.muted; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        ToolButtonLite { text: "Reload"; onClicked: win.inventory = editor.loadInventory() }
                        ToolButtonLite { text: "Check"; onClicked: logModel.insert(0, { text: "Inventory contains " + (win.inventory.structures ? win.inventory.structures.length : 0) + " structure(s)." }) }
                        ToolButtonLite { text: "Sync keys"; onClicked: editor.systemAction("update-config") }
                        ToolButtonLite { text: "Publish"; onClicked: editor.publishInventory("127.0.0.1") }
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: win.inventory.structures !== undefined ? win.inventory.structures : []
                        clip: true
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 62 + (modelData.sensors ? modelData.sensors.length * 22 : 0)
                            color: index % 2 ? "#f7f7f7" : "#ffffff"
                            border.color: win.line
                            Column {
                                anchors { fill: parent; margins: 8 }
                                Text { text: modelData.name + "  [" + modelData.objectId + "]"; color: win.text; font.bold: true; font.pixelSize: 13 }
                                Text { text: modelData.lat + ", " + modelData.lon + "  " + modelData.description; color: win.muted; font.pixelSize: 11 }
                                Repeater {
                                    model: modelData.sensors !== undefined ? modelData.sensors : []
                                    Text {
                                        text: "  sensor " + modelData.sensorId + "  " + modelData.model + "  " + modelData.location + "  channels: " + (modelData.channels ? modelData.channels.length : 0)
                                        color: win.text
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
