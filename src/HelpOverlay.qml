import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TuiPopup {
    id: help
    width: 520
    readonly property var c: theme.colors

    readonly property var rows: [
        ["↑ ↓  k", "Select printer"],
        ["a", "Add a printer"],
        ["t", "Print a test page"],
        ["d", "Make default"],
        ["p", "Pause / resume"],
        ["g", "Accept / reject jobs"],
        ["r", "Rename"],
        ["Delete", "Remove (asks first)"],
        ["o", "Edit defaults: ↑ ↓ row, ← → value, Esc back"],
        ["j", "Jobs for this printer"],
        ["x  r", "In Jobs: cancel  restart"],
        ["F5", "Refresh"],
        ["?", "This help"],
        ["q", "Quit"],
    ]

    contentItem: ColumnLayout {
        spacing: 6
        Label { text: "Keyboard shortcuts"; color: help.c.accent; font.bold: true; font.pixelSize: 16 }
        Rectangle { height: 1; color: help.c.muted; Layout.fillWidth: true; Layout.bottomMargin: 4 }
        Repeater {
            model: help.rows
            RowLayout {
                spacing: 16
                Label { text: modelData[0]; color: help.c.accent; font.bold: true; Layout.preferredWidth: 90 }
                Label { text: modelData[1]; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
    }
}
