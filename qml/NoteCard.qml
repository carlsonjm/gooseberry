// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One note on the board, as a search lists it: a swatch in its colour, its
// first line, and under it its folder and the window it is stuck to, or a
// checklist's items. On a wide board the row shows more: the note's next
// words under its first line, and its place and when it changed at the end.
// Held, it lifts, to be carried to a folder, a day or the trash.
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
    required property var changed
    // The place shown under the note, when the board is not already there.
    property bool showPlace: true
    // A wide board: the row shows the note's next words, and its place and
    // when it changed at its end.
    property bool wide: false

    signal tapped()
    signal tuckRequested()
    // Held: the note lifts at that point, follows it, and is let go there,
    // each in the window's own coordinates.
    signal lifted(point scenePosition)
    signal carried(point scenePosition)
    signal letGo(point scenePosition)

    // Lifted and following the finger or the pointer.
    property bool lifting: false

    // The first line with words, as the row's title.
    readonly property string title: {
        const lines = text.split("\n").map(line => line.trim()).filter(line => line.length > 0);
        return lines.length > 0 ? lines[0] : "";
    }
    readonly property string detail: [showPlace ? placeLabel : "", stuckTo.length > 0 ? i18n("Stuck to %1", stuckTo) : ""]
        .filter(part => part.length > 0).join(" · ")
    // The words after the first line, as one line.
    readonly property string words: {
        const lines = text.split("\n").map(line => line.trim()).filter(line => line.length > 0);
        return lines.slice(1).join(" ");
    }
    // When the note last changed: the time today, Yesterday, the weekday
    // within a week, the date before that.
    readonly property string when: {
        const at = changed instanceof Date ? changed : new Date(changed);
        if (isNaN(at.getTime())) return "";
        const day = date => new Date(date.getFullYear(), date.getMonth(), date.getDate());
        const days = Math.round((day(new Date()) - day(at)) / 86400000);
        if (days <= 0) return Qt.locale().toString(at, Qt.locale().timeFormat(Locale.ShortFormat));
        if (days === 1) return i18n("Yesterday");
        if (days < 7) return Qt.locale().toString(at, "dddd");
        return Qt.locale().toString(at, "d MMM");
    }

    objectName: "note-" + noteId
    color: rowTap.pressed ? Qt.alpha(Kirigami.Theme.textColor, 0.12)
         : rowHover.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.06) : "transparent"
    radius: 8
    implicitHeight: Math.max(wide ? 72 : 56, content.implicitHeight + 24)
    Accessible.role: Accessible.Button
    Accessible.name: text

    HoverHandler {
        id: rowHover
    }

    TapHandler {
        id: rowTap
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

    RowLayout {
        id: content
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            leftMargin: 12
            rightMargin: 4
        }
        spacing: 12

        Rectangle {
            objectName: "bead"
            implicitWidth: card.wide ? 28 : 24
            implicitHeight: implicitWidth
            radius: 6
            color: card.colourHex
            opacity: card.tucked ? 0.55 : 1
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            // A checklist reads as its heading, then its items in one line,
            // those ticked struck through.
            QQC2.Label {
                objectName: "checklistHeading"
                Layout.fillWidth: true
                visible: card.checklist && card.checklistHeading.length > 0
                text: card.checklistHeading
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 16
                font.weight: Font.Medium
                color: card.tucked ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
            }

            QQC2.Label {
                objectName: "title"
                Layout.fillWidth: true
                visible: !card.checklist
                text: card.title
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 16
                font.weight: Font.Medium
                color: card.tucked ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
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
                elide: Text.ElideRight
                font.pixelSize: 12
                color: Kirigami.Theme.disabledTextColor
            }

            QQC2.Label {
                objectName: "words"
                Layout.fillWidth: true
                visible: card.wide && !card.checklist && card.words.length > 0
                text: card.words
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 13
                color: Qt.alpha(Kirigami.Theme.textColor, 0.66)
            }

            QQC2.Label {
                objectName: "detail"
                Layout.fillWidth: true
                visible: !card.wide && card.detail.length > 0
                text: card.detail
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: 12
                color: Kirigami.Theme.disabledTextColor
            }
        }

        // On a wide board: where the note is and when it changed.
        ColumnLayout {
            objectName: "rowEnd"
            visible: card.wide
            Layout.preferredWidth: 220
            Layout.maximumWidth: 220
            spacing: 2
            QQC2.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                visible: text.length > 0
                text: card.detail
                textFormat: Text.PlainText
                elide: Text.ElideLeft
                font.pixelSize: 12
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.Label {
                objectName: "when"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                visible: text.length > 0
                text: card.when
                textFormat: Text.PlainText
                font.pixelSize: 12
                color: Kirigami.Theme.disabledTextColor
            }
        }

        QQC2.AbstractButton {
            objectName: "tuck"
            // Kept in its place when tucked, so the row's end lines up.
            opacity: card.tucked ? 0 : 1
            enabled: !card.tucked
            implicitWidth: 44
            implicitHeight: 44
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
                    color: Kirigami.Theme.textColor
                    isMask: true
                    opacity: parent.parent.down ? 1 : parent.parent.hovered || rowHover.hovered ? 0.7 : 0.35
                }
            }
        }
    }
}
