// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A quiet round control under the note: a glyph, and its words beside it
// only where they say something, such as when a reminder is set. It has no
// face at rest; a press, or being on, gives it one.
QQC2.AbstractButton {
    id: button

    // A file in icons/lucide, without its extension.
    property string glyph: ""
    property bool showText: false
    // The text and glyph colour; the context line's are quieter than an
    // action's.
    property color ink: Kirigami.Theme.textColor

    implicitHeight: 44
    implicitWidth: showText ? Math.max(44, row.implicitWidth + 2 * horizontalPadding) : 44
    horizontalPadding: showText ? 14 : 0
    focusPolicy: Qt.NoFocus
    Accessible.name: text

    background: Rectangle {
        objectName: "face"
        radius: height / 2
        color: Qt.alpha(Kirigami.Theme.textColor, button.down ? 0.16 : button.checked ? 0.24 : button.hovered ? 0.12 : 0)
        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    contentItem: Item {
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Kirigami.Icon {
                visible: button.glyph.length > 0
                source: Qt.resolvedUrl("icons/lucide/" + button.glyph + ".svg")
                implicitWidth: 20
                implicitHeight: 20
                color: button.ink
                isMask: true
            }
            QQC2.Label {
                visible: button.showText
                text: button.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: button.ink
                elide: Text.ElideMiddle
                Layout.maximumWidth: 260
            }
        }
    }
}
