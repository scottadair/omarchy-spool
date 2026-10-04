import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Active and completed jobs for the selected printer.
Panel {
    id: view
    title: "Jobs · " + backend.selectedPrinter
    focused: true
    property alias list: list
    readonly property var c: theme.colors

    Label {
        visible: list.count === 0
        anchors.centerIn: parent
        text: "No jobs. Nothing has been printed here lately."
        color: view.c.dark_foreground
    }

    ListView {
        id: list
        anchors.fill: parent
        model: backend.jobs
        clip: true
        currentIndex: 0
        keyNavigationEnabled: true
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            id: jobRow
            width: ListView.view.width
            height: 46
            readonly property bool active: model.active
            readonly property int jobId: model.id
            readonly property bool current: ListView.isCurrentItem
            readonly property color tone: model.state === 9 ? view.c.green : model.state === 7 ? view.c.dark_foreground
                                        : model.state === 8 ? view.c.red : model.state === 5 ? view.c.accent : view.c.yellow

            Rectangle { anchors.fill: parent; color: jobRow.current ? view.c.selection : "transparent" }
            MouseArea { anchors.fill: parent; onClicked: jobRow.ListView.view.currentIndex = index }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 0
                Item { Layout.fillHeight: true }
                RowLayout {
                    spacing: 12
                    Label { text: jobRow.current ? "▸" : " "; color: view.c.accent; font.bold: true }
                    Label { text: "#" + model.id; color: view.c.dark_foreground; Layout.preferredWidth: 48 }
                    Label { text: model.stateText.toUpperCase(); color: jobRow.tone; font.bold: true; Layout.preferredWidth: 100 }
                    Label {
                        text: model.name || "(untitled)"
                        color: jobRow.current ? view.c.bright_foreground : view.c.foreground
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
                Label {
                    Layout.leftMargin: 76
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: view.c.dark_foreground
                    text: model.user + " · " + model.kbytes + " KB · " + (model.finished || model.created)
                          + (model.reasons ? " · " + model.reasons : "")
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
