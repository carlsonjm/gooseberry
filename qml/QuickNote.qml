// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The quick note, inside its card: the page with the cursor in it, the
// note's colour, the folder it is kept in and the window it is stuck to, and
// Done. What is written is kept from its
// first letter. The page takes the room the card has; when the keys shorten
// the card past what all of it needs, the rest scrolls.
Item {
    id: quick

    required property QtObject capture
    // The application's name, as its desktop file gives it.
    property string title: Qt.application.displayName

    signal boardRequested()

    readonly property alias editor: area
    property bool choosingFolder: false
    property bool choosingWindow: false
    // The open windows Stuck to offers, read as it opens.
    property var windows: []
    // Words for reminders, what Remind offers and the open windows: the
    // shell, where there is one (Shell::reminderLabel,
    // Shell::reminderChoices, Shell::openWindows).
    property QtObject words: null
    property bool choosingReminder: false
    property bool pickingTime: false
    property var reminderChoices: []

    function focusText() {
        choosingFolder = false;
        choosingWindow = false;
        if (capture.checklist) {
            checklistPad.focusLast();
        } else {
            area.forceActiveFocus();
            area.cursorPosition = area.length;
        }
        Qt.inputMethod.show();
    }

    function chooseReminder() {
        choosingReminder = !choosingReminder;
        pickingTime = false;
        if (choosingReminder && words) {
            reminderChoices = words.reminderChoices();
        }
    }

    function remind(choice) {
        if (choice.kind === "pick") {
            timePicker.reset();
            pickingTime = true;
            return;
        }
        if (choice.kind === "opens") {
            capture.setRemindOnOpen();
        } else {
            capture.setRemindAt(choice.time);
        }
        choosingReminder = false;
        focusText();
    }

    function toggleChecklist() {
        if (capture.checklist) {
            capture.makePlain();
        } else {
            capture.makeChecklist();
        }
        Qt.callLater(focusText);
    }

    function chooseFolder() {
        choosingFolder = !choosingFolder;
        choosingWindow = false;
    }

    function chooseWindow() {
        choosingWindow = !choosingWindow;
        choosingFolder = false;
        if (!choosingWindow) {
            return;
        }
        // The open windows, the one in front first; the note's own window
        // first of all when it is no longer open.
        const open = words && words.openWindows ? words.openWindows() : [];
        const list = [];
        const own = capture.window;
        if (own.length > 0 && !open.some(w => w.window === own && w.app === capture.app)) {
            list.push({ window: own, app: capture.app, appName: "", front: false });
        }
        windows = list.concat(open);
    }

    function windowLabel(entry) {
        return entry.appName && entry.appName !== entry.window ? i18nc("application · document", "%1 · %2", entry.appName, entry.window)
                                                               : entry.window;
    }

    function keepIn(name) {
        capture.setFolder(name);
        focusText();
    }

    function newFolder(name) {
        const problem = capture.makeFolder(name);
        if (problem.length > 0) {
            folderProblem.text = problem;
            return;
        }
        folderName.text = "";
        folderProblem.text = "";
        focusText();
    }

    function stickTo(entry) {
        capture.setStuck(entry ? entry.window : "", entry ? entry.app : "");
        focusText();
    }

    // The card's own colour, round a chosen swatch.
    property color surface: Kirigami.Theme.backgroundColor

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: column.y + column.height + 22
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds

        // The header: the application, the note's colours centred, and All
        // notes at the right.
        Item {
            id: header
            objectName: "header"
            x: 0
            y: 14
            width: flick.width
            height: 44

            Row {
                id: identity
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Rectangle {
                    objectName: "appTile"
                    width: 32
                    height: 32
                    radius: 8
                    color: quick.capture.hexFor("butter")
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: Qt.resolvedUrl("icons/note.svg")
                        color: "#1A1A1A"
                        isMask: true
                    }
                }
                QQC2.Label {
                    objectName: "appName"
                    anchors.verticalCenter: parent.verticalCenter
                    // Left out where the colours need the room.
                    visible: identity.x + 42 + implicitWidth + 12 <= colours.x
                    text: quick.title
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
            }

            Row {
                id: colours
                objectName: "colours"
                // Centred, unless a narrow card would put them under All notes.
                x: Math.max(identity.x + 42, Math.min((header.width - width) / 2, allNotes.x - 8 - width))
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Repeater {
                    model: quick.capture.colours
                    delegate: QQC2.AbstractButton {
                        id: swatch
                        required property var modelData
                        objectName: "colour-" + modelData.name
                        readonly property bool chosen: quick.capture.colour === modelData.name
                        implicitWidth: 44
                        implicitHeight: 44
                        focusPolicy: Qt.NoFocus
                        enabled: !quick.capture.readOnly
                        Accessible.name: modelData.name
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: chosen
                        onClicked: quick.capture.colour = modelData.name
                        contentItem: Item {
                            Rectangle {
                                anchors.centerIn: parent
                                visible: swatch.chosen
                                width: 38
                                height: 38
                                radius: 19
                                color: Kirigami.Theme.textColor
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                visible: swatch.chosen
                                width: 34
                                height: 34
                                radius: 17
                                color: quick.surface
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: 28
                                height: 28
                                radius: 14
                                color: swatch.modelData.hex
                            }
                        }
                    }
                }
            }

            Pill {
                id: allNotes
                objectName: "allNotes"
                compact: true
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: i18n("All notes")
                onClicked: quick.boardRequested()
            }
        }

        ColumnLayout {
            id: column
            x: 22
            y: header.y + header.height + 14
            width: flick.width - 44
            height: Math.max(flick.height - y - 22, implicitHeight)
            spacing: 14

            Rectangle {
                id: page
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 96
                // The page takes whatever room the rest leaves it.
                implicitHeight: 96
                radius: 14
                color: quick.capture.hexFor(quick.capture.colour)
                clip: true

                ChecklistPad {
                    id: checklistPad
                    objectName: "checklistPad"
                    anchors.fill: parent
                    visible: quick.capture.checklist
                    capture: quick.capture
                }

                QQC2.ScrollView {
                    anchors.fill: parent
                    visible: !quick.capture.checklist
                    QQC2.TextArea {
                        id: area
                        text: quick.capture.text
                        onTextChanged: {
                            if (text !== quick.capture.text) {
                                quick.capture.text = text;
                            }
                        }
                        readOnly: quick.capture.readOnly
                        placeholderText: i18n("Write it down…")
                        placeholderTextColor: Qt.rgba(0.1, 0.1, 0.1, 0.45)
                        color: "#1A1A1A"
                        selectionColor: Qt.rgba(0, 0, 0, 0.2)
                        selectedTextColor: "#1A1A1A"
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        font.pixelSize: 22
                        font.weight: Font.Medium
                        leftPadding: 20
                        rightPadding: 20
                        topPadding: 18
                        bottomPadding: 18
                        background: null
                        Accessible.name: i18n("Note")
                    }
                }
            }


            // Checklist and Remind, under the note.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    enabled: !quick.capture.readOnly

                    Pill {
                        objectName: "checklist"
                        text: i18n("Checklist")
                        iconName: "checkbox"
                        checked: quick.capture.checklist
                        onClicked: quick.toggleChecklist()
                    }
                    Pill {
                        objectName: "remind"
                        text: quick.capture.hasReminder && quick.words
                              ? quick.words.reminderLabel(quick.capture.remindAt, quick.capture.remindOnOpen)
                              : i18n("Remind")
                        iconName: "notifications"
                        checked: quick.capture.hasReminder || quick.choosingReminder
                        onClicked: quick.chooseReminder()
                    }
                    Pill {
                        objectName: "noReminder"
                        visible: quick.capture.hasReminder
                        text: "✕"
                        Accessible.name: i18n("No reminder")
                        onClicked: {
                            quick.capture.clearReminder();
                            quick.choosingReminder = false;
                        }
                    }
                }

                Flow {
                    objectName: "reminderChoices"
                    Layout.fillWidth: true
                    visible: quick.choosingReminder && !quick.pickingTime
                    spacing: 8
                    Repeater {
                        model: quick.reminderChoices
                        delegate: Pill {
                            required property var modelData
                            objectName: "remind-" + modelData.kind
                            text: modelData.label
                            onClicked: quick.remind(modelData)
                        }
                    }
                }

                TimePicker {
                    id: timePicker
                    objectName: "timePicker"
                    visible: quick.choosingReminder && quick.pickingTime
                    onPicked: time => {
                        quick.capture.setRemindAt(time);
                        quick.choosingReminder = false;
                        quick.pickingTime = false;
                        quick.focusText();
                    }
                }
            }

            // Where the note is kept, and the window it is stuck to: two
            // chips, both already filled in, each opening its choices.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    enabled: !quick.capture.readOnly

                    Pill {
                        objectName: "folderChip"
                        text: i18n("Folder · %1 ▾", quick.capture.folderLabel)
                        iconName: "folder"
                        checked: quick.choosingFolder
                        Accessible.name: i18n("Folder: %1", quick.capture.folderLabel)
                        onClicked: quick.chooseFolder()
                    }
                    Pill {
                        objectName: "stuckChip"
                        text: quick.capture.stuck ? i18n("Stuck to · %1 ▾", quick.capture.window) : i18n("Not stuck to a window ▾")
                        iconName: "pin"
                        checked: quick.choosingWindow
                        Accessible.name: quick.capture.stuck ? i18n("Stuck to %1", quick.capture.window) : i18n("Not stuck to a window")
                        onClicked: quick.chooseWindow()
                    }
                }

                Flow {
                    objectName: "folderChoices"
                    Layout.fillWidth: true
                    visible: quick.choosingFolder
                    spacing: 8

                    Repeater {
                        model: quick.capture.folders
                        delegate: Pill {
                            required property var modelData
                            objectName: "folder-" + (modelData.name.length > 0 ? modelData.name : "inbox")
                            text: modelData.workspace ? i18n("%1 · this workspace", modelData.label) : modelData.label
                            iconName: modelData.name.length > 0 ? "folder" : "mail-folder-inbox"
                            checked: modelData.chosen
                            onClicked: quick.keepIn(modelData.name)
                        }
                    }

                    QQC2.TextField {
                        id: folderName
                        objectName: "folderName"
                        implicitHeight: 44
                        width: 220
                        placeholderText: i18n("New folder")
                        Accessible.name: i18n("New folder")
                        onAccepted: {
                            if (text.trim().length > 0) {
                                quick.newFolder(text);
                            }
                        }
                        background: Rectangle {
                            radius: 22
                            color: Qt.alpha(Kirigami.Theme.textColor, 0.07)
                            border.width: 1
                            border.color: folderName.activeFocus ? Kirigami.Theme.focusColor : Qt.alpha(Kirigami.Theme.textColor, 0.35)
                        }
                        leftPadding: 16
                        rightPadding: 16
                    }

                    QQC2.Label {
                        id: folderProblem
                        objectName: "folderProblem"
                        visible: text.length > 0
                        height: 44
                        verticalAlignment: Text.AlignVCenter
                        color: Kirigami.Theme.negativeTextColor
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                Flow {
                    objectName: "windowChoices"
                    Layout.fillWidth: true
                    visible: quick.choosingWindow
                    spacing: 8

                    Repeater {
                        model: quick.windows
                        delegate: Pill {
                            required property var modelData
                            required property int index
                            objectName: "window-" + index
                            text: modelData.front ? i18n("%1 · in front", quick.windowLabel(modelData)) : quick.windowLabel(modelData)
                            iconName: "window"
                            checked: quick.capture.stuck && quick.capture.window === modelData.window && quick.capture.app === modelData.app
                            onClicked: quick.stickTo(modelData)
                        }
                    }
                    Pill {
                        objectName: "dontStick"
                        text: i18n("Don't stick to a window")
                        checked: !quick.capture.stuck
                        onClicked: quick.stickTo(null)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 2
                spacing: 10

                Kirigami.Icon {
                    implicitWidth: 16
                    implicitHeight: 16
                    source: quick.capture.problem.length > 0 || quick.capture.readOnly ? "dialog-warning" : "checkmark"
                    color: status.color
                    isMask: true
                }
                QQC2.Label {
                    id: status
                    Layout.maximumWidth: column.width * 0.4
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    text: quick.capture.problem.length > 0 ? i18n("Not kept yet: %1", quick.capture.problem)
                        : quick.capture.readOnly ? i18n("Kept by a newer %1: read only", Qt.application.displayName)
                        : i18n("Saved as you go")
                    color: quick.capture.problem.length > 0 || quick.capture.readOnly ? Kirigami.Theme.negativeTextColor
                                                                                      : Kirigami.Theme.positiveTextColor
                }
                Item {
                    Layout.fillWidth: true
                }
                Pill {
                    visible: quick.capture.kept && !quick.capture.readOnly
                    tall: true
                    objectName: "tuckAway"
                    text: i18n("Tuck away")
                    iconName: Qt.resolvedUrl("icons/tuck.svg")
                    onClicked: quick.capture.tuckAway()
                }
                Pill {
                    visible: quick.capture.kept && !quick.capture.readOnly
                    tall: true
                    objectName: "remove"
                    text: i18n("Remove")
                    iconName: "user-trash"
                    onClicked: quick.capture.remove()
                }
                Pill {
                    tall: true
                    objectName: "done"
                    text: i18n("Done")
                    onClicked: quick.capture.finish()
                }
            }
        }
    }
}
