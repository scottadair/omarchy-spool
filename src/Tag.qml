import QtQuick
import QtQuick.Controls

// Reverse-video label, the way a TUI marks a badge.
Rectangle {
    id: tag
    property string text
    property color tone: theme.colors.accent
    implicitWidth: label.implicitWidth + 12
    implicitHeight: label.implicitHeight + 2
    color: tone
    Label {
        id: label
        anchors.centerIn: parent
        text: tag.text.toUpperCase()
        color: theme.colors.background
        font.pixelSize: 11
        font.bold: true
    }
}
