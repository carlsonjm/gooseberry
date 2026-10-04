// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A short message along the bottom of the board, with one action to undo what
// was just done. It goes by itself after a few seconds.
Rectangle {
    id: notice

    property string actionText
    property var action: null

    function show(message, actionText, action) {
        label.text = message;
        notice.actionText = actionText || "";
        notice.action = action || null;
        visible = true;
        timer.restart();
    }

    visible: false
    Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    radius: height / 2
    implicitHeight: 56
    implicitWidth: row.implicitWidth + 2 * 18

    Timer {
        id: timer
        interval: 5000
        onTriggered: notice.visible = false
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 6
        spacing: 12

        QQC2.Label {
            id: label
            Layout.fillWidth: true
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: Kirigami.Theme.textColor
        }
        Pill {
            objectName: "noticeAction"
            visible: notice.actionText.length > 0
            text: notice.actionText
            onClicked: {
                notice.visible = false;
                if (notice.action) {
                    notice.action();
                }
            }
        }
    }
}
