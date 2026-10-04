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

    function focusText() {
        choosingProject = false;
        area.forceActiveFocus();
        area.cursorPosition = area.length;
        Qt.inputMethod.show();
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
                implicitHeight: 168
                radius: 14
                color: quick.capture.hexFor(quick.capture.colour)
                clip: true

                QQC2.ScrollView {
                    anchors.fill: parent
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
