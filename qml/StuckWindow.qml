// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window

// A window's stuck notes, shown over it after a tap on the dot in its title
// bar. The surface lies exactly over the window and follows it; each note
// stands where Robin last let it go, kept in proportion when the window is
// resized, and one never placed starts at the top-right, the newest on top.
// A note held and moved stays where it is let go; a tap opens it on the
// quick-note card. The notes stay up until Robin taps "Put away" above them,
// or the dot or the stack in Spread again; a press anywhere else reaches the
// window underneath, as if the notes were not there.
Window {
    id: root

    required property QtObject shell
    readonly property QtObject stuck: shell.stuck
    readonly property var notes: stuck ? stuck.shownNotes : []
    // The same size on every window, smaller only on a narrow one.
    readonly property real noteWidth: Math.min(220, Math.max(140, width * 0.3))
    // Clear of the title bar and the button, where an unplaced note starts.
    readonly property real topClear: 92

    objectName: "stuckNotesWindow"
    title: Qt.application.displayName
    color: "transparent"
    flags: Qt.FramelessWindowHint
    visible: false

    // The surface takes presses only on the notes and the button; it is told
    // where they are whenever one moves.
    function tellPressable() {
        if (!root.stuck) {
            return;
        }
        const rects = [Qt.rect(putAway.x, putAway.y, putAway.width, putAway.height)];
        for (let i = 0; i < notesRepeater.count; ++i) {
            const item = notesRepeater.itemAt(i);
            if (item) {
                rects.push(Qt.rect(item.x, item.y, item.width, item.height + 3));
            }
        }
        root.stuck.setPressable(rects);
    }

    Timer {
        id: pressableLater
        interval: 0
        onTriggered: root.tellPressable()
    }

    onWidthChanged: pressableLater.restart()
    onHeightChanged: pressableLater.restart()
    onVisibleChanged: pressableLater.restart()

    Rectangle {
        id: putAway

        objectName: "stuckPutAway"
        visible: root.notes.length > 0
        anchors {
            top: parent.top
            right: parent.right
            topMargin: 44
            rightMargin: 24
        }
        width: putAwayLabel.implicitWidth + 28
        height: 36
        radius: height / 2
        color: "#E6202020"
        z: root.notes.length + 1
        Accessible.role: Accessible.Button
        Accessible.name: putAwayLabel.text
        onXChanged: pressableLater.restart()
        onWidthChanged: pressableLater.restart()

        Text {
            id: putAwayLabel
            anchors.centerIn: parent
            text: qsTr("Put away")
            color: "#FFFFFF"
            font.pixelSize: 14
            font.weight: Font.Medium
        }

        TapHandler {
            onTapped: root.stuck.putAway()
        }
    }

    Repeater {
        id: notesRepeater
        model: root.notes
        onCountChanged: pressableLater.restart()

        Rectangle {
            id: note

            required property var modelData
            required property int index
            readonly property bool placed: modelData.x >= 0 && modelData.y >= 0
            readonly property real freeX: Math.max(0, root.width - width)
            readonly property real freeY: Math.max(0, root.height - height)

            objectName: "stuck-" + modelData.id
            width: root.noteWidth
            height: Math.min(root.noteWidth, body.implicitHeight + 28)
            radius: 10
            color: modelData.colourHex
            // The newest note is first in the list and lies on top.
            z: root.notes.length - index
            x: placed ? Math.min(freeX, modelData.x * root.width) : Math.max(0, root.width - width - 24 - index * 14)
            y: placed ? Math.min(freeY, modelData.y * root.height) : Math.min(freeY, root.topClear + index * 30)
            Accessible.role: Accessible.Button
            Accessible.name: modelData.title
            onXChanged: pressableLater.restart()
            onYChanged: pressableLater.restart()
            onHeightChanged: pressableLater.restart()

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                z: -1
                radius: parent.radius
                color: "#33000000"
            }

            Text {
                id: body
                anchors {
                    fill: parent
                    margins: 14
                }
                text: note.modelData.text
                color: "#1A1A1A"
                font.pixelSize: 14
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                textFormat: Text.PlainText
                maximumLineCount: 8
            }

            TapHandler {
                onTapped: root.stuck.openRequested(note.modelData.id)
            }

            DragHandler {
                id: drag
                target: note
                xAxis.minimum: 0
                xAxis.maximum: note.freeX
                yAxis.minimum: 0
                yAxis.maximum: note.freeY
                onActiveChanged: {
                    if (!active && root.width > 0 && root.height > 0) {
                        root.stuck.place(note.modelData.id, note.x / root.width, note.y / root.height);
                    }
                }
            }
        }
    }
}
