// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window

// A window's stuck notes, shown over it after a tap on the dot in its title
// bar. The surface lies exactly over the window and follows it; each note
// stands where Robin last let it go, kept in proportion when the window is
// resized, and one never placed starts at the top-right, the newest on top.
// A note held and moved stays where it is let go; a tap on it types on it
// where it stands, never in a popup, and its words can be selected and
// copied there. The notes stay up until Robin taps the dot again; a press
// anywhere else reaches the window underneath, as if the notes were not
// there, ends the typing and folds the notes into a small dot where the top
// one stands. A tap on that dot opens them again, the top note ready to type.
Window {
    id: root

    required property QtObject shell
    readonly property QtObject stuck: shell.stuck
    readonly property var notes: stuck ? stuck.shownNotes : []
    // The note typed on where it stands, and where its cursor was, so a note
    // made anew under the words, as when it is moved, keeps its place.
    readonly property string writingId: stuck && stuck.writingId ? stuck.writingId : ""
    property int writingCursor: -1
    // The same size on every window, smaller only on a narrow one.
    readonly property real noteWidth: Math.min(220, Math.max(140, width * 0.3))
    // Clear of the title bar, where an unplaced note starts.
    readonly property real topClear: 44
    // Folded, the notes are one dot at the top note's top-right corner: a
    // 44 px target, drawn as the title bar draws its dot.
    readonly property bool folded: stuck !== null && stuck.folded === true
    readonly property real dotTarget: 44
    property point dotAt: Qt.point(0, 0)

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
        if (root.folded) {
            if (dot.visible) {
                rects.push(Qt.rect(dot.x, dot.y, dot.width, dot.height));
            }
            root.stuck.setPressable(rects);
            return;
        }
        for (let i = 0; i < notesRepeater.count; ++i) {
            const item = notesRepeater.itemAt(i);
            if (item) {
                rects.push(Qt.rect(item.x, item.y, item.width, item.height + 3));
                // Its corner, which reaches past the paper.
                rects.push(item.gripRect);
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
            if (item && (item.selecting || item.offerCopy || item.writing)) {
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

    // The note typed on takes the cursor, and the on-screen keys come up for
    // it, once the surface is drawn and holds the keys.
    function focusWriting() {
        for (let i = 0; i < notesRepeater.count; ++i) {
            const item = notesRepeater.itemAt(i);
            if (item && item.writing) {
                item.takeCursor();
            }
        }
    }

    onWritingIdChanged: {
        writingCursor = -1;
        Qt.callLater(root.tellKeys);
        Qt.callLater(root.focusWriting);
    }

    onActiveChanged: {
        if (active) {
            Qt.callLater(root.focusWriting);
        } else {
            letGoAll();
            // A press on the window underneath, or the keys going to it,
            // ends the typing, kept, and folds the notes.
            if (root.visible && root.stuck) {
                root.stuck.keysLeft();
            }
        }
    }

    onFoldedChanged: pressableLater.restart()
    onDotAtChanged: pressableLater.restart()

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
        } else {
            Qt.callLater(root.focusWriting);
        }
    }

    Repeater {
        id: notesRepeater
        model: root.notes
        onCountChanged: {
            pressableLater.restart();
            Qt.callLater(root.tellKeys);
            Qt.callLater(root.focusWriting);
        }

        Rectangle {
            id: note

            required property var modelData
            required property int index
            readonly property bool placed: modelData.x >= 0 && modelData.y >= 0
            readonly property real freeX: Math.max(0, root.width - width)
            readonly property real freeY: Math.max(0, root.height - height)
            readonly property bool selecting: body.selectedText.length > 0 || touchSelect.selecting
            // Typed on where it stands.
            readonly property bool writing: root.writingId !== "" && root.writingId === modelData.id
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
            // Where the note stood as its corner was taken, held there while
            // it grows, so it grows toward the hand and nowhere else.
            property real liveX: -1
            property real liveY: -1
            // A note on the right of its window is resized by its left
            // corner, toward the middle, where there is room to grow.
            readonly property bool gripLeft: x + width / 2 > root.width / 2
            readonly property rect gripRect: Qt.rect(x + grip.x, y + grip.y, grip.width, grip.height)
            // Once the place comes back kept, or the window changes size, the
            // note stands where its kept place says, in proportion again.
            readonly property point kept: Qt.point(modelData.x, modelData.y)
            onKeptChanged: releasePlace()
            function releasePlace() {
                if (!resizing) {
                    liveX = -1;
                    liveY = -1;
                }
            }
            Connections {
                target: root
                function onWidthChanged() { note.releasePlace(); }
                function onHeightChanged() { note.releasePlace(); }
            }
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
                } else if (!writing) {
                    root.stuck.write(modelData.id);
                }
            }

            // The words as they are kept, the cursor where it was, and the
            // keys; on a touch screen the on-screen keys come up with them.
            function takeCursor() {
                if (!writing) {
                    return;
                }
                if (body.text !== root.stuck.writingText) {
                    body.text = root.stuck.writingText;
                }
                if (!body.activeFocus) {
                    body.forceActiveFocus();
                    body.cursorPosition = root.writingCursor >= 0 ? Math.min(root.writingCursor, body.length)
                                                                  : body.length;
                }
                Qt.inputMethod.show();
            }

            onWritingChanged: {
                if (!writing) {
                    body.deselect();
                    body.focus = false;
                }
            }
            Component.onCompleted: {
                if (writing) {
                    Qt.callLater(root.focusWriting);
                }
            }

            function startResize() {
                liveX = x;
                liveY = y;
                resizing = true;
            }

            function resizeBy(fromX, fromWidth, fromHeight, dx, dy) {
                if (gripLeft) {
                    const right = fromX + fromWidth;
                    liveWidth = Math.max(120, Math.min(right, fromWidth - dx));
                    liveX = right - liveWidth;
                } else {
                    liveWidth = Math.max(120, Math.min(root.width - fromX, fromWidth + dx));
                }
                liveHeight = Math.max(80, Math.min(root.height - liveY, fromHeight + dy));
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
                placeIf(false);
            }

            function letGo() {
                body.deselect();
                offerCopy = false;
                copied = false;
            }

            objectName: "stuck-" + modelData.id
            visible: !root.folded
            width: Math.min(root.width, liveWidth > 0 ? liveWidth : sized ? modelData.width : root.noteWidth)
            height: Math.min(root.height, liveHeight > 0 ? liveHeight : sized ? modelData.height : fitHeight)
            radius: 10
            color: modelData.colourHex
            // The newest note is first in the list and lies on top.
            z: root.notes.length - index
            x: liveX >= 0 ? liveX
                : placed ? Math.min(freeX, modelData.x * root.width) : Math.max(0, root.width - width - 24 - index * 14)
            y: liveY >= 0 ? liveY
                : placed ? Math.min(freeY, modelData.y * root.height) : Math.min(freeY, root.topClear + index * 30)
            Accessible.role: Accessible.Button
            Accessible.name: modelData.title
            onXChanged: pressableLater.restart()
            onYChanged: pressableLater.restart()
            onWidthChanged: pressableLater.restart()
            onHeightChanged: pressableLater.restart()
            onGripLeftChanged: pressableLater.restart()
            onSelectingChanged: Qt.callLater(root.tellKeys)
            onOfferCopyChanged: Qt.callLater(root.tellKeys)

            // The top note's top-right corner is where the notes fold to.
            Binding {
                target: root
                property: "dotAt"
                when: note.index === 0
                value: Qt.point(Math.max(0, Math.min(root.width - root.dotTarget, note.x + note.width - root.dotTarget)),
                                Math.max(0, Math.min(root.height - root.dotTarget, note.y)))
                restoreMode: Binding.RestoreNone
            }

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                z: -1
                radius: parent.radius
                color: "#33000000"
            }

            // The words, selectable as on any page: dragged across with a
            // mouse, or held with a finger and slid along. Selected, Copy
            // shows on the note; Ctrl+C works too. More words than the note
            // holds scroll: a finger slides them, a wheel turns them.
            Flickable {
                id: words
                objectName: "stuckScroll-" + note.modelData.id
                x: 14
                y: 14
                width: parent.width - 28
                height: note.sized || note.liveHeight > 0 ? Math.max(0, note.height - 28 - note.toolsRoom)
                                                          : Math.min(body.implicitHeight, root.noteWidth - 28)
                contentWidth: width
                contentHeight: body.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                readonly property bool overflows: body.implicitHeight > height + 1
                // Scrolled only by the handlers below, so a finger's hold
                // still selects: a finger slides the words, a mouse keeps its
                // drag to select them and turns the wheel to scroll.
                interactive: false

                function scrollTo(y) {
                    contentY = Math.max(0, Math.min(contentHeight - height, y));
                }

                // The cursor stays in sight as words are typed below the fold.
                function keepInSight(r) {
                    if (r.y < contentY) {
                        contentY = r.y;
                    } else if (r.y + r.height > contentY + height) {
                        contentY = Math.min(contentHeight - height, r.y + r.height - height);
                    }
                }

                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    enabled: words.overflows
                    target: null
                    // A notch scrolls 80 pixels, as on the note card.
                    onWheel: event => {
                        const step = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 120 * 80;
                        words.scrollTo(words.contentY - step);
                    }
                }

                TextEdit {
                    id: body
                    objectName: "stuckWords-" + note.modelData.id
                    width: words.width
                    height: Math.max(implicitHeight, words.height)
                    color: "#1A1A1A"
                    selectionColor: Qt.rgba(0, 0, 0, 0.2)
                    selectedTextColor: "#1A1A1A"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    // Typed on only while it is the note written on; otherwise
                    // its words are read and copied where it stands.
                    readOnly: !note.writing
                    selectByMouse: true
                    persistentSelection: true
                    clip: true
                    Accessible.ignored: true

                    // Its words come from the bus while it is not typed on; while
                    // it is, from the person, each change kept as the quick note
                    // keeps it.
                    Binding {
                        target: body
                        property: "text"
                        value: note.modelData.text
                        when: !note.writing
                        restoreMode: Binding.RestoreNone
                    }
                    onTextChanged: {
                        if (note.writing && root.stuck && text !== root.stuck.writingText) {
                            root.stuck.setWritingText(text);
                        }
                    }
                    onCursorPositionChanged: {
                        if (note.writing && activeFocus) {
                            root.writingCursor = cursorPosition;
                        }
                    }
                    onCursorRectangleChanged: {
                        if (note.writing && activeFocus) {
                            words.keepInSight(cursorRectangle);
                        }
                    }
                    Keys.onEscapePressed: root.stuck.stopWriting()

                    Connections {
                        target: note.writing ? root.stuck : null
                        // Words changed elsewhere while typed on, kept by the
                        // same rules as on the quick note.
                        function onWritingTextChanged() {
                            if (body.text !== root.stuck.writingText) {
                                body.text = root.stuck.writingText;
                            }
                        }
                    }

                    // The words keep a press on them, a mouse's to select; a tap
                    // on them types on the note, or lets go of what is selected.
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
            }

            // A tap types on the note where it stands, unless words are
            // selected: then it lets them go. A finger moves the note from
            // anywhere on it; a mouse moves it from anywhere but the words,
            // which keep a press that starts on them to select.
            TapHandler {
                onPressedChanged: note.pressedWith(pressed)
                onTapped: note.tapped()
            }

            TapHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
                acceptedButtons: Qt.RightButton
                onTapped: note.offerCopy = true
            }

            // Moved, the note stands where the hand leaves it, and in
            // proportion again once that place comes back kept. Dragged on
            // words that overflow, a finger slides the words instead; the
            // note moves from its edges then.
            DragHandler {
                id: noteDrag
                target: null
                enabled: !touchSelect.selecting && !note.resizing
                property point from
                property bool sliding: false
                onActiveChanged: {
                    if (noteDrag.active) {
                        const at = noteDrag.centroid.pressPosition;
                        noteDrag.sliding = words.overflows && at.x >= words.x && at.x < words.x + words.width
                            && at.y >= words.y && at.y < words.y + words.height;
                        noteDrag.from = noteDrag.sliding ? Qt.point(0, words.contentY) : Qt.point(note.x, note.y);
                    }
                    if (!noteDrag.sliding) {
                        note.placeIf(noteDrag.active);
                    }
                }
                onActiveTranslationChanged: {
                    if (!noteDrag.active) {
                        return;
                    }
                    if (noteDrag.sliding) {
                        words.scrollTo(noteDrag.from.y - noteDrag.activeTranslation.y);
                    } else {
                        note.liveX = Math.max(0, Math.min(note.freeX, noteDrag.from.x + noteDrag.activeTranslation.x));
                        note.liveY = Math.max(0, Math.min(note.freeY, noteDrag.from.y + noteDrag.activeTranslation.y));
                    }
                }
            }

            TouchSelect {
                id: touchSelect
                x: words.x
                y: words.y
                width: words.width
                height: words.height
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
            // Half of it reaches past the paper, so it is big enough for a
            // finger without covering the words: a hold on the last line
            // selects, whichever corner the grip is on.
            Item {
                id: grip
                objectName: "stuckResize-" + note.modelData.id
                readonly property real hang: 22
                x: note.gripLeft ? -hang : parent.width - width + hang
                y: parent.height - height + hang
                width: 44
                height: 44
                Accessible.role: Accessible.Grip
                Accessible.name: i18n("Resize")

                property real fromX: 0
                property real fromWidth: 0
                property real fromHeight: 0

                Canvas {
                    x: note.gripLeft ? grip.hang + 6 : parent.width - grip.hang - width - 6
                    y: parent.height - grip.hang - height - 6
                    width: 12
                    height: 12
                    // Drawn again when the corner changes sides.
                    readonly property bool fromLeft: note.gripLeft
                    onFromLeftChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        ctx.strokeStyle = "rgba(26, 26, 26, 0.45)";
                        ctx.lineWidth = 1.5;
                        ctx.lineCap = "round";
                        ctx.beginPath();
                        if (fromLeft) {
                            ctx.moveTo(1, 1);
                            ctx.lineTo(width - 1, height - 1);
                            ctx.moveTo(1, height / 2);
                            ctx.lineTo(width / 2, height - 1);
                        } else {
                            ctx.moveTo(width - 1, 1);
                            ctx.lineTo(1, height - 1);
                            ctx.moveTo(width - 1, height / 2);
                            ctx.lineTo(width / 2, height - 1);
                        }
                        ctx.stroke();
                    }
                }

                // Pressed here, a mouse resizes rather than selects.
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    cursorShape: note.gripLeft ? Qt.SizeBDiagCursor : Qt.SizeFDiagCursor
                    property point from
                    onPressed: mouse => {
                        // Measured against the window, which stays put
                        // while the note grows under the hand.
                        from = mapToItem(root.contentItem, mouse.x, mouse.y);
                        grip.fromX = note.x;
                        grip.fromWidth = note.width;
                        grip.fromHeight = note.height;
                        note.startResize();
                    }
                    onPositionChanged: mouse => {
                        const at = mapToItem(root.contentItem, mouse.x, mouse.y);
                        note.resizeBy(grip.fromX, grip.fromWidth, grip.fromHeight, at.x - from.x, at.y - from.y);
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

    // The notes folded: a dot in the top note's colour with the count in it
    // when there are several, as in the title bar, and the notes' own shadow,
    // so it reads over light and dark windows alike. A tap opens the notes.
    Item {
        id: dot

        readonly property var topNote: root.notes.length > 0 ? root.notes[0] : null
        readonly property color colour: topNote ? topNote.colourHex : "#F2D98A"
        // Dark figures on a light note, light on a dark one.
        readonly property bool light: 0.2126 * colour.r + 0.7152 * colour.g + 0.0722 * colour.b > 0.55

        objectName: "stuckDot"
        visible: root.folded && topNote !== null
        // Dragged, the dot takes the notes with it: where it is let go, the
        // notes stand, in the same order, its corner on their top one's.
        readonly property point moved: dotDrag.active ? dotDrag.activeTranslation : Qt.point(0, 0)
        x: Math.max(0, Math.min(root.width - width, root.dotAt.x + moved.x))
        y: Math.max(0, Math.min(root.height - height, root.dotAt.y + moved.y))
        onXChanged: pressableLater.restart()
        onYChanged: pressableLater.restart()
        width: root.dotTarget
        height: root.dotTarget
        Accessible.role: Accessible.Button
        Accessible.name: i18np("Show the note", "Show the %1 notes", root.notes.length)
        onVisibleChanged: pressableLater.restart()

        Rectangle {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 2
            width: 18
            height: 18
            radius: 9
            color: "#33000000"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 18
            height: 18
            radius: 9
            color: dot.colour
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.27)

            Text {
                anchors.centerIn: parent
                visible: root.notes.length > 1
                text: root.notes.length > 9 ? i18nc("more than nine notes", "9+") : root.notes.length
                color: dot.light ? "#1A1A1A" : "#FFFFFF"
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
        }

        TapHandler {
            onTapped: root.stuck.unfold()
        }

        DragHandler {
            id: dotDrag
            target: null
            // How far it went, kept for the moment it is let go.
            property point last: Qt.point(0, 0)
            onActiveTranslationChanged: {
                if (active) {
                    last = activeTranslation;
                }
            }
            onActiveChanged: {
                if (active) {
                    last = Qt.point(0, 0);
                    return;
                }
                const x = Math.max(0, Math.min(root.width - dot.width, root.dotAt.x + last.x));
                const y = Math.max(0, Math.min(root.height - dot.height, root.dotAt.y + last.y));
                root.moveNotesBy(x - root.dotAt.x, y - root.dotAt.y);
            }
        }
    }

    // Every note over the window moved by the same amount, each kept in it,
    // and each place kept as a note dragged by hand keeps its own.
    function moveNotesBy(dx, dy) {
        if (!root.stuck || root.width <= 0 || root.height <= 0 || (dx === 0 && dy === 0)) {
            return;
        }
        for (let i = 0; i < notesRepeater.count; ++i) {
            const item = notesRepeater.itemAt(i);
            if (!item) {
                continue;
            }
            const x = Math.max(0, Math.min(item.freeX, item.x + dx));
            const y = Math.max(0, Math.min(item.freeY, item.y + dy));
            item.liveX = x;
            item.liveY = y;
            root.stuck.place(item.modelData.id, x / root.width, y / root.height);
        }
    }
}
