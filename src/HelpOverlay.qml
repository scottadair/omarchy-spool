import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Popup {
    id: help
    modal: true
    anchors.centerIn: parent
    width: 520
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var rows: [
        ["↑ ↓  /  K J", "Select printer (J is Jobs)"],
        ["A", "Add a printer"],
        ["T", "Print a test page"],
        ["D", "Make default"],
        ["P", "Pause / resume"],
        ["G", "Accept / reject jobs"],
        ["R", "Rename"],
        ["Delete", "Remove (asks first)"],
        ["O", "Edit defaults (paper, duplex, color, quality)"],
        ["J", "Jobs for this printer"],
        ["X  /  R", "In Jobs: cancel  /  restart"],
        ["F5", "Refresh"],
        ["?", "This help"],
        ["Q", "Quit"],
    ]

    contentItem: ColumnLayout {
        spacing: 10
        Label { text: "Keyboard shortcuts"; font.pixelSize: 20; font.weight: Font.DemiBold }
        Repeater {
            model: help.rows
            RowLayout {
                spacing: 16
                Label { text: modelData[0]; color: theme.accent; font.family: "monospace"; Layout.preferredWidth: 130 }
                Label { text: modelData[1]; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
    }
}
