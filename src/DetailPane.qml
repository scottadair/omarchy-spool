import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The right-hand side: who the printer is, what's wrong, the actions, and its own options.
Item {
    id: pane
    signal leaveOptions()
    signal rename()
    signal remove()
    signal showJobs()

    readonly property var p: backend.selected
    readonly property bool has: backend.selectedPrinter !== ""

    function focusOptions() {
        if (options.count > 0) { options.itemAt(0).forceActiveFocus(); return true }
        return false
    }

    Label {
        anchors.centerIn: parent
        visible: !pane.has
        color: "#9a9aa3"
        horizontalAlignment: Text.AlignHCenter
        text: backend.loaded ? "No printers yet.\nPress A to add one." : "Looking for printers…"
        font.pixelSize: 16
    }

    Flickable {
        anchors.fill: parent
        visible: pane.has
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 18

            ColumnLayout {
                spacing: 6
                RowLayout {
                    spacing: 10
                    Label { text: pane.p.name || ""; font.pixelSize: 24; font.weight: Font.DemiBold }
                    Chip { visible: !!pane.p.isDefault; text: "Default"; tone: "accent" }
                    Chip {
                        text: !pane.p.enabled ? "Paused" : pane.p.stateText
                        tone: !pane.p.enabled ? "error" : pane.p.stateText === "Printing" ? "accent" : "ok"
                    }
                    Chip { visible: pane.p.accepting === false; text: "Rejecting jobs"; tone: "warn" }
                }
                Label {
                    visible: !!pane.p.makeModel
                    text: pane.p.makeModel || ""
                    color: "#9a9aa3"
                }
                Label {
                    text: pane.p.deviceUri || ""
                    color: "#6d6d75"; font.pixelSize: 12
                    elide: Text.ElideMiddle; Layout.fillWidth: true
                }
            }

            // State reasons: toner low, paper jam and friends.
            Flow {
                Layout.fillWidth: true
                spacing: 8
                visible: pane.p.reasons && pane.p.reasons.length > 0
                Repeater {
                    model: pane.p.reasons || []
                    Chip { text: modelData; tone: pane.p.severity === "error" ? "error" : "warn" }
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8
                Button { text: "Test page (T)"; onClicked: backend.printTestPage() }
                Button { text: pane.p.isDefault ? "Is default" : "Make default (D)"; enabled: !pane.p.isDefault; onClicked: backend.setDefault() }
                Button { text: pane.p.enabled ? "Pause (P)" : "Resume (P)"; onClicked: backend.togglePaused() }
                Button { text: pane.p.accepting ? "Reject jobs (G)" : "Accept jobs (G)"; onClicked: backend.toggleAccepting() }
                Button { text: "Rename (R)"; onClicked: pane.rename() }
                Button { text: "Jobs (J)"; onClicked: pane.showJobs() }
                Button { text: "Remove (Del)"; Material.foreground: "#ff6b6b"; onClicked: pane.remove() }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#26262b" }

            Label { text: "Defaults for new jobs  (O to edit)"; color: "#9a9aa3"; font.pixelSize: 13 }

            Label {
                visible: options.count === 0
                text: "This printer doesn't report any choices."
                color: "#6d6d75"
            }

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 12
                Layout.fillWidth: true
                Repeater {
                    id: options
                    model: backend.options
                    delegate: ColumnLayout {
                        id: opt
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        function forceActiveFocus() { combo.forceActiveFocus() }
                        Label { text: modelData.label; color: "#9a9aa3"; font.pixelSize: 12 }
                        ComboBox {
                            id: combo
                            Layout.fillWidth: true
                            model: modelData.values
                            textRole: "label"
                            currentIndex: Math.max(0, modelData.values.findIndex(v => v.value === modelData.current))
                            onActivated: index => backend.setOption(modelData.key, modelData.values[index].value)
                            Keys.onEscapePressed: event => { pane.leaveOptions(); event.accepted = true }
                        }
                    }
                }
            }
        }
    }
}
