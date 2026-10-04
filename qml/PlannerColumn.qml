// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The planner: the chosen day's notes with a time, in order, then the days
// ahead that have any. The next one still to come today stands out; one marked
// done says Done and is struck through. Tapping a row opens its note; the tick
// at its end marks it done, or not.
QQC2.ScrollView {
    id: column

    required property QtObject shell
    readonly property QtObject planner: shell.planner
    readonly property color mark: "#F2A65A"
    // The desktop's short time, to the minute.
    readonly property string timeFormat: Qt.locale().timeFormat(Locale.ShortFormat).replace(/[:.]ss/, "")

    contentWidth: availableWidth

    function heading(day, first) {
        const words = day.today ? i18n("Today")
            : Math.abs(planner.today - day.date) < 7 * 86400000 ? Qt.locale().toString(day.date, "dddd")
            : Qt.locale().toString(day.date, "d MMMM");
        return (first ? i18n("Planner · %1", words) : words).toUpperCase();
    }

    ColumnLayout {
        width: column.availableWidth
        spacing: 10

        Repeater {
            model: column.planner.days
            delegate: ColumnLayout {
                id: dayBlock
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.topMargin: index > 0 ? 6 : 0
                spacing: 10

                QQC2.Label {
                    objectName: "plannerHeading"
                    Layout.bottomMargin: 4
                    text: column.heading(dayBlock.modelData, dayBlock.index === 0)
                    font.pixelSize: 13
                    font.weight: Font.ExtraBold
                    font.letterSpacing: 0.8
                    color: Kirigami.Theme.disabledTextColor
                    Accessible.role: Accessible.Heading
                }

                Repeater {
                    model: dayBlock.modelData.rows
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        objectName: "plan-" + modelData.id
                        Layout.fillWidth: true
                        implicitHeight: Math.max(48, content.implicitHeight + 24)
                        radius: 12
                        color: Qt.alpha(Kirigami.Theme.textColor, modelData.next ? 0.10 : 0.05)
                        border.width: modelData.next ? 1 : 0
                        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.35)
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title

                        TapHandler {
                            onTapped: column.shell.openNote(row.modelData.id)
                        }

                        RowLayout {
                            id: content
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 14
                            anchors.rightMargin: 4
                            anchors.topMargin: 12
                            spacing: 14

                            QQC2.Label {
                                objectName: "planTime"
                                Layout.preferredWidth: 58
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 2
                                text: row.modelData.done ? i18n("Done")
                                    : Qt.locale().toString(row.modelData.time, column.timeFormat)
                                font.pixelSize: 13
                                font.weight: row.modelData.next ? Font.ExtraBold : Font.Bold
                                color: row.modelData.next ? column.mark : Kirigami.Theme.disabledTextColor
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                spacing: 6
                                QQC2.Label {
                                    objectName: "planTitle"
                                    Layout.fillWidth: true
                                    text: row.modelData.title
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    font.strikeout: row.modelData.done
                                    color: row.modelData.done ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                                }
                                Rectangle {
                                    implicitHeight: 24
                                    implicitWidth: Math.min(place.implicitWidth, content.width - 140)
                                    radius: 12
                                    color: Qt.alpha(Kirigami.Theme.textColor, 0.10)
                                    QQC2.Label {
                                        id: place
                                        anchors.fill: parent
                                        leftPadding: 9
                                        rightPadding: 9
                                        verticalAlignment: Text.AlignVCenter
                                        text: row.modelData.place
                                        elide: Text.ElideRight
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                        color: Kirigami.Theme.textColor
                                        opacity: 0.8
                                    }
                                }
                            }

                            QQC2.AbstractButton {
                                id: tick
                                objectName: "planDone"
                                Layout.alignment: Qt.AlignVCenter
                                Layout.topMargin: -12
                                implicitWidth: 44
                                implicitHeight: 44
                                focusPolicy: Qt.NoFocus
                                enabled: !row.modelData.readOnly
                                Accessible.role: Accessible.CheckBox
                                Accessible.checked: row.modelData.done
                                Accessible.name: i18n("Done")
                                onClicked: column.planner.setDone(row.modelData.id, !row.modelData.done)
                                contentItem: Item {
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 26
                                        height: 26
                                        radius: 13
                                        color: row.modelData.done ? Kirigami.Theme.textColor : "transparent"
                                        border.width: row.modelData.done ? 0 : 2
                                        border.color: Qt.alpha(Kirigami.Theme.textColor, tick.down ? 0.8 : 0.45)
                                        QQC2.Label {
                                            anchors.centerIn: parent
                                            visible: row.modelData.done
                                            text: "✓"
                                            font.pixelSize: 16
                                            font.weight: Font.Bold
                                            color: Kirigami.Theme.backgroundColor
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
