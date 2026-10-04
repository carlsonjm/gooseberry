// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The planner's days, two back to three ahead. The chosen day turns over to
// the board's own colours; a day with notes planned carries a dot.
Row {
    id: strip

    required property QtObject planner
    // The mock-up's mark for what is planned.
    readonly property color mark: "#F2A65A"

    spacing: 6

    Repeater {
        model: strip.planner.strip
        delegate: QQC2.AbstractButton {
            id: day
            required property var modelData
            readonly property bool chosen: modelData.chosen
            objectName: "day-" + Qt.formatDate(modelData.date, "yyyy-MM-dd")
            width: 64
            height: 64
            focusPolicy: Qt.NoFocus
            Accessible.name: Qt.locale().toString(modelData.date, "dddd d MMMM")
            Accessible.role: Accessible.RadioButton
            Accessible.checked: chosen
            onClicked: strip.planner.day = modelData.date
            background: Rectangle {
                radius: 14
                color: day.chosen ? Kirigami.Theme.textColor
                                  : Qt.alpha(Kirigami.Theme.textColor, day.down ? 0.14 : 0.06)
            }
            contentItem: Item {
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    QQC2.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Qt.locale().toString(day.modelData.date, "ddd").toUpperCase()
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        color: day.chosen ? Kirigami.Theme.backgroundColor : Kirigami.Theme.textColor
                    }
                    QQC2.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: day.modelData.date.getDate()
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: day.chosen ? Kirigami.Theme.backgroundColor : Kirigami.Theme.textColor
                    }
                    Rectangle {
                        objectName: "planned"
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 5
                        height: 5
                        radius: 3
                        color: strip.mark
                        visible: day.modelData.planned
                    }
                }
            }
        }
    }
}
