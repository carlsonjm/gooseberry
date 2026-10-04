// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import org.kde.kirigami as Kirigami

// The board in a window of its own, as Gooseberry's own entry opens it, so it
// can stand as a card beside the work.
Kirigami.ApplicationWindow {
    id: root

    required property QtObject shell

    title: i18n("Gooseberry")
    width: 1100
    height: 720
    minimumWidth: 600
    minimumHeight: 420
    visible: false
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    // The board starts on Today, or on the place a note sits in.
    function present(place) {
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
