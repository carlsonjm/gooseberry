// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The board: every note in one place, gathered without filing. The places run
// down the side; the notes of the chosen one fill the rest. It fills its own
// window, and the quick-note card when the card grows.
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

    // Each time the board is opened it starts on Today.
    function reset() {
        search.text = "";
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
        case "windows": return i18n("On windows");
        case "projects": return i18n("Projects");
        case "workspaces": return i18n("Workspace");
        }
        return "";
    }

    function iconFor(key) {
        if (key === "loose") return "mail-folder-inbox";
        if (key === "today") return "view-calendar-day";
        if (key === "tucked") return Qt.resolvedUrl("icons/tuck.svg");
        if (key.startsWith("window:")) return "window";
        if (key.startsWith("project:")) return "folder";
        return "virtual-desktops";
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: board.places
                    boundsBehavior: Flickable.StopAtBounds
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
                        objectName: "place-" + key
                        width: ListView.view.width
                        implicitHeight: 44
                        focusPolicy: Qt.NoFocus
                        Accessible.name: label
                        onClicked: {
                            search.text = "";
                            board.notes.place = key;
                        }
                        background: Rectangle {
                            radius: 8
                            color: placeButton.current ? Qt.alpha(Kirigami.Theme.textColor, 0.16)
                                                       : placeButton.down ? Qt.alpha(Kirigami.Theme.textColor, 0.08) : "transparent"
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
                            if (place.startsWith("window:")) return i18n("Window");
                            if (place.startsWith("project:")) return i18n("Project");
                            if (place.startsWith("workspace:")) return i18n("Workspace");
                            return "";
                        }
                        visible: text.length > 0
                    }
                    QQC2.Label {
                        objectName: "heading"
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
                }

                DayStrip {
                    objectName: "dayStrip"
                    visible: board.planning && main.roomy
                    planner: board.shell.planner ? board.shell.planner : null
                }

                Pill {
                    objectName: "newNote"
                    tall: true
                    primary: true
                    text: i18n("New note")
                    iconName: "list-add"
                    enabled: !board.shell.store.readOnly
                    onClicked: board.shell.newNoteIn(board.searching ? "loose" : board.notes.place)
                }
            }

            DayStrip {
                objectName: "dayStripBelow"
                Layout.leftMargin: 28
                visible: board.planning && !main.roomy
                planner: board.shell.planner ? board.shell.planner : null
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

                            Masonry {
                                id: grid
                                width: scroller.availableWidth - 28
                                height: contentHeight + 28
                                model: board.notes
                                delegate: NoteCard {
                                    showPlace: board.searching || !board.notes.place.includes(":")
                                    onTapped: tucked ? board.bringBack(noteId, placeLabel) : board.shell.openNote(noteId)
                                    onTuckRequested: board.tuckAway(noteId)
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
                                case "loose": return i18n("Nothing loose");
                                case "tucked": return i18n("Nothing tucked away");
                                }
                                return i18n("No notes here");
                            }
                            explanation: {
                                if (board.searching) return "";
                                switch (board.notes.place) {
                                case "loose": return i18n("A note that belongs nowhere yet waits here.");
                                case "tucked": return i18n("Tuck a note away to keep it here, out of sight. Tap it to bring it back.");
                                }
                                return i18n("Tap New note, or Gooseberry's button, to write one.");
                            }
                        }
                    }
                }
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
