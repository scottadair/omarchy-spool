import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Two steps: pick a printer (found on the network, or typed in), then name it.
Popup {
    id: dlg
    modal: true
    anchors.centerIn: parent
    width: 580
    padding: 24
    closePolicy: Popup.CloseOnEscape

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
        spacing: 14

        Label {
            text: dlg.stage === 0 ? "Add a printer" : "Name the printer"
            font.pixelSize: 20; font.weight: Font.DemiBold
        }

        // ---- step 1 ----
        ColumnLayout {
            visible: dlg.stage === 0
            spacing: 12
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#f0a030"
                font.pixelSize: 12
                text: "Spool adds driverless (IPP Everywhere) printers: most network printers made since about 2015. Printers that need a vendor driver aren't supported yet."
            }

            RowLayout {
                Label { text: "On your network"; color: "#9a9aa3"; Layout.fillWidth: true }
                BusyIndicator { visible: backend.discovering; running: visible; implicitHeight: 24; implicitWidth: 24 }
                Button { text: "Rescan (F5)"; flat: true; onClicked: backend.discover() }
            }

            ListView {
                id: found
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(Math.max(count, 1), 4) * 52
                model: backend.discovered
                clip: true
                keyNavigationEnabled: true
                spacing: 2
                Keys.onReturnPressed: dlg.chooseFound()
                Keys.onEnterPressed: dlg.chooseFound()
                Keys.onTabPressed: address.forceActiveFocus()
                Label {
                    anchors.centerIn: parent
                    visible: found.count === 0
                    color: "#6d6d75"
                    text: backend.discovering ? "Searching…" : "No printers found. Try Rescan, or enter an address below."
                }
                delegate: ItemDelegate {
                    width: ListView.view.width
                    height: 50
                    highlighted: ListView.isCurrentItem && found.activeFocus
                    onClicked: { found.currentIndex = index; dlg.chooseFound() }
                    contentItem: RowLayout {
                        Label { text: model.name; Layout.fillWidth: true; elide: Text.ElideRight }
                        Chip { visible: model.added; text: "Already added"; tone: "muted" }
                    }
                    background: Rectangle {
                        radius: 8
                        color: highlighted ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.16) : "#17171a"
                        border.color: highlighted ? Material.accent : "transparent"
                    }
                }
            }

            Label { text: "Or by address"; color: "#9a9aa3" }
            TextField {
                id: address
                Layout.fillWidth: true
                placeholderText: "192.168.1.20, printer.local, ipp://…, ipps://…, socket://…"
                onTextEdited: dlg.addressError = ""
                onAccepted: dlg.chooseAddress()
                Keys.onTabPressed: found.forceActiveFocus()
            }
            Label {
                visible: dlg.addressError !== ""
                text: dlg.addressError
                color: "#ff6b6b"; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "Cancel"; flat: true; onClicked: dlg.close() }
                Button { text: "Next (Enter)"; highlighted: true; onClicked: address.activeFocus || address.text ? dlg.chooseAddress() : dlg.chooseFound() }
            }
        }

        // ---- step 2 ----
        ColumnLayout {
            visible: dlg.stage === 1
            spacing: 12
            Layout.fillWidth: true

            Label { text: dlg.display; font.pixelSize: 15 }
            Label { text: dlg.uri; color: "#6d6d75"; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.fillWidth: true }

            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "Queue name"
                onTextEdited: dlg.nameError = backend.checkName(text)
                onAccepted: dlg.confirmAdd()
            }
            Label {
                visible: dlg.nameError !== ""
                text: dlg.nameError
                color: "#ff6b6b"; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            CheckBox { id: makeDefault; text: "Make this the default printer"; checked: backend.printers.count === 0 }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "Back (Esc)"; flat: true; onClicked: { dlg.stage = 0; found.forceActiveFocus() } }
                Button { text: "Add printer (Enter)"; highlighted: true; enabled: dlg.nameError === ""; onClicked: dlg.confirmAdd() }
            }
        }
    }

    Shortcut { sequence: "F5"; enabled: dlg.opened && dlg.stage === 0; onActivated: backend.discover() }
}
