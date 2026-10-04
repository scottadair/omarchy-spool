import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Two steps: pick a printer (found on the network, or typed in), then name it.
TuiPopup {
    id: dlg
    width: 600
    readonly property var c: theme.colors

    property int stage: 0
    property string uri
    property string display
    property string addressError
    property string nameError

    function openDialog() {
        stage = 0; uri = ""; display = ""; addressError = ""; nameError = ""
        address.text = ""
        open()
        backend.discover()
        found.forceActiveFocus()
    }

    function chooseFound() {
        const row = backend.discovered.get(found.currentIndex)
        if (!row.uri) return
        if (row.added) { addressError = row.name + " is already set up."; return }
        pick(row.uri, row.name)
    }

    function chooseAddress() {
        const r = backend.checkAddress(address.text)
        if (!r.ok) { addressError = r.error; return }
        pick(r.uri, r.uri.replace(/^[a-z]+:\/\//, "").split("/")[0])
    }

    function pick(u, label) {
        addressError = ""
        uri = u; display = label
        nameField.text = backend.suggestName(label)
        nameError = backend.checkName(nameField.text)
        stage = 1
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    function confirmAdd() {
        nameError = backend.checkName(nameField.text)
        if (nameError) return
        backend.addPrinter(uri, nameField.text, display, makeDefault.checked)
        close()
    }

    // Esc steps back before it closes.
    onClosed: stage = 0
    Keys.onEscapePressed: event => {
        if (stage === 1) { stage = 0; found.forceActiveFocus() } else close()
        event.accepted = true
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: dlg.stage === 0 ? "Add a printer" : "Name the printer"
            color: dlg.c.accent; font.bold: true; font.pixelSize: 16
        }
        Rectangle { height: 1; color: dlg.c.muted; Layout.fillWidth: true }

        // ---- step 1 ----
        ColumnLayout {
            visible: dlg.stage === 0
            spacing: 10
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: dlg.c.yellow
                font.pixelSize: 12
                text: "! Spool adds driverless (IPP Everywhere) printers: most network printers made since about 2015. Printers that need a vendor driver aren't supported yet."
            }

            RowLayout {
                Label { text: "On your network"; color: dlg.c.dark_foreground; Layout.fillWidth: true }
                Label { visible: backend.discovering; text: "searching…"; color: dlg.c.accent }
                Hint { key: "F5"; text: "rescan"; onActivated: backend.discover() }
            }

            ListView {
                id: found
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(Math.max(count, 1), 4) * 34
                model: backend.discovered
                clip: true
                keyNavigationEnabled: true
                Keys.onReturnPressed: dlg.chooseFound()
                Keys.onEnterPressed: dlg.chooseFound()
                Keys.onTabPressed: address.forceActiveFocus()
                Label {
                    anchors.centerIn: parent
                    visible: found.count === 0
                    color: dlg.c.dark_foreground
                    text: backend.discovering ? "Searching…" : "None found. Rescan, or enter an address below."
                }
                delegate: Item {
                    width: ListView.view.width
                    height: 34
                    readonly property bool current: ListView.isCurrentItem && found.activeFocus
                    Rectangle { anchors.fill: parent; color: parent.current ? dlg.c.selection : "transparent" }
                    MouseArea { anchors.fill: parent; onClicked: { found.currentIndex = index; dlg.chooseFound() } }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8
                        Label { text: parent.parent.current ? "▸" : " "; color: dlg.c.accent; font.bold: true }
                        Label {
                            text: model.name
                            color: model.added ? dlg.c.dark_foreground : dlg.c.foreground
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label { visible: model.added; text: "already added"; color: dlg.c.dark_foreground; font.pixelSize: 12 }
                    }
                }
            }

            Label { text: "Or by address"; color: dlg.c.dark_foreground }
            TuiField {
                id: address
                Layout.fillWidth: true
                placeholderText: "192.168.1.20  printer.local  ipp://…  ipps://…  socket://…"
                onTextEdited: dlg.addressError = ""
                onAccepted: dlg.chooseAddress()
                Keys.onTabPressed: found.forceActiveFocus()
            }
            Label {
                visible: dlg.addressError !== ""
                text: "✗ " + dlg.addressError
                color: dlg.c.red; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            RowLayout {
                spacing: 18
                Hint { key: "enter"; text: "select"; onActivated: address.text ? dlg.chooseAddress() : dlg.chooseFound() }
                Hint { key: "tab"; text: "list / address" }
                Hint { key: "esc"; text: "cancel"; onActivated: dlg.close() }
            }
        }

        // ---- step 2 ----
        ColumnLayout {
            visible: dlg.stage === 1
            spacing: 10
            Layout.fillWidth: true

            Label { text: dlg.display; color: dlg.c.bright_foreground; font.bold: true }
            Label { text: dlg.uri; color: dlg.c.dark_foreground; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.fillWidth: true }

            Label { text: "Queue name"; color: dlg.c.dark_foreground }
            TuiField {
                id: nameField
                Layout.fillWidth: true
                onTextEdited: dlg.nameError = backend.checkName(text)
                onAccepted: dlg.confirmAdd()
                Keys.onTabPressed: makeDefault.forceActiveFocus()
            }
            Label {
                visible: dlg.nameError !== ""
                text: "✗ " + dlg.nameError
                color: dlg.c.red; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            // Space toggles, like a TUI checkbox.
            FocusScope {
                id: makeDefault
                property bool checked: backend.printers.count === 0
                implicitHeight: checkLabel.implicitHeight + 8
                Layout.fillWidth: true
                Keys.onSpacePressed: checked = !checked
                Keys.onTabPressed: nameField.forceActiveFocus()
                Keys.onReturnPressed: dlg.confirmAdd()
                Rectangle { anchors.fill: parent; color: makeDefault.activeFocus ? dlg.c.selection : "transparent" }
                Label {
                    id: checkLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    text: (makeDefault.checked ? "[x] " : "[ ] ") + "Make this the default printer"
                    color: makeDefault.checked ? dlg.c.accent : dlg.c.foreground
                }
                MouseArea { anchors.fill: parent; onClicked: { makeDefault.forceActiveFocus(); makeDefault.checked = !makeDefault.checked } }
            }
            RowLayout {
                spacing: 18
                Hint { key: "enter"; text: "add printer"; onActivated: dlg.confirmAdd() }
                Hint { key: "space"; text: "toggle default" }
                Hint { key: "esc"; text: "back"; onActivated: { dlg.stage = 0; found.forceActiveFocus() } }
            }
        }
    }

    Shortcut { sequence: "F5"; enabled: dlg.opened && dlg.stage === 0; onActivated: backend.discover() }
}
