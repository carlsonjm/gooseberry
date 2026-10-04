// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One note on the board, in its colour, with where it belongs underneath.
Rectangle {
    id: card

    required property string noteId
    required property string text
    required property string colourHex
    required property string placeLabel
    required property string placeKey
    required property bool tucked
    required property bool checklist
    required property string checklistHeading
    required property var checklistItems
    // The handwriting, as its drawing; what was read from it.
    required property string drawing
    required property string reading
    required property string readingAlso
    // The board's search, to show why a note of handwriting was found.
    property string searchText: ""
    readonly property bool foundInInk: {
        const needle = searchText.trim().toLowerCase();
        return needle.length > 0 && drawing.length > 0 && !text.toLowerCase().includes(needle)
            && (reading.toLowerCase().includes(needle) || readingAlso.toLowerCase().includes(needle));
    }

    function marked(words, needle) {
        const safe = words.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        const at = safe.toLowerCase().indexOf(needle.toLowerCase());
        if (needle.length === 0 || at < 0) {
            return safe;
        }
        return safe.slice(0, at) + "<b><u>" + safe.slice(at, at + needle.length) + "</u></b>" + safe.slice(at + needle.length);
    }
    // The place shown under the note, when the board is not already there.
    property bool showPlace: true

    signal tapped()
    signal tuckRequested()

    readonly property color ink: "#1A1A1A"

    objectName: "note-" + noteId
    color: colourHex
    radius: 12
    implicitHeight: content.implicitHeight + 28
    Accessible.role: Accessible.Button
    Accessible.name: text.length > 0 ? text : reading

    TapHandler {
        onTapped: card.tapped()
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
            visible: !card.checklist && card.text.trim().length > 0
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

        // The handwriting, as it was written.
        Image {
            objectName: "drawing"
            Layout.fillWidth: true
            Layout.preferredHeight: implicitWidth > 0 ? width * implicitHeight / implicitWidth : 0
            Layout.maximumHeight: 200
            visible: card.drawing.length > 0
            source: card.drawing
            sourceSize.width: Math.max(1, Math.round(width * 2))
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignLeft
            verticalAlignment: Image.AlignTop
            asynchronous: true
            cache: false
            Accessible.name: card.reading.length > 0 ? card.reading : i18n("Handwriting")
        }

        // Found in the handwriting: what it was read as, the word marked.
        QQC2.Label {
            objectName: "readAs"
            Layout.fillWidth: true
            visible: card.foundInInk
            text: {
                const needle = card.searchText.trim();
                if (card.reading.toLowerCase().includes(needle.toLowerCase())) {
                    return i18n("Read as “%1”", card.marked(card.reading, needle));
                }
                // Found by a runner-up word: the reading, and the word.
                const word = card.readingAlso.split(" ").find(w => w.toLowerCase().includes(needle.toLowerCase())) || needle;
                return card.reading.length > 0 ? i18n("Read as “%1”, or “%2”", card.marked(card.reading, ""), card.marked(word, needle))
                                                : i18n("Read as “%1”", card.marked(word, needle));
            }
            textFormat: Text.StyledText
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
            color: Qt.rgba(0.1, 0.1, 0.1, 0.7)
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            visible: card.showPlace || !card.tucked || card.drawing.length > 0
            spacing: 6

            Rectangle {
                objectName: "handwritten"
                visible: card.drawing.length > 0
                implicitHeight: 24
                implicitWidth: handwrittenText.implicitWidth
                radius: 12
                color: Qt.rgba(0, 0, 0, 0.09)
                QQC2.Label {
                    id: handwrittenText
                    anchors.fill: parent
                    leftPadding: 9
                    rightPadding: 9
                    verticalAlignment: Text.AlignVCenter
                    text: i18n("Handwritten")
                    color: card.ink
                    font.pixelSize: 11
                    font.weight: Font.Bold
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
