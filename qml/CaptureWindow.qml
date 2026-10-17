// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami

// The quick note's surface: the whole display, with the note on a card the
// size of the desktop search's, centred. With the on-screen keys up the card
// rises only as far as it must to stay above them, then shortens. All notes
// opens the board as Gooseberry's own window and the card goes. A note opened
// here from a search's Notes has a bar on top leading back there. A tap
// outside the card puts it away; the note is already kept. In Kadunce's
// Spread the card is drawn where Kadunce gives it Spread's centre.
Window {
    id: root

    required property QtObject shell

    property bool shown: false
    // Where the card may stand: the whole surface, as the desktop's search.
    property rect area: Qt.rect(0, 0, width, height)
    // Holding Kadunce's Spread centre, and the place Kadunce gives it.
    readonly property QtObject guest: shell.guest !== undefined ? shell.guest : null
    readonly property bool asGuest: guest !== null && guest.placed
    readonly property rect guestRect: asGuest ? guest.rect : Qt.rect(0, 0, 0, 0)
    // Where the on-screen keys lie over this window, as the compositor reports
    // them to the focused window; empty while they are down. The keys hold no
    // room of their own, so the card keeps above them itself.
    property rect keysRect: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle : Qt.rect(0, 0, 0, 0)
    readonly property bool keysUp: keysRect.width > 0 && keysRect.height > 0 && keysRect.y < height
    // How far up from the window's bottom the card keeps clear.
    property real keysReach: keysUp ? height - keysRect.y + 10 : 0
    // How long the card takes to move, at Plasma's animation speed: Instant,
    // which Plasma also reports as reduced motion, moves it at once.
    readonly property int travelDuration: Math.round(220 * Math.max(0, Kirigami.Units.longDuration) / 200)
    Behavior on keysReach {
        NumberAnimation {
            id: keysMotion
            duration: root.travelDuration
            easing.type: Easing.OutCubic
        }
    }

    readonly property alias card: card
    readonly property alias note: note
    // The words of the way back, empty when there is none.
    readonly property string backLabel: shell.backLabel !== undefined ? shell.backLabel : ""

    objectName: "quickNoteWindow"
    title: Qt.application.displayName
    color: "transparent"
    visible: false

    function open() {
        hideTimer.stop();
        visible = true;
        shown = true;
        note.choosingReminder = false;
        note.pickingTime = false;
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

    function allNotes() {
        Qt.inputMethod.hide();
        shell.boardFromCard();
    }

    // Esc steps back as the search does: anything open on the note, then
    // back to whoever opened it, or away.
    function back() {
        if (note.closeOpen()) {
            return;
        } else if (backLabel.length > 0) {
            shell.goBack();
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

        readonly property real restHeight: root.asGuest ? root.guestRect.height : Math.round(root.area.height * 0.64)
        readonly property real keysLine: root.height - root.keysReach
        // In Spread the card keeps the place Kadunce gave it and only shortens.
        readonly property real highest: root.asGuest ? root.guestRect.y
            : root.area.y + Math.min(10, Math.round((root.area.height - restHeight) / 2))

        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false

        width: root.asGuest ? root.guestRect.width
             : Math.min(root.area.width - 20, Math.max(Math.round(root.area.width * 0.64), 420))
        height: Math.max(Math.min(restHeight, 160), Math.min(restHeight, keysLine - highest))
        x: root.asGuest ? root.guestRect.x : root.area.x + (root.area.width - width) / 2
        y: root.asGuest ? root.guestRect.y
           : Math.max(highest, Math.min(keysLine - height, root.area.y + Math.round((root.area.height - height) / 2)))
        Behavior on x {
            enabled: root.asGuest
            NumberAnimation {
                duration: root.travelDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on y {
            enabled: root.asGuest
            NumberAnimation {
                duration: root.travelDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on width {
            NumberAnimation {
                duration: root.travelDuration
                easing.type: Easing.OutCubic
            }
        }
        // The keys' own easing already moves the card; chasing it would lag.
        Behavior on height {
            enabled: !keysMotion.running
            NumberAnimation {
                duration: root.travelDuration
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

        // The way back to whoever opened the note here, as the search's own
        // note page has.
        Item {
            id: backBar
            objectName: "backBar"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: visible ? 44 : 0
            visible: root.backLabel.length > 0

            Pill {
                objectName: "back"
                x: 12
                y: 0
                compact: true
                iconName: "go-previous"
                text: root.backLabel
                onClicked: root.shell.goBack()
            }
        }

        QuickNote {
            id: note
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: backBar.bottom
            anchors.bottom: parent.bottom
            capture: root.shell.capture
            title: root.shell.applicationName()
            words: root.shell.reminderLabel !== undefined ? root.shell : null
            surface: card.color
            onBoardRequested: root.allNotes()
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
