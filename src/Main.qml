import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1040
    height: 700
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: "Printer Settings"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: "#0e0e10"

    property string view: "printers"
    readonly property bool modalOpen: addDialog.opened || renameDialog.opened || removeDialog.opened || help.opened
    readonly property bool inPrinters: view === "printers" && !modalOpen
    readonly property bool inJobs: view === "jobs" && !modalOpen

    function focusList() { (view === "jobs" ? jobsView.list : list).forceActiveFocus() }
    function showJobs() { if (backend.selectedPrinter) { view = "jobs"; jobsView.list.forceActiveFocus() } }
    function showPrinters() { view = "printers"; list.forceActiveFocus() }

    Connections {
        target: backend.printers
        function onModelReset() { win.syncSelection() }
    }
    Connections {
        target: backend
        function onSelectionChanged() { win.syncSelection() }
        function onPrinterAdded() { win.syncSelection() }
    }
    // The model is the source of truth; keep the list cursor on the selected printer.
    function syncSelection() {
        const i = backend.printers.indexOf(backend.selectedPrinter)
        if (i >= 0 && list.currentIndex !== i) list.currentIndex = i
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !addDialog.opened && !renameDialog.opened && !removeDialog.opened; onActivated: help.opened ? help.close() : help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.modalOpen; onActivated: Qt.quit() }
    Shortcut { sequence: "F5"; context: Qt.ApplicationShortcut; enabled: !win.modalOpen; onActivated: backend.refresh() }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: !win.modalOpen; onActivated: addDialog.openDialog() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: backend.printTestPage() }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: backend.setDefault() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: backend.togglePaused() }
    Shortcut { sequence: "G"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: backend.toggleAccepting() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: win.askRename() }
    Shortcut { sequence: "Delete"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: win.askRemove() }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: detail.focusOptions() }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; enabled: win.inPrinters; onActivated: win.showJobs() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: win.inJobs; onActivated: win.showPrinters() }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; enabled: win.inJobs; onActivated: win.showPrinters() }
    Shortcut { sequence: "X"; context: Qt.ApplicationShortcut; enabled: win.inJobs && jobsView.list.currentItem && jobsView.list.currentItem.active; onActivated: backend.cancelJob(jobsView.list.currentItem.jobId) }
    Shortcut { sequence: "Delete"; context: Qt.ApplicationShortcut; enabled: win.inJobs && jobsView.list.currentItem && jobsView.list.currentItem.active; onActivated: backend.cancelJob(jobsView.list.currentItem.jobId) }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.inJobs && jobsView.list.currentItem && !jobsView.list.currentItem.active; onActivated: backend.restartJob(jobsView.list.currentItem.jobId) }

    function askRename() {
        if (!backend.selectedPrinter) return
        renameField.text = backend.selectedPrinter
        renameDialog.open()
        renameField.forceActiveFocus()
        renameField.selectAll()
    }
    function askRemove() {
        if (!backend.selectedPrinter) return
        removeDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Problems with cupsd itself, shown above everything else.
        Rectangle {
            visible: backend.connectionError !== ""
            Layout.fillWidth: true
            implicitHeight: connLabel.implicitHeight + 20
            radius: 8
            color: "#3a1a1a"; border.color: "#ff6b6b"
            Label { id: connLabel; anchors.fill: parent; anchors.margins: 10; text: backend.connectionError; color: "#ffb4b4"; wrapMode: Text.Wrap }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.view === "jobs" ? 1 : 0

            RowLayout {
                spacing: 16

                ColumnLayout {
                    Layout.preferredWidth: 320
                    Layout.maximumWidth: 320
                    Layout.fillHeight: true
                    spacing: 10
                    RowLayout {
                        Label { text: "Printers"; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.fillWidth: true }
                        Button { text: "Add (A)"; highlighted: true; onClicked: addDialog.openDialog() }
                    }
                    ListView {
                        id: list
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: backend.printers
                        clip: true
                        focus: true
                        spacing: 4
                        keyNavigationEnabled: true
                        boundsBehavior: Flickable.StopAtBounds
                        onCurrentIndexChanged: {
                            const name = backend.printers.get(currentIndex).name
                            if (name) backend.select(name)
                        }
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_K && !(event.modifiers & Qt.ControlModifier)) { decrementCurrentIndex(); event.accepted = true }
                        }
                        delegate: PrinterDelegate {}
                    }
                }

                Rectangle { Layout.fillHeight: true; width: 1; color: "#26262b" }

                DetailPane {
                    id: detail
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onRename: win.askRename()
                    onRemove: win.askRemove()
                    onShowJobs: win.showJobs()
                    onLeaveOptions: list.forceActiveFocus()
                }
            }

            JobsView {
                id: jobsView
                onClose: win.showPrinters()
            }
        }

        // Status line: what just happened, or what we're waiting for.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 38
            radius: 8
            color: backend.messageIsError ? "#3a1a1a" : "#17171a"
            border.color: backend.messageIsError ? "#ff6b6b" : "#26262b"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                BusyIndicator { visible: backend.busy; running: visible; implicitHeight: 24; implicitWidth: 24 }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: backend.message !== "" ? backend.message : "Press ? for keyboard shortcuts"
                    color: backend.messageIsError ? "#ffb4b4" : backend.message !== "" ? "#e8e8ea" : "#6d6d75"
                }
                ToolButton { visible: backend.message !== ""; text: "✕"; onClicked: backend.dismissMessage() }
            }
        }
    }

    AddDialog { id: addDialog; onClosed: win.focusList() }
    HelpOverlay { id: help; onClosed: win.focusList() }

    Popup {
        id: renameDialog
        modal: true
        anchors.centerIn: parent
        width: 420
        padding: 24
        onClosed: win.focusList()
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Rename " + backend.selectedPrinter; font.pixelSize: 18; font.weight: Font.DemiBold }
            TextField {
                id: renameField
                Layout.fillWidth: true
                onAccepted: { if (backend.checkName(text) === "" || text === backend.selectedPrinter) { backend.renamePrinter(text); renameDialog.close() } }
            }
            Label {
                readonly property string problem: renameField.text === backend.selectedPrinter ? "" : backend.checkName(renameField.text)
                visible: problem !== ""; text: problem; color: "#ff6b6b"; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label { text: "Queue names can't contain spaces, / or #."; color: "#6d6d75"; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "Cancel"; flat: true; onClicked: renameDialog.close() }
                Button { text: "Rename (Enter)"; highlighted: true; onClicked: renameField.accepted() }
            }
        }
    }

    Popup {
        id: removeDialog
        modal: true
        anchors.centerIn: parent
        width: 440
        padding: 24
        onOpened: confirmRemove.forceActiveFocus()
        onClosed: win.focusList()
        contentItem: ColumnLayout {
            spacing: 14
            Label { text: "Remove " + backend.selectedPrinter + "?"; font.pixelSize: 18; font.weight: Font.DemiBold }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#9a9aa3"
                text: "Its queue and any waiting jobs are deleted. The printer itself is untouched, and you can add it again later."
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "Keep it (Esc)"; flat: true; onClicked: removeDialog.close() }
                Button {
                    id: confirmRemove
                    text: "Remove (Enter)"
                    highlighted: true
                    Material.accent: "#ff6b6b"
                    Keys.onReturnPressed: clicked()
                    onClicked: { backend.removePrinter(); removeDialog.close() }
                }
            }
        }
    }
}
