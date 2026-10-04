import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1100
    height: 720
    minimumWidth: 760
    minimumHeight: 480
    visible: true
    title: "Printer Settings"

    readonly property var c: theme.colors
    color: c.background
    font.family: theme.fontFamily
    font.pixelSize: 14

    // Material is the base style (as in Omarchy's other apps); every color it
    // draws comes from the current Omarchy theme.
    Material.theme: theme.mode === "light" ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.primary: theme.accent
    Material.background: c.background
    Material.foreground: c.foreground

    property string view: "printers"
    readonly property bool modalOpen: addDialog.opened || renameDialog.opened || removeDialog.opened || help.opened
    readonly property bool inPrinters: view === "printers" && !modalOpen
    readonly property bool inJobs: view === "jobs" && !modalOpen
    readonly property bool jobSelected: !!jobsView.list.currentItem

    function focusList() { (view === "jobs" ? jobsView.list : list).forceActiveFocus() }
    function showJobs() { if (backend.selectedPrinter) { view = "jobs"; jobsView.list.forceActiveFocus() } }
    function showPrinters() { view = "printers"; list.forceActiveFocus() }
    function cancelSelectedJob() { if (jobSelected && jobsView.list.currentItem.active) backend.cancelJob(jobsView.list.currentItem.jobId) }
    function restartSelectedJob() { if (jobSelected && !jobsView.list.currentItem.active) backend.restartJob(jobsView.list.currentItem.jobId) }

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
    Shortcut { sequence: "X"; context: Qt.ApplicationShortcut; enabled: win.inJobs; onActivated: win.cancelSelectedJob() }
    Shortcut { sequence: "Delete"; context: Qt.ApplicationShortcut; enabled: win.inJobs; onActivated: win.cancelSelectedJob() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.inJobs; onActivated: win.restartSelectedJob() }

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
        anchors.margins: 14
        spacing: 8

        // Title line, like the header of a TUI.
        RowLayout {
            spacing: 10
            Label { text: "spool"; color: win.c.accent; font.bold: true; font.pixelSize: 16 }
            Label { text: "printer settings"; color: win.c.dark_foreground }
            Item { Layout.fillWidth: true }
            Label {
                visible: backend.connectionError !== ""
                text: "✗ " + backend.connectionError
                color: win.c.red
                elide: Text.ElideRight
                Layout.maximumWidth: win.width * 0.7
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.view === "jobs" ? 1 : 0

            RowLayout {
                spacing: 10

                Panel {
                    title: "Printers"
                    focused: list.activeFocus
                    Layout.preferredWidth: 340
                    Layout.maximumWidth: 340
                    Layout.fillHeight: true
                    ListView {
                        id: list
                        anchors.fill: parent
                        model: backend.printers
                        clip: true
                        focus: true
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

                DetailPane {
                    id: detail
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onLeaveOptions: list.forceActiveFocus()
                }
            }

            JobsView { id: jobsView }
        }

        // Status line: what just happened, or what we're waiting for.
        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Label {
                text: backend.busy ? "…" : backend.messageIsError ? "✗" : backend.message !== "" ? "✓" : " "
                color: backend.messageIsError ? win.c.red : backend.busy ? win.c.accent : win.c.green
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: backend.message
                color: backend.messageIsError ? win.c.red : win.c.foreground
            }
            Hint { visible: backend.message !== ""; key: "×"; text: "dismiss"; onActivated: backend.dismissMessage() }
        }

        // The key bar, clickable for the mouse.
        Flow {
            Layout.fillWidth: true
            spacing: 18
            Hint { visible: win.view === "printers"; key: "a"; text: "add"; onActivated: addDialog.openDialog() }
            Hint { visible: win.view === "printers"; key: "t"; text: "test page"; onActivated: backend.printTestPage() }
            Hint { visible: win.view === "printers"; key: "d"; text: "default"; onActivated: backend.setDefault() }
            Hint { visible: win.view === "printers"; key: "p"; text: "pause/resume"; onActivated: backend.togglePaused() }
            Hint { visible: win.view === "printers"; key: "g"; text: "accept/reject"; onActivated: backend.toggleAccepting() }
            Hint { visible: win.view === "printers"; key: "r"; text: "rename"; onActivated: win.askRename() }
            Hint { visible: win.view === "printers"; key: "del"; text: "remove"; onActivated: win.askRemove() }
            Hint { visible: win.view === "printers"; key: "o"; text: "options"; onActivated: detail.focusOptions() }
            Hint { visible: win.view === "printers"; key: "j"; text: "jobs"; onActivated: win.showJobs() }
            Hint { visible: win.view === "jobs"; key: "x"; text: "cancel job"; onActivated: win.cancelSelectedJob() }
            Hint { visible: win.view === "jobs"; key: "r"; text: "restart job"; onActivated: win.restartSelectedJob() }
            Hint { visible: win.view === "jobs"; key: "esc"; text: "back"; onActivated: win.showPrinters() }
            Hint { key: "?"; text: "help"; onActivated: help.open() }
            Hint { key: "q"; text: "quit"; onActivated: Qt.quit() }
        }
    }

    AddDialog { id: addDialog; onClosed: win.focusList() }
    HelpOverlay { id: help; onClosed: win.focusList() }

    TuiPopup {
        id: renameDialog
        width: 460
        onClosed: win.focusList()
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "Rename " + backend.selectedPrinter; color: win.c.accent; font.bold: true; font.pixelSize: 16 }
            Rectangle { height: 1; color: win.c.muted; Layout.fillWidth: true }
            TuiField {
                id: renameField
                Layout.fillWidth: true
                onAccepted: { if (text === backend.selectedPrinter || backend.checkName(text) === "") { backend.renamePrinter(text); renameDialog.close() } }
            }
            Label {
                readonly property string problem: renameField.text === backend.selectedPrinter ? "" : backend.checkName(renameField.text)
                visible: problem !== ""; text: "✗ " + problem; color: win.c.red; Layout.fillWidth: true; wrapMode: Text.Wrap
            }
            Label { text: "Queue names can't contain spaces, / or #."; color: win.c.dark_foreground; font.pixelSize: 12 }
            RowLayout {
                spacing: 18
                Hint { key: "enter"; text: "rename"; onActivated: renameField.accepted() }
                Hint { key: "esc"; text: "cancel"; onActivated: renameDialog.close() }
            }
        }
    }

    TuiPopup {
        id: removeDialog
        width: 480
        onOpened: removeKeys.forceActiveFocus()
        onClosed: win.focusList()
        contentItem: FocusScope {
            id: removeKeys
            implicitHeight: removeColumn.implicitHeight
            implicitWidth: removeColumn.implicitWidth
            Keys.onReturnPressed: { backend.removePrinter(); removeDialog.close() }
            Keys.onEnterPressed: { backend.removePrinter(); removeDialog.close() }
            ColumnLayout {
                id: removeColumn
                anchors.fill: parent
                spacing: 10
                Label { text: "Remove " + backend.selectedPrinter + "?"; color: win.c.red; font.bold: true; font.pixelSize: 16 }
                Rectangle { height: 1; color: win.c.muted; Layout.fillWidth: true }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: win.c.foreground
                    text: "Its queue and any waiting jobs are deleted. The printer itself is untouched, and you can add it again later."
                }
                RowLayout {
                    spacing: 18
                    Hint { key: "enter"; text: "remove"; onActivated: { backend.removePrinter(); removeDialog.close() } }
                    Hint { key: "esc"; text: "keep it"; onActivated: removeDialog.close() }
                }
            }
        }
    }
}
