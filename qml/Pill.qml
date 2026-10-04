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
    property bool primary: false
    readonly property bool filled: checked || primary

    implicitHeight: tall ? 52 : 44
    implicitWidth: Math.max(implicitHeight, row.implicitWidth + 2 * horizontalPadding)
    horizontalPadding: tall ? 24 : 16
    focusPolicy: Qt.NoFocus
    Accessible.name: text

    background: Rectangle {
        radius: height / 2
        color: pill.filled ? Kirigami.Theme.textColor
                           : Qt.alpha(Kirigami.Theme.textColor, pill.down ? 0.16 : 0.07)
        border.width: pill.filled ? 0 : 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.35)
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
                font.pixelSize: pill.tall ? 15 : 14
                font.weight: pill.primary ? Font.Bold : Font.DemiBold
                color: pill.filled ? Kirigami.Theme.backgroundColor : Kirigami.Theme.textColor
                elide: Text.ElideMiddle
                Layout.maximumWidth: 420
            }
        }
    }
}
