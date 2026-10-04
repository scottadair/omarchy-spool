import QtQuick
import QtQuick.Controls

// One entry of the key bar: the key in the accent, what it does dimmed.
Item {
    id: hint
    property string key
    property string text
    signal activated()
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    Row {
        id: row
        spacing: 6
        Label { text: hint.key; color: theme.colors.accent; font.bold: true }
        Label { text: hint.text; color: theme.colors.dark_foreground }
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: hint.activated() }
}
