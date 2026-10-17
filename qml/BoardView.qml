// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The board: every note in one place. The places run down the side, Today,
// then the folders with Inbox first and New folder last, the windows with
// notes stuck to them, and Tucked away; the notes of the chosen one fill the
// rest. A held note lifts, and drops on a folder, a day or the trash. It
// fills its own window, and the quick-note card when the card grows.
Item {
    id: board

    required property QtObject shell
    readonly property QtObject notes: shell.notes
    readonly property QtObject places: shell.places
    readonly property bool searching: search.text.trim().length > 0
    // Today shows the planner beside the notes with no date.
    readonly property bool planning: !searching && notes.place === "today" && !!shell.planner
    property date now: new Date()
    // Drawn where a person can see it: in a shown window, and not faded out.
    readonly property bool shown: visible && Window.window !== null && Window.window.visible
    // The note being carried, while one is: its id, colour and title.
    property var carrying: null
    // Renaming the folder shown, a new one included.
    property bool renaming: false
    readonly property string shownFolder: notes.place.startsWith("folder:") ? notes.place.substring(7) : ""
    readonly property string workspace: shell.currentWorkspace ? shell.currentWorkspace() : ""

    // Each time the board is opened it starts on Today.
    function reset() {
        search.text = "";
        renaming = false;
        notes.place = "today";
        now = new Date();
        if (shell.planner) {
            shell.planner.showToday();
        }
    }

    // A note leaves the place shown as soon as it is tucked away or brought
    // back, so what follows is done here rather than on its card.
    function tuckAway(id) {
        if (notes.tuckAway(id)) {
            notice.show(i18n("Tucked away"), i18n("Bring back"), () => notes.bringBack(id));
        }
    }

    function bringBack(id, placeLabel) {
        if (notes.bringBack(id)) {
            notice.show(i18n("Back in %1", placeLabel));
        }
    }

    function sectionTitle(section) {
        switch (section) {
        case "folders": return i18n("Folders");
        case "windows": return i18n("Stuck to windows");
        }
        return "";
    }

    function iconFor(key) {
        if (key === "inbox") return "mail-folder-inbox";
        if (key === "today") return "view-calendar-day";
        if (key === "tucked") return Qt.resolvedUrl("icons/tuck.svg");
        if (key === "newfolder") return "folder-new";
        if (key.startsWith("window:")) return "window";
        return "folder";
    }

    // One tap makes a folder, shows it, and has its name ready to type over.
    function newFolder() {
        const taken = places.folders;
        let name = i18n("New folder");
        for (let n = 2; taken.indexOf(name) >= 0; ++n) {
            name = i18nc("a new folder's name, numbered", "New folder %1", n);
        }
        const problem = places.makeFolder(name);
        if (problem.length > 0) {
            notice.show(problem);
            return;
        }
        search.text = "";
        notes.place = "folder:" + name;
        renameField.text = name;
        renaming = true;
        renameField.forceActiveFocus();
        renameField.selectAll();
    }

    function renameFolder(name) {
        const from = shownFolder;
        const problem = places.renameFolder(from, name);
        if (problem.length > 0) {
            notice.show(problem);
            return;
        }
        renaming = false;
        notes.place = "folder:" + name.trim().replace(/\s+/g, " ");
    }

    // Its notes go to Inbox, so nothing is asked first; undo puts it back.
    function removeFolder() {
        const name = shownFolder;
        const removed = places.removeFolder(name);
        if (!removed.name) {
            notice.show(places.problem());
            return;
        }
        notes.place = "inbox";
        notice.show(i18n("%1 removed; its notes are in Inbox", name), i18n("Undo"), () => {
            if (places.undoRemoveFolder(removed)) {
                notes.place = "folder:" + name;
            }
        });
    }

    // A note let go over a place: a folder or Inbox keeps it there, Tucked
    // away tucks it, a day plans it, the trash removes it.
    function dropOn(target, id, day) {
        if (target.startsWith("folder:") || target === "inbox") {
            if (places.moveNote(id, target)) {
                notice.show(i18n("Moved to %1", places.labelFor(target)));
            } else {
                notice.show(places.problem());
            }
        } else if (target === "tucked") {
            tuckAway(id);
        } else if (target === "trash") {
            const inTrash = notes.remove(id);
            if (inTrash.length > 0) {
                notice.show(i18n("Moved to the trash"), i18n("Undo"), () => notes.restore(id, inTrash));
            }
        } else if (target === "day") {
            const before = notes.remindOf(id);
            if (notes.planOn(id, day)) {
                notice.show(i18n("Planned for %1", Qt.locale().toString(day, "dddd")), i18n("Undo"), () => notes.setRemind(id, before));
            }
        }
    }

    function lift(card, at) {
        carrying = { id: card.noteId, colour: card.colourHex, title: card.text.trim() };
        follow(at);
    }

    function follow(at) {
        const local = board.mapFromItem(null, at.x, at.y);
        lifted.x = local.x - lifted.width / 2;
        lifted.y = local.y - lifted.height / 2;
    }

    function letGo(at) {
        follow(at);
        const id = carrying ? carrying.id : "";
        lifted.Drag.drop();
        carrying = null;
        return id;
    }

    Timer {
        interval: 60 * 1000
        running: board.shown
        repeat: true
        onTriggered: board.now = new Date()
    }

    Connections {
        target: board.shell.capture
        function onRemoved() {
            if (board.shown) {
                notice.show(i18n("Moved to the trash"), i18n("Undo"), () => board.shell.capture.undoRemove());
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Along the top, as Search's Notes has it: Find or start a note, and
        // the places in one row under it, Today, the folders with Inbox
        // first and New folder last, the windows with notes stuck to them,
        // and Tucked away. A carried note can be let go on a place.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 20
            spacing: 12

            QQC2.TextField {
                id: search
                objectName: "search"
                Layout.fillWidth: true
                implicitHeight: 56
                placeholderText: i18n("Find or start a note")
                Accessible.name: i18n("Find or start a note")
                font.pixelSize: 20
                font.weight: Font.Medium
                leftPadding: 52
                rightPadding: 20
                onTextChanged: board.notes.search = text
                // Enter with words no note has starts a note with them.
                onAccepted: {
                    const words = text.trim();
                    if (words.length === 0 || board.notes.count > 0 || board.shell.store.readOnly) {
                        return;
                    }
                    board.shell.newNoteIn("inbox");
                    board.shell.capture.text = words;
                    text = "";
                }
                background: Rectangle {
                    radius: height / 2
                    color: Qt.alpha(Kirigami.Theme.textColor, 0.09)
                    border.width: search.activeFocus ? 1 : 0
                    border.color: Kirigami.Theme.focusColor
                    Kirigami.Icon {
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 20
                        implicitHeight: 20
                        source: Qt.resolvedUrl("icons/lucide/search.svg")
                        color: Kirigami.Theme.disabledTextColor
                        isMask: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ListView {
                    id: placeList
                    Layout.fillWidth: true
                    implicitHeight: 44
                    orientation: ListView.Horizontal
                    spacing: 4
                    clip: true
                    model: board.places
                    boundsBehavior: Flickable.StopAtBounds
                    // A wheel or a touchpad, either way, moves the row along,
                    // at twice the distance a list moves down.
                    WheelHandler {
                        target: null
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: event => {
                            const pixels = Math.abs(event.pixelDelta.x) + Math.abs(event.pixelDelta.y) > 0
                            const along = 2 * (pixels
                                ? (Math.abs(event.pixelDelta.x) > Math.abs(event.pixelDelta.y) ? event.pixelDelta.x : event.pixelDelta.y)
                                : (Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y) ? event.angleDelta.x : event.angleDelta.y) / 120 * 80);
                            const end = Math.max(0, placeList.contentWidth - placeList.width);
                            placeList.contentX = Math.max(0, Math.min(end, placeList.contentX - along));
                        }
                    }
                    // A thin rule before the folders, the windows and Tucked
                    // away; Today, first, has none.
                    section.property: "section"
                    section.delegate: Item {
                        required property string section
                        width: section.length > 0 ? 17 : 0
                        height: 44
                        Rectangle {
                            anchors.centerIn: parent
                            width: 1
                            height: 20
                            color: Qt.alpha(Kirigami.Theme.textColor, 0.2)
                        }
                    }
                    delegate: QQC2.AbstractButton {
                        id: placeButton
                        required property string key
                        required property string label
                        required property int count
                        readonly property bool current: !board.searching && board.notes.place === key
                        // A carried note can be let go here.
                        readonly property bool takesNotes: key === "inbox" || key === "tucked" || key.startsWith("folder:")
                        readonly property bool newFolder: key === "newfolder"
                        readonly property bool quiet: key === "tucked" || newFolder
                        objectName: "place-" + key
                        implicitHeight: 44
                        // Once there is a folder, New folder is a + after them.
                        readonly property bool plus: newFolder && board.places.folders.length > 0
                        implicitWidth: plus ? 44 : Math.max(44, placeRow.implicitWidth + 28)
                        focusPolicy: Qt.NoFocus
                        Accessible.name: label
                        onClicked: {
                            if (newFolder) {
                                board.newFolder();
                                return;
                            }
                            search.text = "";
                            board.notes.place = key;
                        }
                        DropArea {
                            id: dropHere
                            anchors.fill: parent
                            enabled: placeButton.takesNotes
                            keys: ["gooseberry-note"]
                            onDropped: drop => {
                                drop.accept();
                                // Once the drag has ended: the move can take the
                                // note's card off the board.
                                const key = placeButton.key;
                                const id = drop.source.noteId;
                                Qt.callLater(() => board.dropOn(key, id));
                            }
                        }
                        background: Rectangle {
                            radius: height / 2
                            color: dropHere.containsDrag ? Qt.alpha(Kirigami.Theme.highlightColor, 0.25)
                                 : placeButton.current ? Qt.alpha(Kirigami.Theme.textColor, 0.16)
                                 : placeButton.down ? Qt.alpha(Kirigami.Theme.textColor, 0.08)
                                 : placeButton.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.06) : "transparent"
                            border.width: dropHere.containsDrag ? 2 : 0
                            border.color: Kirigami.Theme.highlightColor
                        }
                        contentItem: Item {
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                visible: placeButton.plus
                                implicitWidth: 20
                                implicitHeight: 20
                                source: "list-add"
                                color: Qt.alpha(Kirigami.Theme.textColor, 0.72)
                                isMask: true
                            }
                            RowLayout {
                                id: placeRow
                                anchors.centerIn: parent
                                visible: !placeButton.plus
                                spacing: 6
                                QQC2.Label {
                                    text: placeButton.newFolder ? i18n("+ New folder") : placeButton.label
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: placeButton.current ? Kirigami.Theme.textColor
                                         : placeButton.quiet ? Kirigami.Theme.disabledTextColor
                                         : Qt.alpha(Kirigami.Theme.textColor, 0.72)
                                }
                                QQC2.Label {
                                    text: placeButton.count
                                    visible: placeButton.count > 0 && placeButton.key !== "today"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            id: main
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            // Under this width the day strip takes a line of its own.
            readonly property bool roomy: width >= 820

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 16
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QQC2.Label {
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Kirigami.Theme.disabledTextColor
                        text: {
                            if (board.searching) return i18n("Search");
                            const place = board.notes.place;
                            if (board.planning) return Qt.locale().toString(board.shell.planner.day, "dddd");
                            if (place === "today") return Qt.locale().toString(board.now, "dddd");
                            if (place.startsWith("window:")) return i18n("Stuck to");
                            if (place.startsWith("folder:") || place === "inbox") return i18n("Folder");
                            return "";
                        }
                        visible: text.length > 0
                    }
                    QQC2.Label {
                        objectName: "heading"
                        visible: !board.renaming
                        Layout.fillWidth: true
                        Accessible.role: Accessible.Heading
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        text: {
                            if (board.searching) return i18np("%1 note", "%1 notes", board.notes.count);
                            const place = board.notes.place;
                            if (board.planning) return Qt.locale().toString(board.shell.planner.day, "d MMMM");
                            if (place === "today") return Qt.locale().toString(board.now, "d MMMM");
                            return board.places.labelFor(place);
                        }
                    }
                    QQC2.TextField {
                        id: renameField
                        objectName: "renameField"
                        visible: board.renaming
                        Layout.fillWidth: true
                        implicitHeight: 48
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                        Accessible.name: i18n("Folder name")
                        onAccepted: {
                            if (text.trim().length > 0) {
                                board.renameFolder(text);
                            }
                        }
                        Keys.onEscapePressed: board.renaming = false
                    }
                    // A folder's own actions, under its name.
                    Flow {
                        objectName: "folderActions"
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        visible: board.shownFolder.length > 0 && !board.searching
                        enabled: !board.shell.store.readOnly
                        spacing: 8
                        Pill {
                            objectName: "renameFolder"
                            compact: true
                            text: board.renaming ? i18n("Keep the name") : i18n("Rename")
                            onClicked: {
                                if (board.renaming) {
                                    board.renaming = false;
                                    return;
                                }
                                renameField.text = board.shownFolder;
                                board.renaming = true;
                                renameField.forceActiveFocus();
                                renameField.selectAll();
                            }
                        }
                        Pill {
                            objectName: "workspaceFolder"
                            compact: true
                            visible: board.workspace.length > 0
                            // Read again whenever the folders change.
                            checked: board.places.folders.length >= 0 && board.workspace.length > 0
                                     && board.places.workspaceFolder(board.workspace) === board.shownFolder
                            text: i18n("New notes on %1 go here", board.workspace)
                            onClicked: {
                                board.places.setWorkspaceFolder(board.workspace, checked ? "" : board.shownFolder);
                            }
                        }
                        Pill {
                            objectName: "removeFolder"
                            compact: true
                            text: i18n("Remove folder")
                            iconName: "edit-delete"
                            onClicked: board.removeFolder()
                        }
                    }
                }

                DayStrip {
                    objectName: "dayStrip"
                    visible: board.planning && main.roomy
                    planner: board.shell.planner ? board.shell.planner : null
                    onNoteDropped: (id, day) => board.dropOn("day", id, day)
                }

            }

            DayStrip {
                objectName: "dayStripBelow"
                Layout.leftMargin: 28
                visible: board.planning && !main.roomy
                planner: board.shell.planner ? board.shell.planner : null
                onNoteDropped: (id, day) => board.dropOn("day", id, day)
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 24
                spacing: 24

                PlannerColumn {
                    objectName: "planner"
                    visible: board.planning
                    Layout.fillHeight: true
                    Layout.preferredWidth: Math.min(360, main.width * 0.45)
                    shell: board.shell
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 16

                    QQC2.Label {
                        objectName: "ideasHeading"
                        visible: board.planning
                        text: i18n("Ideas · no date needed")
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Kirigami.Theme.disabledTextColor
                        Accessible.role: Accessible.Heading
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        QQC2.ScrollView {
                            id: scroller
                            anchors.fill: parent
                            contentWidth: availableWidth
                            contentHeight: grid.implicitHeight
                            // A carried note moves, the board stays still.
                            Binding {
                                target: scroller.contentItem
                                property: "interactive"
                                value: false
                                when: board.carrying !== null
                            }

                            // One note a row, as Search's Notes lists them.
                            Column {
                                id: grid
                                width: scroller.availableWidth - 24
                                // Clear of the floating New note.
                                bottomPadding: 96
                                spacing: 2
                                Repeater {
                                    model: board.notes
                                    delegate: NoteCard {
                                        id: noteCard
                                        width: grid.width
                                        wide: grid.width >= 640
                                        showPlace: board.searching || !board.notes.place.startsWith("folder:") && board.notes.place !== "inbox"
                                        opacity: board.carrying && board.carrying.id === noteId ? 0.35 : 1
                                        onTapped: tucked ? board.bringBack(noteId, placeLabel) : board.shell.openNote(noteId)
                                        onTuckRequested: board.tuckAway(noteId)
                                        onLifted: at => board.lift(noteCard, at)
                                        onCarried: at => board.follow(at)
                                        onLetGo: at => board.letGo(at)
                                    }
                                }
                            }
                        }

                        Kirigami.PlaceholderMessage {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: -12
                            width: Math.min(parent.width - 2 * Kirigami.Units.gridUnit, 420)
                            visible: board.notes.count === 0
                            icon.name: board.iconFor(board.notes.place)
                            text: {
                                if (board.searching) return i18n("No note has those words");
                                switch (board.notes.place) {
                                case "today": return i18n("Nothing written today yet");
                                case "inbox": return i18n("Inbox is empty");
                                case "tucked": return i18n("Nothing tucked away");
                                }
                                return i18n("No notes here");
                            }
                            explanation: {
                                if (board.searching) return "";
                                if (board.shownFolder.length > 0) return i18n("Hold a note anywhere on the board and drop it here, or tap New note.");
                                switch (board.notes.place) {
                                case "inbox": return i18n("A note lands here when no folder is chosen.");
                                case "tucked": return i18n("Tuck a note away to keep it here, out of sight. Tap it to bring it back.");
                                }
                                return i18n("Tap New note, or %1's button, to write one.", Qt.application.displayName);
                            }
                        }
                    }
                }
            }
        }
    }

    // The note being carried, tilted a little, above everything.
    Rectangle {
        id: lifted
        objectName: "lifted"
        readonly property string noteId: board.carrying ? board.carrying.id : ""
        visible: board.carrying !== null
        z: 3
        width: 200
        height: 120
        radius: 12
        rotation: -4
        color: board.carrying ? board.carrying.colour : "transparent"
        Drag.active: board.carrying !== null
        Drag.keys: ["gooseberry-note"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2
        layer.enabled: true
        QQC2.Label {
            anchors.fill: parent
            anchors.margins: 14
            text: board.carrying ? board.carrying.title : ""
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            maximumLineCount: 4
            color: "#1A1A1A"
            font.pixelSize: 15
            font.weight: Font.Medium
        }
    }

    // New note floats in the board's corner, in the highlight colour, in
    // reach while the list scrolls.
    Pill {
        objectName: "newNote"
        z: 1
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        implicitHeight: 56
        tall: true
        accent: true
        visible: board.carrying === null
        text: i18n("New note")
        iconName: "list-add"
        enabled: !board.shell.store.readOnly
        onClicked: board.shell.newNoteIn(board.searching ? "inbox" : board.notes.place)
    }

    // The trash, shown only while a note is carried.
    Rectangle {
        id: trash
        objectName: "trashTarget"
        visible: board.carrying !== null
        z: 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: 125
        width: 220
        height: 56
        radius: 28
        color: trashDrop.containsDrag ? Kirigami.Theme.negativeTextColor : Qt.alpha(Kirigami.Theme.textColor, 0.12)
        border.width: 1
        border.color: Kirigami.Theme.negativeTextColor
        RowLayout {
            anchors.centerIn: parent
            spacing: 8
            Kirigami.Icon {
                implicitWidth: 18
                implicitHeight: 18
                source: "user-trash"
                color: trashDrop.containsDrag ? Kirigami.Theme.backgroundColor : Kirigami.Theme.negativeTextColor
                isMask: true
            }
            QQC2.Label {
                text: i18n("Trash")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: trashDrop.containsDrag ? Kirigami.Theme.backgroundColor : Kirigami.Theme.negativeTextColor
            }
        }
        DropArea {
            id: trashDrop
            anchors.fill: parent
            keys: ["gooseberry-note"]
            onDropped: drop => {
                drop.accept();
                const id = drop.source.noteId;
                Qt.callLater(() => board.dropOn("trash", id));
            }
        }
    }

    Notice {
        id: notice
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: 125
        z: 1
    }
}
