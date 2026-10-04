import QtQuick
import QtQuick.Controls.Material

// A small rounded label. `tone` picks the color: "accent", "ok", "warn", "error" or "muted".
Rectangle {
    id: chip
    property string text
    property string tone: "muted"
    readonly property color base: tone === "accent" ? theme.accent
                                : tone === "ok" ? "#5fd08a"
                                : tone === "warn" ? "#f0a030"
                                : tone === "error" ? "#ff6b6b" : "#9a9aa3"

    implicitWidth: label.implicitWidth + 16
    implicitHeight: 22
    radius: 11
    color: Qt.rgba(base.r, base.g, base.b, 0.16)
    border.color: Qt.rgba(base.r, base.g, base.b, 0.45)

    Label {
        id: label
        anchors.centerIn: parent
        text: chip.text
        font.pixelSize: 12
        color: chip.base
    }
}
