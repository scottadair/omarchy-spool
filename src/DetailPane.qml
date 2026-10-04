import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The right-hand side: who the printer is, what's wrong with it, and its own defaults.
Panel {
    id: pane
    title: backend.selectedPrinter || "Printer"
    focused: optionsFocused

    signal leaveOptions()

    readonly property var p: backend.selected
    readonly property var c: theme.colors
    readonly property bool has: backend.selectedPrinter !== ""
    readonly property bool optionsFocused: {
        for (let i = 0; i < rows.count; ++i)
            if (rows.itemAt(i) && rows.itemAt(i).activeFocus) return true
        return false
    }

    function focusOptions() {
        if (rows.count > 0) { rows.itemAt(0).forceActiveFocus(); return true }
        return false
    }

    Label {
        anchors.centerIn: parent
        visible: !pane.has
        color: pane.c.dark_foreground
        horizontalAlignment: Text.AlignHCenter
        text: backend.loaded ? "No printers yet.\nPress  a  to add one." : "Looking for printers…"
    }

    Flickable {
        anchors.fill: parent
        visible: pane.has
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 14

            RowLayout {
                spacing: 8
                Tag { visible: !!pane.p.isDefault; text: "Default"; tone: pane.c.accent }
                Tag {
                    text: !pane.p.enabled ? "Paused" : pane.p.stateText
                    tone: !pane.p.enabled ? pane.c.red : pane.p.stateText === "Printing" ? pane.c.accent : pane.c.green
                }
                Tag { visible: pane.p.accepting === false; text: "Rejecting jobs"; tone: pane.c.yellow }
            }

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 4
                Label { text: "Model"; color: pane.c.dark_foreground; visible: !!pane.p.makeModel }
                Label { text: pane.p.makeModel || ""; visible: !!pane.p.makeModel; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: "Location"; color: pane.c.dark_foreground; visible: !!pane.p.location }
                Label { text: pane.p.location || ""; visible: !!pane.p.location; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: "Device"; color: pane.c.dark_foreground }
                Label { text: pane.p.deviceUri || ""; Layout.fillWidth: true; elide: Text.ElideMiddle }
            }

            // State reasons: toner low, paper jam and friends.
            ColumnLayout {
                visible: pane.p.reasons && pane.p.reasons.length > 0
                spacing: 2
                Repeater {
                    model: pane.p.reasons || []
                    Label {
                        text: "! " + modelData
                        color: pane.p.severity === "error" ? pane.c.red : pane.c.yellow
                    }
                }
            }

            RowLayout {
                spacing: 8
                Label { text: "Defaults for new jobs"; color: pane.c.accent; font.bold: true }
                Rectangle { height: 1; color: pane.c.muted; Layout.fillWidth: true }
                Hint { key: "o"; text: "edit" }
            }

            Label {
                visible: rows.count === 0
                text: "This printer doesn't report any choices."
                color: pane.c.dark_foreground
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                // Counting rows (not listing them) keeps the rows, and the keyboard
                // focus on them, alive when the printer reports a new current value.
                Repeater {
                    id: rows
                    model: backend.options.length
                    delegate: OptionRow {
                        Layout.fillWidth: true
                        opt: backend.options[index]
                        onUp: if (index > 0) rows.itemAt(index - 1).forceActiveFocus()
                        onDown: if (index < rows.count - 1) rows.itemAt(index + 1).forceActiveFocus()
                        onLeave: pane.leaveOptions()
                    }
                }
            }
        }
    }
}
