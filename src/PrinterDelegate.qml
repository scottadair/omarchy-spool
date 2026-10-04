import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ItemDelegate {
    id: row
    width: ListView.view.width
    height: 68
    highlighted: ListView.isCurrentItem
    onClicked: { ListView.view.currentIndex = index; ListView.view.forceActiveFocus() }

    readonly property string tone: !enabled_ ? "error" : severity === "error" ? "error" : severity === "warning" ? "warn"
                                  : stateText === "Printing" ? "accent" : "ok"
    readonly property bool enabled_: model.enabled

    background: Rectangle {
        radius: 10
        color: row.highlighted ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.16)
                               : row.hovered ? "#1c1c20" : "transparent"
        border.color: row.highlighted ? Material.accent : "transparent"
        border.width: 1
    }

    contentItem: RowLayout {
        spacing: 12
        Rectangle {
            Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5
            color: row.tone === "accent" ? theme.accent : row.tone === "ok" ? "#5fd08a" : row.tone === "warn" ? "#f0a030" : "#ff6b6b"
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                spacing: 6
                Label {
                    text: model.name
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label { visible: model.isDefault; text: "★"; color: theme.accent; font.pixelSize: 14 }
            }
            Label {
                // The state, then the first thing that's wrong with it.
                text: model.stateText + (model.accepting ? "" : " · not accepting jobs")
                      + (model.reasons ? " · " + model.reasons : "")
                color: "#9a9aa3"
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        Label {
            visible: model.activeJobs > 0
            text: model.activeJobs + (model.activeJobs === 1 ? " job" : " jobs")
            color: theme.accent
            font.pixelSize: 12
        }
    }
}
