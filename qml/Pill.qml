// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A round touch button: a choice under Belongs to, or an action such as Done.
// Chosen, it turns over to the card's own colours.
QQC2.AbstractButton {
    id: pill

    property var iconName: ""
    property bool tall: false
    // Drawn 30 pixels tall, as a header's pill, inside the full touch height.
    property bool compact: false
    property bool primary: false
    // Filled in the person's highlight colour: the board's floating New note.
    property bool accent: false
    readonly property bool filled: checked || primary || accent

    implicitHeight: tall ? 52 : 44
    implicitWidth: Math.max(implicitHeight, row.implicitWidth + 2 * horizontalPadding)
    horizontalPadding: tall ? 24 : compact ? 12 : 16
    focusPolicy: Qt.NoFocus
    Accessible.name: text

    // The accent lights under the pointer, with a soft ring of its colour
    // 4 out, as the sheet's rhythm steps.
    hoverEnabled: true

    background: Item {
        Rectangle {
            objectName: "ring"
            visible: pill.accent
            anchors.fill: face
            anchors.margins: -4
            radius: height / 2
            color: "transparent"
            border.width: 4
            border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.35)
            opacity: pill.hovered && !pill.down ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 90 } }
        }
        Rectangle {
            id: face
            objectName: "face"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: pill.compact ? 30 : parent.height
            radius: height / 2
            color: pill.accent ? (pill.down ? Qt.darker(Kirigami.Theme.highlightColor, 1.15)
                                : pill.hovered ? Qt.lighter(Kirigami.Theme.highlightColor, 1.15)
                                : Kirigami.Theme.highlightColor)
                 : pill.filled ? Kirigami.Theme.textColor
                 : Qt.alpha(Kirigami.Theme.textColor, pill.down ? 0.16 : 0.07)
            border.width: pill.filled ? 0 : 1
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.35)
        }
    }

    contentItem: Item {
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Kirigami.Icon {
                visible: String(pill.iconName).length > 0
                source: pill.iconName
                implicitWidth: 16
                implicitHeight: 16
                color: label.color
                isMask: true
            }
            QQC2.Label {
                id: label
                text: pill.text
                font.pixelSize: pill.tall ? 15 : pill.compact ? 13 : 14
                font.weight: pill.primary || pill.accent ? Font.Bold : Font.DemiBold
                color: pill.accent ? Kirigami.Theme.highlightedTextColor
                     : pill.filled ? Kirigami.Theme.backgroundColor : Kirigami.Theme.textColor
                elide: Text.ElideMiddle
                Layout.maximumWidth: 420
            }
        }
    }
}
