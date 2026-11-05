// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import org.kde.kirigami as Kirigami

// The board in a window of its own, as Gooseberry's own entry opens it, so it
// can stand as a card beside the work.
Kirigami.ApplicationWindow {
    id: root

    required property QtObject shell

    objectName: "boardWindow"
    title: Qt.application.displayName
    width: 1100
    height: 720
    minimumWidth: 600
    minimumHeight: 420
    visible: false
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    // One page: a sideways swipe is the places row's, not the page stack's.
    pageStack.interactive: false

    // Plasma's Cut, Copy and Paste wait for a finger on the field: a press
    // left over from before the window opened does not bring them up over
    // Find or start a note.
    function forgetTextPress() {
        const forget = Qt.createComponent(Qt.resolvedUrl("ForgetTextPress.qml"));
        if (forget.status === Component.Ready) {
            forget.createObject(root).destroy();
        }
    }

    // The board starts on Today, or on the place a note sits in.
    function present(place) {
        forgetTextPress();
        board.reset();
        if (place) {
            board.notes.place = place;
        }
        visible = true;
        raise();
        requestActivate();
    }

    pageStack.initialPage: Kirigami.Page {
        padding: 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

        BoardView {
            id: board
            anchors.fill: parent
            shell: root.shell
        }
    }
}
