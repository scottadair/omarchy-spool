import QtQuick
import QtQuick.Controls

// A bordered box with its title sitting on the top edge, like lazygit and btop.
Item {
    id: panel
    property string title
    property bool focused: false
    default property alias content: body.data
    property alias body: body

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: titleLabel.implicitHeight / 2
        color: "transparent"
        border.width: 1
        border.color: panel.focused ? theme.colors.accent : theme.colors.muted
    }
    Rectangle {
        x: 12
        width: titleLabel.implicitWidth + 8
        height: titleLabel.implicitHeight
        color: theme.colors.background
        Label {
            id: titleLabel
            anchors.centerIn: parent
            text: panel.title
            color: panel.focused ? theme.colors.accent : theme.colors.foreground
            font.bold: true
        }
    }
    Item {
        id: body
        anchors.fill: parent
        anchors.margins: 14
        anchors.topMargin: titleLabel.implicitHeight + 8
    }
}
