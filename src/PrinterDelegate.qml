import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: row
    width: ListView.view.width
    height: 46
    readonly property bool current: ListView.isCurrentItem
    readonly property var c: theme.colors
    readonly property color tone: !model.enabled || model.severity === "error" ? c.red
                                : model.severity === "warning" ? c.yellow
                                : model.stateText === "Printing" ? c.accent : c.green

    Rectangle { anchors.fill: parent; color: row.current ? row.c.selection : "transparent" }
    MouseArea { anchors.fill: parent; onClicked: { row.ListView.view.currentIndex = index; row.ListView.view.forceActiveFocus() } }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 0
        Item { Layout.fillHeight: true }
        RowLayout {
            spacing: 8
            Label { text: row.current ? "▸" : " "; color: row.c.accent; font.bold: true }
            Label { text: "●"; color: row.tone }
            Label {
                text: model.name
                color: row.current ? row.c.bright_foreground : row.c.foreground
                font.bold: row.current
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label { visible: model.isDefault; text: "★"; color: row.c.accent }
            Label { visible: model.activeJobs > 0; text: model.activeJobs + "▤"; color: row.c.accent }
        }
        Label {
            Layout.leftMargin: 38
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: (model.enabled ? model.stateText : "Paused") + (model.accepting ? "" : " · rejecting jobs")
                  + (model.reasons ? " · " + model.reasons : "")
            color: model.reasons ? row.tone : row.c.dark_foreground
            font.pixelSize: 12
        }
        Item { Layout.fillHeight: true }
    }
}
