// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The quick note, inside its card: the page with the cursor in it, and one
// quiet line under it saying where the note is kept and what it is stuck to,
// with Checklist, Remind, More and All notes. The colour is a dot on the
// page; what is rarely needed waits under More. What is written is kept from
// its first letter. The page takes the room the card has; when the keys
// shorten the card past what all of it needs, the rest scrolls.
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
    property bool choosingColour: false
    property bool showingMore: false

    function focusText() {
        choosingFolder = false;
        choosingWindow = false;
        choosingColour = false;
        showingMore = false;
        if (capture.checklist) {
            checklistPad.focusLast();
        } else {
            area.forceActiveFocus();
            area.cursorPosition = area.length;
        }
        Qt.inputMethod.show();
    }

    // Closes whatever is open over or under the note, for Esc to step back
    // through before the card goes; true when something was open.
    function closeOpen() {
        const open = choosingFolder || choosingWindow || choosingColour || showingMore || choosingReminder;
        if (open) {
            choosingReminder = false;
            pickingTime = false;
            focusText();
        }
        return open;
    }

    function chooseReminder() {
        choosingReminder = !choosingReminder;
        showingMore = false;
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
        showingMore = false;
    }

    function chooseWindow() {
        choosingWindow = !choosingWindow;
        choosingFolder = false;
        showingMore = false;
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


    // The card's own colour.
    property color surface: Kirigami.Theme.backgroundColor
    readonly property color quiet: Qt.alpha(Kirigami.Theme.textColor, 0.68)

    // A row of More, its glyph and words.
    component MenuRow: QQC2.AbstractButton {
        id: menuRow
        property string glyph: ""
        property bool quietRow: false
        Layout.fillWidth: true
        implicitHeight: 44
        focusPolicy: Qt.NoFocus
        Accessible.name: text
        background: Rectangle {
            radius: 8
            color: Qt.alpha(Kirigami.Theme.textColor, menuRow.down ? 0.16 : menuRow.hovered ? 0.12 : 0)
        }
        contentItem: RowLayout {
            spacing: 12
            Item { implicitWidth: 0 }
            Kirigami.Icon {
                source: Qt.resolvedUrl("icons/" + menuRow.glyph)
                implicitWidth: 16
                implicitHeight: 16
                color: label.color
                isMask: true
            }
            QQC2.Label {
                id: label
                Layout.fillWidth: true
                text: menuRow.text
                font.pixelSize: 14
                font.weight: Font.Medium
                color: menuRow.quietRow ? Qt.alpha(Kirigami.Theme.textColor, 0.68) : Kirigami.Theme.textColor
            }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: column.y + column.height + 12
        // Still while a finger selects words on the page.
        interactive: contentHeight > height && !touchSelect.selecting
        boundsBehavior: Flickable.StopAtBounds
        // A wheel notch scrolls 80 pixels at Plasma's usual three lines per notch.
        Kirigami.WheelHandler { target: flick; verticalStepSize: Qt.styleHints.wheelScrollLines * 80 / 3 }

        ColumnLayout {
            id: column
            x: 12
            y: 12
            width: flick.width - 24
            height: Math.max(flick.height - y - 12, implicitHeight)
            spacing: 8

            Rectangle {
                id: page
                objectName: "page"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 96
                // The page takes whatever room the rest leaves it.
                implicitHeight: 96
                radius: 8
                color: quick.capture.hexFor(quick.capture.colour)
                clip: true

                ChecklistPad {
                    id: checklistPad
                    objectName: "checklistPad"
                    anchors.fill: parent
                    anchors.rightMargin: 44
                    visible: quick.capture.checklist
                    capture: quick.capture
                }

                QQC2.ScrollView {
                    id: pageScroll
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
                        font.pixelSize: 21
                        font.weight: Font.Medium
                        leftPadding: 24
                        // Clear of the colour's dot.
                        rightPadding: 60
                        topPadding: 22
                        bottomPadding: 22
                        background: null
                        // Selecting with a mouse or a touchpad stays on in
                        // tablet mode, where the desktop's style turns it off
                        // once the screen has been touched.
                        selectByMouse: true
                        Accessible.name: i18n("Note")

                        TouchSelect {
                            id: touchSelect
                            anchors.fill: parent
                            target: area
                        }
                    }
                }

                Binding {
                    target: pageScroll.contentItem
                    property: "interactive"
                    value: false
                    when: touchSelect.selecting
                }

                // The note's colour, a dot in the page's corner; a tap lays
                // the five colours out beside it.
                QQC2.AbstractButton {
                    id: colourDot
                    objectName: "colourDot"
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 6
                    implicitWidth: 44
                    implicitHeight: 44
                    focusPolicy: Qt.NoFocus
                    enabled: !quick.capture.readOnly
                    Accessible.name: i18n("Colour")
                    onClicked: {
                        quick.choosingColour = !quick.choosingColour;
                        quick.showingMore = false;
                    }
                    contentItem: Item {
                        Rectangle {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            radius: 10
                            color: page.color
                            border.width: 2
                            border.color: Qt.rgba(0.1, 0.1, 0.1, 0.55)
                        }
                    }
                }

                Rectangle {
                    id: colours
                    objectName: "colours"
                    visible: quick.choosingColour
                    anchors.right: colourDot.left
                    anchors.verticalCenter: colourDot.verticalCenter
                    width: swatches.implicitWidth + 12
                    height: 48
                    radius: 24
                    color: Qt.rgba(0.1, 0.1, 0.1, 0.85)

                    Row {
                        id: swatches
                        anchors.centerIn: parent
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
                                Accessible.name: modelData.name
                                Accessible.role: Accessible.RadioButton
                                Accessible.checked: chosen
                                onClicked: {
                                    quick.capture.colour = modelData.name;
                                    quick.focusText();
                                }
                                contentItem: Item {
                                    Rectangle {
                                        anchors.centerIn: parent
                                        visible: swatch.chosen
                                        width: 32
                                        height: 32
                                        radius: 16
                                        color: "#F8F8FF"
                                    }
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 26
                                        height: 26
                                        radius: 13
                                        color: swatch.modelData.hex
                                        border.width: swatch.chosen ? 2 : 0
                                        border.color: Qt.rgba(0.1, 0.1, 0.1, 0.85)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // What Remind offers, opened from its glyph.
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

            // One line under the page: where the note is kept and what it is
            // stuck to, already filled in, then the note's own controls.
            RowLayout {
                id: tray
                objectName: "tray"
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                spacing: 0

                IconButton {
                    objectName: "folderChip"
                    glyph: "folder"
                    showText: true
                    horizontalPadding: 12
                    text: quick.capture.folderLabel
                    ink: quick.quiet
                    checked: quick.choosingFolder
                    enabled: !quick.capture.readOnly
                    Layout.minimumWidth: 44
                    Layout.maximumWidth: tray.width * 0.32
                    Accessible.name: i18n("Folder: %1", quick.capture.folderLabel)
                    onClicked: quick.chooseFolder()
                }
                IconButton {
                    objectName: "stuckChip"
                    glyph: "pin"
                    showText: true
                    horizontalPadding: 12
                    text: quick.capture.stuck ? quick.capture.window : i18n("Not stuck")
                    ink: quick.quiet
                    checked: quick.choosingWindow
                    enabled: !quick.capture.readOnly
                    Layout.minimumWidth: 44
                    Layout.maximumWidth: tray.width * 0.32
                    Accessible.name: quick.capture.stuck ? i18n("Stuck to %1", quick.capture.window) : i18n("Not stuck to a window")
                    onClicked: quick.chooseWindow()
                }

                // Kept as it is written: a quiet tick, or why it is not.
                RowLayout {
                    objectName: "status"
                    readonly property bool trouble: quick.capture.problem.length > 0 || quick.capture.readOnly
                    Layout.leftMargin: 6
                    Layout.maximumWidth: tray.width * 0.3
                    spacing: 6
                    Kirigami.Icon {
                        implicitWidth: 16
                        implicitHeight: 16
                        source: Qt.resolvedUrl(parent.trouble ? "icons/lucide/triangle-alert.svg" : "icons/lucide/check.svg")
                        color: parent.trouble ? Kirigami.Theme.negativeTextColor : Qt.alpha(Kirigami.Theme.textColor, 0.45)
                        isMask: true
                        Accessible.name: parent.trouble ? statusWords.text : i18n("Saved as you go")
                    }
                    QQC2.Label {
                        id: statusWords
                        visible: parent.trouble
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Kirigami.Theme.negativeTextColor
                        text: quick.capture.problem.length > 0 ? i18n("Not kept yet: %1", quick.capture.problem)
                            : i18n("Kept by a newer %1: read only", Qt.application.displayName)
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // Words selected on the page, to copy with a tap as well as
                // with Ctrl+C or the right-click menu.
                IconButton {
                    objectName: "copy"
                    visible: area.selectedText.length > 0 && !quick.capture.checklist
                    glyph: "copy"
                    text: i18n("Copy")
                    onClicked: area.copy()
                }
                IconButton {
                    objectName: "checklist"
                    glyph: "list-checks"
                    text: i18n("Checklist")
                    checked: quick.capture.checklist
                    enabled: !quick.capture.readOnly
                    onClicked: quick.toggleChecklist()
                }
                IconButton {
                    objectName: "remind"
                    glyph: "bell"
                    showText: quick.capture.hasReminder
                    text: quick.capture.hasReminder && quick.words
                          ? quick.words.reminderLabel(quick.capture.remindAt, quick.capture.remindOnOpen)
                          : i18n("Remind")
                    checked: quick.capture.hasReminder || quick.choosingReminder
                    enabled: !quick.capture.readOnly
                    onClicked: quick.chooseReminder()
                }
                IconButton {
                    objectName: "noReminder"
                    visible: quick.capture.hasReminder
                    glyph: "x"
                    text: i18n("No reminder")
                    onClicked: {
                        quick.capture.clearReminder();
                        quick.choosingReminder = false;
                    }
                }
                IconButton {
                    id: more
                    objectName: "more"
                    visible: quick.capture.kept && !quick.capture.readOnly
                    glyph: "ellipsis"
                    text: i18n("More")
                    checked: quick.showingMore
                    onClicked: {
                        quick.choosingColour = false;
                        if (!quick.showingMore) {
                            const at = more.mapToItem(quick, more.width, 0);
                            moreMenu.x = Math.max(12, Math.min(quick.width - moreMenu.width - 12, at.x - moreMenu.width));
                            moreMenu.reachesTo = at.y - 6;
                        }
                        quick.showingMore = !quick.showingMore;
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 20
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    color: Qt.alpha(Kirigami.Theme.textColor, 0.2)
                }
                IconButton {
                    objectName: "allNotes"
                    glyph: "layout-grid"
                    text: i18n("All notes")
                    onClicked: quick.boardRequested()
                }
            }
        }
    }

    // What is rarely needed, over the note above More, placed as it opens.
    Rectangle {
        id: moreMenu
        objectName: "moreMenu"
        visible: quick.showingMore
        z: 2
        // Where its lower edge stands, just above More.
        property real reachesTo: 0
        width: 240
        height: moreRows.implicitHeight + 12
        y: Math.max(12, reachesTo - height)
        radius: 12
        color: quick.surface
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)

        ColumnLayout {
            id: moreRows
            anchors.fill: parent
            anchors.margins: 6
            spacing: 2

            MenuRow {
                objectName: "copyAll"
                glyph: "lucide/copy.svg"
                text: i18n("Copy all")
                onClicked: {
                    area.selectAll();
                    area.copy();
                    area.deselect();
                    quick.focusText();
                }
            }
            MenuRow {
                objectName: "pin"
                visible: !quick.capture.readOnly && quick.capture.text.trim().length > 0
                glyph: "lucide/pin.svg"
                text: quick.capture.pinned
                    ? (quick.capture.insideShuffle ? i18n("Unpin from the Actions Bar") : i18n("Unpin from the panel"))
                    : (quick.capture.insideShuffle ? i18n("Pin to the Actions Bar") : i18n("Pin to the panel"))
                onClicked: {
                    quick.showingMore = false;
                    quick.capture.setPinned(!quick.capture.pinned);
                    quick.focusText();
                }
            }
            MenuRow {
                objectName: "tuckAway"
                glyph: "tuck.svg"
                text: i18n("Tuck away")
                onClicked: {
                    quick.showingMore = false;
                    quick.capture.tuckAway();
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.topMargin: 3
                Layout.bottomMargin: 3
                color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
            }
            MenuRow {
                objectName: "remove"
                glyph: "lucide/trash-2.svg"
                quietRow: true
                text: i18n("Remove")
                onClicked: {
                    quick.showingMore = false;
                    quick.capture.remove();
                }
            }
        }
    }
}
