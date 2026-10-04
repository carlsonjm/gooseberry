// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami

// The quick note's surface: the room the panels leave, with the note on a card
// the size of the desktop search's, centred. With the on-screen keys up the
// card rises only as far as it must to stay above them, then shortens. All
// notes grows the card into the board, as Apps and Files grow the search; a
// note opened or started there brings the card back to its size. A tap
// outside the card puts it away; the note is already kept.
Window {
    id: root

    required property QtObject shell

    property bool shown: false
    property bool expanded: false
    // Where the on-screen keys lie over this window, as the compositor reports
    // them to the focused window; empty while they are down. The keys hold no
    // room of their own, so the card keeps above them itself.
    property rect keysRect: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle : Qt.rect(0, 0, 0, 0)
    readonly property bool keysUp: keysRect.width > 0 && keysRect.height > 0 && keysRect.y < height
    // How far up from the window's bottom the card keeps clear.
    property real keysReach: keysUp ? height - keysRect.y + 10 : 0
    Behavior on keysReach {
        NumberAnimation {
            id: keysMotion
            duration: 220
            easing.type: Easing.OutCubic
        }
    }

    readonly property alias card: card
    readonly property alias note: note
    readonly property alias board: board

    title: i18n("Gooseberry")
    color: "transparent"
    visible: false

    function open() {
        hideTimer.stop();
        expanded = false;
        visible = true;
        shown = true;
        requestActivate();
        Qt.callLater(note.focusText);
    }

    function putAway() {
        if (!shown) {
            return;
        }
        shown = false;
        Qt.inputMethod.hide();
        hideTimer.restart();
    }

    function expand() {
        board.reset();
        expanded = true;
        Qt.inputMethod.hide();
    }

    function collapse() {
        expanded = false;
        note.focusText();
    }

    // Esc steps back as the search does: the board to the note, then away.
    function back() {
        if (expanded) {
            collapse();
        } else {
            shell.capture.finish();
        }
    }

    Timer {
        id: hideTimer
        interval: Kirigami.Units.longDuration
        onTriggered: root.visible = false
    }

    // The work around the card: a tap there puts the card away.
    Item {
        id: around
        anchors.fill: parent
        TapHandler {
            onTapped: event => {
                const point = card.mapFromItem(around, event.position.x, event.position.y);
                if (!card.contains(point)) {
                    root.shell.capture.finish();
                }
            }
        }
    }

    Rectangle {
        id: card
        objectName: "card"

        readonly property real restHeight: root.expanded ? Math.max(1, root.height - 20) : Math.round(root.height * 0.64)
        readonly property real keysLine: root.height - root.keysReach
        readonly property real highest: Math.min(10, Math.round((root.height - restHeight) / 2))

        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false

        width: root.expanded ? Math.max(1, root.width - 20)
                             : Math.min(root.width - 20, Math.max(Math.round(root.width * 0.64), 420))
        height: Math.max(Math.min(restHeight, 160), Math.min(restHeight, keysLine - highest))
        x: (root.width - width) / 2
        y: Math.max(highest, Math.min(keysLine - height,
                                      root.expanded ? 10 : Math.round((root.height - height) / 2)))
        Behavior on width {
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
        // The keys' own easing already moves the card; chasing it would lag.
        Behavior on height {
            enabled: !keysMotion.running
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }

        radius: 8
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
        clip: true
        opacity: root.shown ? 1 : 0
        scale: root.shown ? 1 : 0.96
        Behavior on opacity {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }

        QuickNote {
            id: note
            anchors.fill: parent
            capture: root.shell.capture
            projects: root.shell.places.projects
            surface: card.color
            opacity: root.expanded ? 0 : 1
            visible: opacity > 0
            enabled: !root.expanded
            Behavior on opacity {
                NumberAnimation {
                    duration: 160
                }
            }
            onBoardRequested: root.expand()
        }

        BoardView {
            id: board
            anchors.fill: parent
            anchors.margins: card.border.width
            shell: root.shell
            collapsible: true
            opacity: root.expanded ? 1 : 0
            visible: opacity > 0
            enabled: root.expanded
            Behavior on opacity {
                NumberAnimation {
                    duration: 160
                }
            }
            onCollapseRequested: root.collapse()
        }
    }

    // Where the desktop moves the keyboard elsewhere, the card goes away
    // with it, as when going back to the work.
    onActiveChanged: {
        if (!active && shown) {
            shell.capture.finish();
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: root.back()
    }

    Connections {
        target: root.shell.capture
        function onFinished() {
            root.putAway();
        }
    }
}
