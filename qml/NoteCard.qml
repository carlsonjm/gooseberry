// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One note on the board, in its colour, with its folder and the window it is
// stuck to underneath. Held, it lifts, to be carried to a folder, a day or the
// trash.
Rectangle {
    id: card

    required property string noteId
    required property string text
    required property string colourHex
    required property string placeLabel
    required property string placeKey
    required property string stuckTo
    required property bool tucked
    required property bool checklist
    required property string checklistHeading
    required property var checklistItems
    // The place shown under the note, when the board is not already there.
    property bool showPlace: true

    signal tapped()
    signal tuckRequested()
    // Held: the note lifts at that point, follows it, and is let go there,
    // each in the window's own coordinates.
    signal lifted(point scenePosition)
    signal carried(point scenePosition)
    signal letGo(point scenePosition)

    // Lifted and following the finger or the pointer.
    property bool lifting: false

    readonly property color ink: "#1A1A1A"

    objectName: "note-" + noteId
    color: colourHex
    radius: 12
    implicitHeight: content.implicitHeight + 28
    Accessible.role: Accessible.Button
    Accessible.name: text

    TapHandler {
        onTapped: card.tapped()
        onLongPressed: {
            card.lifting = true;
            card.lifted(point.scenePosition);
        }
    }

    // Follows the point without taking it from the board's scrolling until
    // the note is lifted.
    PointHandler {
        id: tracker
        readonly property point at: point.scenePosition
        onAtChanged: {
            if (card.lifting) {
                card.carried(at);
            }
        }
        onActiveChanged: {
            if (!active && card.lifting) {
                card.lifting = false;
                card.letGo(at);
            }
        }
    }

    ColumnLayout {
        id: content
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: 16
            rightMargin: 16
            topMargin: 14
        }
        spacing: 8

        // A checklist reads as its heading, then its items in one line, those
        // ticked struck through.
        QQC2.Label {
            objectName: "checklistHeading"
            Layout.fillWidth: true
            Layout.bottomMargin: -4
            visible: card.checklist && card.checklistHeading.length > 0
            text: card.checklistHeading
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: card.ink
            font.pixelSize: 15
            font.weight: Font.Bold
        }

        QQC2.Label {
            objectName: "checklistItems"
            Layout.fillWidth: true
            visible: card.checklist
            text: card.checklistItems.map(item => {
                const words = item.text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                return item.checked ? "<s>" + words + "</s>" : words;
            }).join(" · ")
            textFormat: Text.StyledText
            wrapMode: Text.Wrap
            maximumLineCount: 8
            elide: Text.ElideRight
            color: card.ink
            font.pixelSize: 15
            font.weight: Font.Medium
            lineHeight: 1.15
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: !card.checklist
            text: card.text.trim()
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            maximumLineCount: 12
            elide: Text.ElideRight
            color: card.ink
            font.pixelSize: 15
            font.weight: Font.Medium
            lineHeight: 1.15
        }

        RowLayout {
            Layout.fillWidth: true
            visible: card.showPlace || !card.tucked || card.stuckTo.length > 0
            spacing: 6

            Rectangle {
                objectName: "stuckTo"
                visible: card.stuckTo.length > 0
                implicitHeight: 24
                implicitWidth: Math.min(stuckRow.implicitWidth + 18, content.width - 50)
                radius: 12
                color: Qt.rgba(0, 0, 0, 0.09)
                Row {
                    id: stuckRow
                    anchors.verticalCenter: parent.verticalCenter
                    x: 9
                    spacing: 4
                    Kirigami.Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 12
                        height: 12
                        source: "pin"
                        color: card.ink
                        isMask: true
                    }
                    QQC2.Label {
                        width: Math.min(implicitWidth, content.width - 90)
                        text: card.stuckTo
                        elide: Text.ElideRight
                        color: card.ink
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }
                }
            }

            Rectangle {
                visible: card.showPlace
                implicitHeight: 24
                implicitWidth: Math.min(placeText.implicitWidth, content.width - 50)
                radius: 12
                color: Qt.rgba(0, 0, 0, 0.09)
                QQC2.Label {
                    id: placeText
                    anchors.fill: parent
                    leftPadding: 9
                    rightPadding: 9
                    verticalAlignment: Text.AlignVCenter
                    text: card.placeLabel
                    elide: Text.ElideRight
                    color: card.ink
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }

            Item {
                Layout.fillWidth: true
            }

            QQC2.AbstractButton {
                objectName: "tuck"
                visible: !card.tucked
                implicitWidth: 44
                implicitHeight: 44
                Layout.topMargin: -8
                Layout.bottomMargin: -4
                Layout.rightMargin: -10
                focusPolicy: Qt.NoFocus
                Accessible.name: i18n("Tuck away")
                QQC2.ToolTip.text: Accessible.name
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: card.tuckRequested()
                contentItem: Item {
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: Qt.resolvedUrl("icons/tuck.svg")
                        color: card.ink
                        isMask: true
                        opacity: parent.parent.down ? 1 : 0.6
                    }
                }
            }
        }
    }
}
