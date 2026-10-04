// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The quick note, inside its card: the page with the cursor in it, the
// note's colour, where it belongs, and Done. What is written is kept from its
// first letter. The page takes the room the card has; when the keys shorten
// the card past what all of it needs, the rest scrolls.
Item {
    id: quick

    required property QtObject capture
    // Project names, the most recently used first.
    property var projects: []
    // The application's name, as its desktop file gives it.
    property string title: Qt.application.displayName

    signal boardRequested()

    readonly property alias editor: area
    readonly property string shownProject: capture.belongs === "project" ? capture.project : (projects.length > 0 ? projects[0] : "")
    readonly property var otherProjects: projects.filter(name => name !== shownProject)
    property bool choosingProject: false
    // Words for reminders, and what Remind offers: the shell, where there
    // is one (Shell::reminderLabel, Shell::reminderChoices).
    property QtObject words: null
    property bool choosingReminder: false
    property bool pickingTime: false
    property var reminderChoices: []
    // Pen: adding ink under the words, rather than typing them. The ink the
    // pen writes in, or the eraser instead.
    property bool writing: false
    property string ink: "black"
    property bool erasing: false
    // Typing the right words for what the handwriting was read as.
    property bool fixing: false

    // A note of ink alone opens in Pen; any other in Type.
    function startMode() {
        fixing = false;
        erasing = false;
        writing = capture.hasInk && capture.text.length === 0;
    }

    function penMode() {
        if (capture.readOnly) {
            return;
        }
        writing = true;
        Qt.inputMethod.hide();
    }

    function typeMode() {
        writing = false;
        erasing = false;
        focusText();
    }

    function startFixing() {
        fixField.text = capture.readText;
        fixing = true;
        fixField.forceActiveFocus();
        fixField.selectAll();
        Qt.inputMethod.show();
    }

    function finishFixing() {
        if (!fixing) {
            return;
        }
        fixing = false;
        if (fixField.text.trim().length > 0 && fixField.text.trim() !== capture.readText) {
            capture.fixReading(fixField.text);
        }
    }

    function focusText() {
        choosingProject = false;
        if (writing) {
            return;
        }
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

    function chooseProject(name) {
        capture.setBelongs("project", name);
        projectName.text = "";
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

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // The typed words, above the ink.
                    Item {
                        id: typed
                        objectName: "typed"
                        readonly property real wanted: quick.capture.checklist ? checklistPad.contentHeight + 24 : area.implicitHeight
                        Layout.fillWidth: true
                        Layout.fillHeight: !quick.writing
                        Layout.preferredHeight: quick.writing ? Math.min(wanted, page.height * 0.3) : -1
                        visible: !quick.writing || quick.capture.text.length > 0

                        ChecklistPad {
                            id: checklistPad
                            objectName: "checklistPad"
                            anchors.fill: parent
                            visible: quick.capture.checklist
                            enabled: !quick.writing
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
                                readOnly: quick.capture.readOnly || quick.writing
                                placeholderText: quick.capture.hasInk ? "" : i18n("Write it down…")
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

                        // In Pen, a finger on the words goes back to typing.
                        TapHandler {
                            enabled: quick.writing
                            acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Mouse
                            onTapped: quick.typeMode()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Qt.rgba(0, 0, 0, 0.12)
                        visible: typed.visible && inkPage.visible
                    }

                    // The ink, under the words.
                    InkPage {
                        id: inkPage
                        objectName: "inkPage"
                        Layout.fillWidth: true
                        Layout.fillHeight: quick.writing
                        Layout.preferredHeight: quick.writing ? -1 : Math.min(canvas.pageHeight, page.height * 0.45)
                        visible: quick.writing || quick.capture.hasInk
                        capture: quick.capture
                        writing: quick.writing
                        ink: quick.ink
                        erasing: quick.erasing
                        bottomRoom: inkFoot.visible ? inkFoot.height + 8 : 0
                        onPenArrived: quick.penMode()
                        onErased: count => {
                            if (count > 0) {
                                notice.show(i18np("Erased", "Erased %1 strokes", count), i18n("Undo"), () => quick.capture.undoErase());
                            }
                        }
                    }
                }

                // Under the ink: what was read from it, to fix, and in Pen the
                // palette.
                Item {
                    id: inkFoot
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    height: 44
                    visible: inkPage.visible && (quick.writing || quick.capture.readText.length > 0)

                    QQC2.AbstractButton {
                        id: reading
                        objectName: "readAs"
                        anchors.left: parent.left
                        anchors.right: palette.visible ? palette.left : parent.right
                        anchors.rightMargin: 8
                        height: 44
                        visible: quick.capture.readText.length > 0 && !quick.fixing
                        enabled: !quick.capture.readOnly
                        focusPolicy: Qt.NoFocus
                        Accessible.name: i18n("Read as “%1”. Fix", quick.capture.readText)
                        onClicked: quick.startFixing()
                        contentItem: QQC2.Label {
                            leftPadding: 14
                            verticalAlignment: Text.AlignVCenter
                            text: i18n("Read as “%1”", quick.capture.readText)
                            elide: Text.ElideRight
                            color: Qt.rgba(0.1, 0.1, 0.1, 0.6)
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }

                    QQC2.TextField {
                        id: fixField
                        objectName: "fixReading"
                        anchors.left: parent.left
                        anchors.right: palette.visible ? palette.left : parent.right
                        anchors.rightMargin: 8
                        height: 44
                        visible: quick.fixing
                        leftPadding: 14
                        rightPadding: 14
                        color: "#1A1A1A"
                        placeholderText: i18n("What it says")
                        Accessible.name: i18n("What the handwriting says")
                        onAccepted: quick.finishFixing()
                        onActiveFocusChanged: {
                            if (!activeFocus && quick.fixing) {
                                quick.finishFixing();
                            }
                        }
                        background: Rectangle {
                            radius: 22
                            color: Qt.rgba(1, 1, 1, 0.55)
                            border.width: 1
                            border.color: Qt.rgba(0, 0, 0, 0.25)
                        }
                    }

                    Row {
                        id: palette
                        objectName: "palette"
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: quick.writing
                        spacing: 0

                        Repeater {
                            model: quick.capture.inks
                            delegate: QQC2.AbstractButton {
                                id: inkButton
                                required property string modelData
                                readonly property bool chosen: !quick.erasing && quick.ink === modelData
                                objectName: "ink-" + modelData
                                width: 44
                                height: 44
                                focusPolicy: Qt.NoFocus
                                Accessible.name: modelData
                                Accessible.role: Accessible.RadioButton
                                Accessible.checked: chosen
                                onClicked: {
                                    quick.ink = modelData;
                                    quick.erasing = false;
                                }
                                contentItem: Item {
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 30
                                        height: 30
                                        radius: 15
                                        color: quick.capture.inkHexFor(inkButton.modelData)
                                        border.width: inkButton.chosen ? 3 : 0
                                        border.color: "#F8F8FF"
                                    }
                                    Rectangle {
                                        anchors.centerIn: parent
                                        visible: inkButton.chosen
                                        width: 34
                                        height: 34
                                        radius: 17
                                        color: "transparent"
                                        border.width: 1
                                        border.color: "#1A1A1A"
                                    }
                                }
                            }
                        }

                        QQC2.AbstractButton {
                            id: eraserButton
                            objectName: "eraser"
                            width: 44
                            height: 44
                            focusPolicy: Qt.NoFocus
                            Accessible.name: i18n("Eraser")
                            Accessible.role: Accessible.CheckBox
                            Accessible.checked: quick.erasing
                            onClicked: quick.erasing = !quick.erasing
                            contentItem: Item {
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 34
                                    height: 34
                                    radius: 17
                                    color: quick.erasing ? "#1A1A1A" : Qt.rgba(0, 0, 0, 0.08)
                                    Kirigami.Icon {
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        source: Qt.resolvedUrl("icons/eraser.svg")
                                        color: quick.erasing ? "#F8F8FF" : "#1A1A1A"
                                        isMask: true
                                    }
                                }
                            }
                        }
                    }
                }

                Notice {
                    id: notice
                    objectName: "inkNotice"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    z: 2
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
                        objectName: "type"
                        text: i18n("Type")
                        iconName: "insert-text"
                        checked: !quick.writing
                        onClicked: quick.typeMode()
                    }
                    Pill {
                        objectName: "pen"
                        text: i18n("Pen")
                        iconName: "draw-freehand"
                        checked: quick.writing
                        onClicked: quick.penMode()
                    }
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                QQC2.Label {
                    text: i18n("Belongs to").toUpperCase()
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    font.letterSpacing: 0.5
                    color: Kirigami.Theme.disabledTextColor
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    enabled: !quick.capture.readOnly

                    Pill {
                        visible: quick.capture.window.length > 0
                        text: i18n("This window · %1", quick.capture.window)
                        checked: quick.capture.belongs === "window"
                        objectName: "belongs-window"
                        onClicked: quick.capture.setBelongs("window")
                    }
                    Pill {
                        visible: quick.shownProject.length > 0
                        text: i18n("Project · %1", quick.shownProject)
                        checked: quick.capture.belongs === "project"
                        objectName: "belongs-project"
                        onClicked: quick.capture.setBelongs("project", quick.shownProject)
                    }
                    Pill {
                        text: quick.capture.workspace.length > 0 ? i18n("Workspace · %1", quick.capture.workspace) : i18n("Workspace")
                        checked: quick.capture.belongs === "workspace"
                        objectName: "belongs-workspace"
                        onClicked: quick.capture.setBelongs("workspace")
                    }
                    Pill {
                        text: i18n("Loose")
                        checked: quick.capture.belongs === "loose"
                        objectName: "belongs-loose"
                        onClicked: quick.capture.setBelongs("loose")
                    }
                    Pill {
                        objectName: "chooseProject"
                        text: quick.otherProjects.length > 0 ? i18n("Other project") : i18n("New project")
                        iconName: quick.choosingProject ? "go-up" : "list-add"
                        checked: quick.choosingProject
                        onClicked: {
                            quick.choosingProject = !quick.choosingProject;
                            if (quick.choosingProject && quick.otherProjects.length === 0) {
                                projectName.forceActiveFocus();
                            }
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    visible: quick.choosingProject
                    spacing: 8

                    Repeater {
                        model: quick.otherProjects
                        delegate: Pill {
                            required property string modelData
                            text: modelData
                            onClicked: quick.chooseProject(modelData)
                        }
                    }

                    QQC2.TextField {
                        id: projectName
                        objectName: "projectName"
                        implicitHeight: 44
                        width: 260
                        placeholderText: i18n("Name a new project")
                        Accessible.name: i18n("New project")
                        onAccepted: {
                            if (text.trim().length > 0) {
                                quick.chooseProject(text);
                            }
                        }
                        background: Rectangle {
                            radius: 22
                            color: Qt.alpha(Kirigami.Theme.textColor, 0.07)
                            border.width: 1
                            border.color: projectName.activeFocus ? Kirigami.Theme.focusColor : Qt.alpha(Kirigami.Theme.textColor, 0.35)
                        }
                        leftPadding: 16
                        rightPadding: 16
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
                        : quick.capture.readOnly ? i18n("Kept by a newer Gooseberry: read only")
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
