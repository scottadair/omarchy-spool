import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Active and completed jobs for the selected printer.
ColumnLayout {
    id: view
    spacing: 12
    signal close()
    property alias list: list

    RowLayout {
        spacing: 12
        Button { text: "← Printers (Esc)"; flat: true; onClicked: view.close() }
        Label { text: "Jobs on " + backend.selectedPrinter; font.pixelSize: 20; font.weight: Font.DemiBold }
        Item { Layout.fillWidth: true }
        Button {
            text: "Cancel (X)"
            enabled: list.currentItem && list.currentItem.active
            onClicked: backend.cancelJob(list.currentItem.jobId)
        }
        Button {
            text: "Restart (R)"
            enabled: list.currentItem && !list.currentItem.active
            onClicked: backend.restartJob(list.currentItem.jobId)
        }
    }

    Label {
        visible: list.count === 0
        text: "No jobs. Nothing has been printed here lately."
        color: "#9a9aa3"
        Layout.topMargin: 30
        Layout.alignment: Qt.AlignHCenter
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: backend.jobs
        clip: true
        currentIndex: 0
        keyNavigationEnabled: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 4
        ScrollBar.vertical: ScrollBar {}

        delegate: ItemDelegate {
            id: jobRow
            width: ListView.view.width
            height: 56
            highlighted: ListView.isCurrentItem
            readonly property bool active: model.active
            readonly property int jobId: model.id
            onClicked: ListView.view.currentIndex = index

            background: Rectangle {
                radius: 8
                color: jobRow.highlighted ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.16) : "transparent"
                border.color: jobRow.highlighted ? Material.accent : "transparent"
            }
            contentItem: RowLayout {
                spacing: 14
                Label { text: "#" + model.id; color: "#6d6d75"; Layout.preferredWidth: 46 }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Label { text: model.name || "(untitled)"; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label {
                        text: model.user + " · " + model.kbytes + " KB · " + (model.finished || model.created)
                              + (model.reasons ? " · " + model.reasons : "")
                        color: "#9a9aa3"; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true
                    }
                }
                Chip {
                    text: model.stateText
                    tone: model.state === 9 ? "ok" : model.state === 7 ? "muted" : model.state === 8 ? "error"
                          : model.state === 5 ? "accent" : "warn"
                }
            }
        }
    }
}
