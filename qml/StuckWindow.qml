// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window

// A window's stuck notes, shown over it after a tap on the dot in its title
// bar. The surface lies exactly over the window and follows it; each note
// stands where Robin last let it go, kept in proportion when the window is
// resized, and one never placed starts at the top-right, the newest on top.
// A note held and moved stays where it is let go; a tap opens it on the
// quick-note card; its words can be selected and copied where it stands. The
// notes stay up until Robin taps the dot, or the stack in Spread, again; a
// press anywhere else reaches the window underneath, as if the notes were not
// there.
Window {
    id: root

    required property QtObject shell
    readonly property QtObject stuck: shell.stuck
    readonly property var notes: stuck ? stuck.shownNotes : []
    // The same size on every window, smaller only on a narrow one.
    readonly property real noteWidth: Math.min(220, Math.max(140, width * 0.3))
    // Clear of the title bar, where an unplaced note starts.
    readonly property real topClear: 44

    objectName: "stuckNotesWindow"
    title: Qt.application.displayName
    color: "transparent"
    flags: Qt.FramelessWindowHint
    visible: false

    // The surface takes presses only on the notes; it is told
    // where they are whenever one moves.
    function tellPressable() {
        if (!root.stuck) {
            return;
        }
        const rects = [];
        for (let i = 0; i < notesRepeater.count; ++i) {
            const item = notesRepeater.itemAt(i);
            if (item) {
                rects.push(Qt.rect(item.x, item.y, item.width, item.height + 3));
            }
        }
        root.stuck.setPressable(rects);
    }

    // Words selected on a note, or Copy offered on one, while the surface
    // holds the keys; told to the shell whenever that changes.
    property bool takesKeys: false

    function tellKeys() {
        let wanted = false;
        for (let i = 0; i < notesRepeater.count; ++i) {
            const item = notesRepeater.itemAt(i);
            if (item && (item.selecting || item.offerCopy)) {
                wanted = true;
            }
        }
        if (wanted !== takesKeys && root.stuck) {
            takesKeys = wanted;
            root.stuck.setTakesKeys(wanted);
        }
    }

    // A press on the window underneath, or the notes going away, lets go of
    // whatever words were selected.
    function letGoAll() {
        for (let i = 0; i < notesRepeater.count; ++i) {
            const item = notesRepeater.itemAt(i);
            if (item) {
                item.letGo();
            }
        }
        tellKeys();
    }

    onActiveChanged: {
        if (!active) {
            letGoAll();
        }
    }

    Timer {
        id: pressableLater
        interval: 0
        onTriggered: root.tellPressable()
    }

    onWidthChanged: pressableLater.restart()
    onHeightChanged: pressableLater.restart()
    onVisibleChanged: {
        pressableLater.restart();
        if (!visible) {
            letGoAll();
        }
    }

    Repeater {
        id: notesRepeater
        model: root.notes
        onCountChanged: {
            pressableLater.restart();
            Qt.callLater(root.tellKeys);
        }

        Rectangle {
            id: note

            required property var modelData
            required property int index
            readonly property bool placed: modelData.x >= 0 && modelData.y >= 0
            readonly property real freeX: Math.max(0, root.width - width)
            readonly property real freeY: Math.max(0, root.height - height)
            readonly property bool selecting: body.selectedText.length > 0 || touchSelect.selecting
            // Resized by its corner, a note keeps that size; otherwise it is
            // as wide as every note and as tall as its words.
            readonly property bool sized: modelData.width > 0 && modelData.height > 0
            readonly property real toolsRoom: tools.visible ? tools.height + 6 : 0
            readonly property real fitHeight: Math.min(root.noteWidth, body.implicitHeight + 28) + toolsRoom
            // The size while the corner is held, and as it was let go until
            // the note comes back with it kept.
            property bool resizing: false
            property real liveWidth: -1
            property real liveHeight: -1
            // When the corner was last let go: the lift that ends a resize is
            // not a tap on the note.
            property real resizedAt: 0
            property bool offerCopy: false
            property bool copied: false

            function placeIf(active) {
                if (!active && root.width > 0 && root.height > 0) {
                    root.stuck.place(modelData.id, x / root.width, y / root.height);
                }
            }

            function copy() {
                if (body.selectedText.length > 0) {
                    body.copy();
                } else {
                    body.selectAll();
                    body.copy();
                    body.deselect();
                }
                copied = true;
                copiedFor.restart();
            }

            // Whether words were selected as the press began, before the
            // press itself could let go of them.
            property bool heldWords: false

            function pressedWith(pressed) {
                if (pressed) {
                    heldWords = selecting || offerCopy;
                }
            }

            function tapped() {
                if (resizing || Date.now() - resizedAt < 400) {
                    return;
                }
                if (heldWords) {
                    letGo();
                } else {
                    root.stuck.openRequested(modelData.id);
                }
            }

            function resizeBy(fromWidth, fromHeight, dx, dy) {
                liveWidth = Math.max(120, Math.min(root.width - x, fromWidth + dx));
                liveHeight = Math.max(80, Math.min(root.height - y, fromHeight + dy));
            }

            function resized() {
                if (!resizing) {
                    return;
                }
                resizing = false;
                resizedAt = Date.now();
                if (liveWidth > 0 && liveHeight > 0) {
                    root.stuck.resize(modelData.id, liveWidth, liveHeight);
                }
            }

            function letGo() {
                body.deselect();
                offerCopy = false;
                copied = false;
            }

            objectName: "stuck-" + modelData.id
            width: Math.min(root.width, liveWidth > 0 ? liveWidth : sized ? modelData.width : root.noteWidth)
            height: Math.min(root.height, liveHeight > 0 ? liveHeight : sized ? modelData.height : fitHeight)
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
            onSelectingChanged: Qt.callLater(root.tellKeys)
            onOfferCopyChanged: Qt.callLater(root.tellKeys)

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                z: -1
                radius: parent.radius
                color: "#33000000"
            }

            // The words, selectable as on any page: dragged across with a
            // mouse, or held with a finger and slid along. Selected, Copy
            // shows on the note; Ctrl+C works too.
            TextEdit {
                id: body
                objectName: "stuckWords-" + note.modelData.id
                x: 14
                y: 14
                width: parent.width - 28
                height: note.sized || note.liveHeight > 0 ? Math.max(0, note.height - 28 - note.toolsRoom)
                                                          : Math.min(implicitHeight, root.noteWidth - 28)
                text: note.modelData.text
                color: "#1A1A1A"
                selectionColor: Qt.rgba(0, 0, 0, 0.2)
                selectedTextColor: "#1A1A1A"
                font.pixelSize: 14
                font.weight: Font.Medium
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                persistentSelection: true
                clip: true
                Accessible.ignored: true

                // The words keep a press on them, a mouse's to select; a tap
                // on them still opens the note, or lets go of what is
                // selected.
                TapHandler {
                    onPressedChanged: note.pressedWith(pressed)
                    onTapped: note.tapped()
                }

                TapHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    acceptedButtons: Qt.RightButton
                    onTapped: note.offerCopy = true
                }
            }

            // A tap opens the note, unless words are selected: then it lets
            // them go. A finger moves the note from anywhere on it; a mouse
            // moves it from anywhere but the words, which keep a press that
            // starts on them to select.
            TapHandler {
                onPressedChanged: note.pressedWith(pressed)
                onTapped: note.tapped()
            }

            TapHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                acceptedButtons: Qt.RightButton
                onTapped: note.offerCopy = true
            }

            DragHandler {
                target: note
                enabled: !touchSelect.selecting && !note.resizing
                xAxis.minimum: 0
                xAxis.maximum: note.freeX
                yAxis.minimum: 0
                yAxis.maximum: note.freeY
                onActiveChanged: note.placeIf(active)
            }

            TouchSelect {
                id: touchSelect
                x: body.x
                y: body.y
                width: body.width
                height: body.height
                target: body
                onSelected: body.forceActiveFocus()
            }

            // Copy, and Select all, along the note's foot while words are
            // selected or after a right-click.
            Row {
                id: tools
                objectName: "stuckTools-" + note.modelData.id
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 26
                anchors.bottomMargin: 6
                spacing: 4
                visible: note.selecting || note.offerCopy

                Repeater {
                    model: [
                        { name: "copy", label: note.copied ? i18n("Copied") : body.selectedText.length > 0 ? i18n("Copy") : i18n("Copy all") },
                        { name: "selectAll", label: i18n("Select all") }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        objectName: "stuck-" + modelData.name
                        width: Math.max(44, word.implicitWidth + 20)
                        height: 30
                        radius: 15
                        color: press.pressed ? "#3A3A3A" : "#1A1A1A"
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.label

                        Text {
                            id: word
                            anchors.centerIn: parent
                            text: parent.modelData.label
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        // Taller than drawn, for a finger.
                        MouseArea {
                            id: press
                            anchors.fill: parent
                            anchors.topMargin: -7
                            anchors.bottomMargin: -7
                            onClicked: {
                                if (parent.modelData.name === "copy") {
                                    note.copy();
                                } else {
                                    body.selectAll();
                                    note.offerCopy = true;
                                }
                            }
                        }
                    }
                }
            }

            // The corner that resizes the note, by finger or pointer; the note
            // keeps the size it is let go at, on this window and any other.
            Item {
                id: grip
                objectName: "stuckResize-" + note.modelData.id
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 44
                height: 44
                Accessible.role: Accessible.Grip
                Accessible.name: i18n("Resize")

                property real fromWidth: 0
                property real fromHeight: 0

                Canvas {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    width: 12
                    height: 12
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        ctx.strokeStyle = "rgba(26, 26, 26, 0.45)";
                        ctx.lineWidth = 1.5;
                        ctx.lineCap = "round";
                        ctx.beginPath();
                        ctx.moveTo(width - 1, 1);
                        ctx.lineTo(1, height - 1);
                        ctx.moveTo(width - 1, height / 2);
                        ctx.lineTo(width / 2, height - 1);
                        ctx.stroke();
                    }
                }

                // Pressed here, a mouse resizes rather than selects.
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    cursorShape: Qt.SizeFDiagCursor
                    property point from
                    onPressed: mouse => {
                        from = mapToItem(note, mouse.x, mouse.y);
                        grip.fromWidth = note.width;
                        grip.fromHeight = note.height;
                        note.resizing = true;
                    }
                    onPositionChanged: mouse => {
                        const at = mapToItem(note, mouse.x, mouse.y);
                        note.resizeBy(grip.fromWidth, grip.fromHeight, at.x - from.x, at.y - from.y);
                    }
                    onReleased: note.resized()
                    onCanceled: note.resized()
                }
            }

            Timer {
                id: copiedFor
                interval: 1500
                onTriggered: note.copied = false
            }
        }
    }
}
