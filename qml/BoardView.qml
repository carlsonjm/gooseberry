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
    // Inside the quick-note card, whose corners it follows.
    property bool collapsible: false
    // The note being carried, while one is: its id, colour and title.
    property var carrying: null
    // Naming a new folder in the side list, or renaming the one shown.
    property bool naming: false
    property bool renaming: false
    readonly property string shownFolder: notes.place.startsWith("folder:") ? notes.place.substring(7) : ""
    readonly property string workspace: shell.currentWorkspace ? shell.currentWorkspace() : ""

    // Each time the board is opened it starts on Today.
    function reset() {
        search.text = "";
        naming = false;
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

    function makeFolder(name) {
        const problem = places.makeFolder(name);
        if (problem.length > 0) {
            notice.show(problem);
            return;
        }
        naming = false;
        newFolderName.text = "";
        search.text = "";
        notes.place = "folder:" + name.trim().replace(/\s+/g, " ");
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

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 250
            // Inside the card, its corners follow the card's.
            topLeftRadius: board.collapsible ? 7 : 0
            bottomLeftRadius: board.collapsible ? 7 : 0
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                anchors.topMargin: 14
                spacing: 2

                QQC2.TextField {
                    id: search
                    objectName: "search"
                    Layout.fillWidth: true
                    Layout.bottomMargin: 10
                    implicitHeight: 44
                    placeholderText: i18n("Search notes")
                    leftPadding: 40
                    rightPadding: 14
                    onTextChanged: board.notes.search = text
                    background: Rectangle {
                        radius: 22
                        color: Qt.alpha(Kirigami.Theme.textColor, 0.07)
                        border.width: 1
                        border.color: search.activeFocus ? Kirigami.Theme.focusColor : Qt.alpha(Kirigami.Theme.textColor, 0.35)
                        Kirigami.Icon {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 16
                            implicitHeight: 16
                            source: "search"
                            color: Kirigami.Theme.disabledTextColor
                            isMask: true
                        }
                    }
                }

                ListView {
                    id: placeList
                    footer: Item {
                        width: ListView.view ? ListView.view.width : 0
                        height: 8
                    }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: board.places
                    boundsBehavior: Flickable.StopAtBounds
                    // A wheel notch scrolls by Plasma's step, as KDE's own lists do.
                    Kirigami.WheelHandler { target: placeList }
                    section.property: "section"
                    section.delegate: QQC2.Label {
                        required property string section
                        width: ListView.view.width
                        text: board.sectionTitle(section).toUpperCase()
                        topPadding: 14
                        bottomPadding: 4
                        leftPadding: 10
                        font.pixelSize: 11
                        font.weight: Font.ExtraBold
                        font.letterSpacing: 0.6
                        color: Kirigami.Theme.disabledTextColor
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
                        objectName: "place-" + key
                        width: ListView.view.width
                        implicitHeight: 44
                        focusPolicy: Qt.NoFocus
                        Accessible.name: label
                        onClicked: {
                            if (newFolder) {
                                board.naming = true;
                                newFolderName.forceActiveFocus();
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
                            radius: 8
                            color: dropHere.containsDrag ? Qt.alpha(Kirigami.Theme.highlightColor, 0.25)
                                 : placeButton.current ? Qt.alpha(Kirigami.Theme.textColor, 0.16)
                                 : placeButton.down ? Qt.alpha(Kirigami.Theme.textColor, 0.08) : "transparent"
                            border.width: dropHere.containsDrag ? 2 : 0
                            border.color: Kirigami.Theme.highlightColor
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            Kirigami.Icon {
                                Layout.leftMargin: 10
                                implicitWidth: 16
                                implicitHeight: 16
                                source: board.iconFor(placeButton.key)
                                fallback: "folder"
                                color: Kirigami.Theme.textColor
                                isMask: true
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                text: placeButton.label
                                elide: Text.ElideRight
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: placeButton.newFolder ? Kirigami.Theme.linkColor : Kirigami.Theme.textColor
                            }
                            QQC2.Label {
                                Layout.rightMargin: 10
                                text: placeButton.count
                                visible: placeButton.count > 0
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: placeButton.current ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                            }
                        }
                    }
                }

                // A new folder's name, typed once; Enter makes it.
                QQC2.TextField {
                    id: newFolderName
                    objectName: "newFolderName"
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    visible: board.naming
                    implicitHeight: 44
                    placeholderText: i18n("Name the folder")
                    Accessible.name: i18n("New folder")
                    leftPadding: 14
                    rightPadding: 14
                    onAccepted: {
                        if (text.trim().length > 0) {
                            board.makeFolder(text);
                        }
                    }
                    Keys.onEscapePressed: board.naming = false
                    background: Rectangle {
                        radius: 22
                        color: Qt.alpha(Kirigami.Theme.textColor, 0.07)
                        border.width: 1
                        border.color: newFolderName.activeFocus ? Kirigami.Theme.focusColor : Qt.alpha(Kirigami.Theme.textColor, 0.35)
                    }
                }
            }
        }

        ColumnLayout {
            id: main
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18
            // Under this width the day strip takes a line of its own.
            readonly property bool roomy: width >= 820

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 22
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
                        font.pixelSize: 30
                        font.weight: Font.ExtraBold
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
                        font.weight: Font.ExtraBold
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
                        Layout.topMargin: 6
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

                Pill {
                    objectName: "newNote"
                    tall: true
                    primary: true
                    text: i18n("New note")
                    iconName: "list-add"
                    enabled: !board.shell.store.readOnly
                    onClicked: board.shell.newNoteIn(board.searching ? "inbox" : board.notes.place)
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
                Layout.leftMargin: 28
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
                    spacing: 14

                    QQC2.Label {
                        objectName: "ideasHeading"
                        visible: board.planning
                        text: i18n("Ideas · No date needed").toUpperCase()
                        font.pixelSize: 13
                        font.weight: Font.ExtraBold
                        font.letterSpacing: 0.8
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
                            // A carried note moves, the board stays still.
                            Binding {
                                target: scroller.contentItem
                                property: "interactive"
                                value: false
                                when: board.carrying !== null
                            }

                            Masonry {
                                id: grid
                                width: scroller.availableWidth - 28
                                height: contentHeight + 28
                                model: board.notes
                                delegate: NoteCard {
                                    id: noteCard
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

                        Kirigami.PlaceholderMessage {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: -14
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
