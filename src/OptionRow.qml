import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Paper size   ◂ A4 ▸": Left and Right pick a value, and a moment after the
// last press it is applied, so holding the key doesn't ask for a password per step.
FocusScope {
    id: row
    property var opt
    signal up()
    signal down()
    signal leave()

    readonly property var c: theme.colors
    readonly property var values: opt ? opt.values : []
    readonly property int currentIndex: Math.max(0, values.findIndex(v => v.value === (opt ? opt.current : "")))
    property int pending: -1
    readonly property int shown: pending >= 0 ? pending : currentIndex

    // Once the printer reports the new value the pending choice is no longer needed.
    onCurrentIndexChanged: pending = -1

    function step(delta) {
        const next = Math.max(0, Math.min(values.length - 1, shown + delta))
        if (next === shown) return
        pending = next
        apply.restart()
    }

    Timer {
        id: apply
        interval: 700
        onTriggered: {
            if (row.pending >= 0 && row.pending !== row.currentIndex)
                backend.setOption(row.opt.key, row.values[row.pending].value)
            else
                row.pending = -1
        }
    }

    implicitHeight: 30
    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)
    Keys.onUpPressed: up()
    Keys.onDownPressed: down()
    Keys.onEscapePressed: leave()

    Rectangle { anchors.fill: parent; color: row.activeFocus ? row.c.selection : "transparent" }
    MouseArea { anchors.fill: parent; onClicked: row.forceActiveFocus() }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8
        Label { text: row.activeFocus ? "▸" : " "; color: row.c.accent; font.bold: true }
        Label { text: row.opt ? row.opt.label : ""; color: row.c.dark_foreground; Layout.preferredWidth: 100 }
        Label {
            text: "◂"
            color: row.shown > 0 ? row.c.accent : row.c.muted
            MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: { row.forceActiveFocus(); row.step(-1) } }
        }
        Label {
            text: row.values.length ? row.values[row.shown].label : ""
            color: row.pending >= 0 ? row.c.accent : row.c.bright_foreground
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: "▸"
            color: row.shown < row.values.length - 1 ? row.c.accent : row.c.muted
            MouseArea { anchors.fill: parent; anchors.margins: -6; onClicked: { row.forceActiveFocus(); row.step(1) } }
        }
    }
}
