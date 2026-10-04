// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The capture sheet: the page with the cursor in it, the note's colour, where
// it belongs, and Done. What is written is kept from its first letter.
Rectangle {
    id: sheet

    required property QtObject capture
    // Project names, the most recently used first.
    property var projects: []

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

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    topLeftRadius: 20
    topRightRadius: 20
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
    implicitWidth: 760
    implicitHeight: column.implicitHeight + 10 + 20

    // Taps on the sheet itself stay on the sheet.
    MouseArea {
        anchors.fill: parent
        onPressed: mouse => mouse.accepted = true
    }

    ColumnLayout {
        id: column
        anchors {
            fill: parent
            leftMargin: 22
            rightMargin: 22
            topMargin: 10
            bottomMargin: 20
        }
        spacing: 14

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 44
            implicitHeight: 5
            radius: 3
            color: Qt.alpha(Kirigami.Theme.textColor, 0.25)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Pill {
                objectName: "allNotes"
                text: i18n("All notes")
                iconName: "view-list-details"
                onClicked: sheet.boardRequested()
            }

            Item {
                Layout.fillWidth: true
            }

            Kirigami.Icon {
                implicitWidth: 16
                implicitHeight: 16
                source: sheet.capture.problem.length > 0 || sheet.capture.readOnly ? "dialog-warning" : "checkmark"
                color: status.color
                isMask: true
            }
            QQC2.Label {
                id: status
                Layout.maximumWidth: column.width * 0.65
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.DemiBold
                text: sheet.capture.problem.length > 0 ? i18n("Not kept yet: %1", sheet.capture.problem)
                    : sheet.capture.readOnly ? i18n("Kept by a newer Gooseberry: read only")
                    : i18n("Saved as you go")
                color: sheet.capture.problem.length > 0 || sheet.capture.readOnly ? Kirigami.Theme.negativeTextColor
                                                                                  : Kirigami.Theme.positiveTextColor
            }
        }

        Rectangle {
            id: page
            Layout.fillWidth: true
            implicitHeight: 168
            radius: 14
            color: sheet.capture.hexFor(sheet.capture.colour)
            clip: true

            QQC2.ScrollView {
                anchors.fill: parent
                QQC2.TextArea {
                    id: area
                    text: sheet.capture.text
                    onTextChanged: {
                        if (text !== sheet.capture.text) {
                            sheet.capture.text = text;
                        }
                    }
                    readOnly: sheet.capture.readOnly
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 2

            Item {
                Layout.fillWidth: true
            }

            Repeater {
                model: sheet.capture.colours
                delegate: QQC2.AbstractButton {
                    id: swatch
                    required property var modelData
                    objectName: "colour-" + modelData.name
                    readonly property bool chosen: sheet.capture.colour === modelData.name
                    implicitWidth: 44
                    implicitHeight: 44
                    focusPolicy: Qt.NoFocus
                    enabled: !sheet.capture.readOnly
                    Accessible.name: modelData.name
                    Accessible.role: Accessible.RadioButton
                    Accessible.checked: chosen
                    onClicked: sheet.capture.colour = modelData.name
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
                            color: sheet.color
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
                enabled: !sheet.capture.readOnly

                Pill {
                    visible: sheet.capture.window.length > 0
                    text: i18n("This window · %1", sheet.capture.window)
                    checked: sheet.capture.belongs === "window"
                    objectName: "belongs-window"
                    onClicked: sheet.capture.setBelongs("window")
                }
                Pill {
                    visible: sheet.shownProject.length > 0
                    text: i18n("Project · %1", sheet.shownProject)
                    checked: sheet.capture.belongs === "project"
                    objectName: "belongs-project"
                    onClicked: sheet.capture.setBelongs("project", sheet.shownProject)
                }
                Pill {
                    text: sheet.capture.workspace.length > 0 ? i18n("Workspace · %1", sheet.capture.workspace) : i18n("Workspace")
                    checked: sheet.capture.belongs === "workspace"
                    objectName: "belongs-workspace"
                    onClicked: sheet.capture.setBelongs("workspace")
                }
                Pill {
                    text: i18n("Loose")
                    checked: sheet.capture.belongs === "loose"
                    objectName: "belongs-loose"
                    onClicked: sheet.capture.setBelongs("loose")
                }
                Pill {
                    objectName: "chooseProject"
                    text: sheet.otherProjects.length > 0 ? i18n("Other project") : i18n("New project")
                    iconName: sheet.choosingProject ? "go-up" : "list-add"
                    checked: sheet.choosingProject
                    onClicked: {
                        sheet.choosingProject = !sheet.choosingProject;
                        if (sheet.choosingProject && sheet.otherProjects.length === 0) {
                            projectName.forceActiveFocus();
                        }
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                visible: sheet.choosingProject
                spacing: 8

                Repeater {
                    model: sheet.otherProjects
                    delegate: Pill {
                        required property string modelData
                        text: modelData
                        onClicked: sheet.chooseProject(modelData)
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
                            sheet.chooseProject(text);
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

            Pill {
                visible: sheet.capture.kept && !sheet.capture.readOnly
                tall: true
                objectName: "tuckAway"
                text: i18n("Tuck away")
                iconName: Qt.resolvedUrl("icons/tuck.svg")
                onClicked: sheet.capture.tuckAway()
            }
            Pill {
                visible: sheet.capture.kept && !sheet.capture.readOnly
                tall: true
                objectName: "remove"
                text: i18n("Remove")
                iconName: "user-trash"
                onClicked: sheet.capture.remove()
            }
            Item {
                Layout.fillWidth: true
            }
            Pill {
                tall: true
                objectName: "done"
                text: i18n("Done")
                onClicked: sheet.capture.finish()
            }
        }
    }
}
