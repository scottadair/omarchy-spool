import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A modal box in the same square style as the panels.
Popup {
    modal: true
    anchors.centerIn: parent
    padding: 20
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "#a0000000" }
    background: Rectangle {
        color: theme.colors.background
        border.width: 1
        border.color: theme.colors.accent
    }
}
