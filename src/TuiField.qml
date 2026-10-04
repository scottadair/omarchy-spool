import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

TextField {
    color: theme.colors.bright_foreground
    placeholderTextColor: theme.colors.dark_foreground
    selectionColor: theme.colors.accent
    selectedTextColor: theme.accentForeground
    leftPadding: 10
    rightPadding: 10
    topPadding: 8
    bottomPadding: 8
    background: Rectangle {
        color: theme.colors.dark_background
        border.width: 1
        border.color: parent.activeFocus ? theme.colors.accent : theme.colors.muted
    }
}
