// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

// The note as a checklist, on its page: each item with a box to tick, ticked
// ones struck through where they stand, and the heading above them. Return
// starts the next item; backspace in an empty one takes it away.
QQC2.ScrollView {
    id: pad

    required property QtObject capture
    readonly property color ink: "#1A1A1A"
    readonly property var lines: capture.lines
    onLinesChanged: sync()
    Component.onCompleted: sync()

    // The lines kept row by row, only added or taken at the end, so the row
    // being written in stays as it is while the list changes round it.
    function sync() {
        while (rows.count > lines.length) {
            rows.remove(rows.count - 1);
        }
        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i];
            if (i >= rows.count) {
                rows.append({lineText: line.text, item: line.item, checked: line.checked});
                continue;
            }
            const row = rows.get(i);
            if (row.lineText !== line.text) rows.setProperty(i, "lineText", line.text);
            if (row.item !== line.item) rows.setProperty(i, "item", line.item);
            if (row.checked !== line.checked) rows.setProperty(i, "checked", line.checked);
        }
    }

    // Return: the next item, with the cursor in it.
    function nextItem(line) {
        capture.addItemAfter(line);
        focusLine(line + 1, 0);
    }

    // Backspace in an empty line: it goes, and the cursor goes back a line.
    function dropLine(line) {
        if (lines.length < 2) {
            return;
        }
        capture.removeLine(line);
        focusLine(Math.max(0, line - 1), -1);
    }

    // Into the last line, at its end.
    function focusLast() {
        focusLine(lines.length - 1, -1);
    }

    function focusLine(line, at) {
        const row = repeater.itemAt(line);
        if (row) {
            row.focusAt(at);
        }
    }

    // The line being written stays in sight as the list grows past the page.
    function reveal(item) {
        const flick = pad.contentItem;
        const top = item.mapToItem(flick.contentItem, 0, 0).y;
        if (top < flick.contentY) {
            flick.contentY = Math.max(0, top - 6);
        } else if (top + item.height > flick.contentY + flick.height) {
            flick.contentY = Math.min(flick.contentHeight - flick.height, top + item.height - flick.height + 6);
        }
    }

    contentWidth: availableWidth

    Column {
        width: pad.availableWidth
        topPadding: 12
        bottomPadding: 12

        Repeater {
            id: repeater
            model: ListModel {
                id: rows
            }
            delegate: Item {
                id: row
                required property int index
                required property string lineText
                required property bool item
                required property bool checked
                readonly property var line: ({text: lineText, item: item, checked: checked})
                objectName: "line-" + index
                width: pad.availableWidth
                height: Math.max(44, input.contentHeight + 14)

                function focusAt(at) {
                    input.forceActiveFocus();
                    input.cursorPosition = at < 0 ? input.length : at;
                }

                QQC2.AbstractButton {
                    id: box
                    objectName: "tick"
                    visible: row.line.item
                    x: 8
                    width: 44
                    height: 44
                    focusPolicy: Qt.NoFocus
                    enabled: !pad.capture.readOnly
                    Accessible.role: Accessible.CheckBox
                    Accessible.checked: row.line.checked
                    Accessible.name: row.line.text
                    onClicked: pad.capture.setLineChecked(row.index, !row.line.checked)
                    contentItem: Item {
                        Rectangle {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            radius: 6
                            color: row.line.checked ? pad.ink : "transparent"
                            border.width: row.line.checked ? 0 : 2
                            border.color: Qt.alpha(pad.ink, box.down ? 0.9 : 0.6)
                            QQC2.Label {
                                anchors.centerIn: parent
                                visible: row.line.checked
                                text: "✓"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                color: "#F8F8FF"
                            }
                        }
                    }
                }

                TextEdit {
                    id: input
                    objectName: "lineText"
                    x: row.line.item ? 56 : 20
                    y: 7
                    width: parent.width - x - 20
                    text: row.line.text
                    readOnly: pad.capture.readOnly
                    color: row.line.checked ? Qt.alpha(pad.ink, 0.5) : pad.ink
                    selectionColor: Qt.rgba(0, 0, 0, 0.2)
                    selectedTextColor: pad.ink
                    wrapMode: TextEdit.Wrap
                    font.pixelSize: 22
                    font.weight: row.line.item ? Font.Medium : Font.Bold
                    font.strikeout: row.line.checked
                    Accessible.name: row.line.text
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            Qt.callLater(pad.reveal, row);
                        }
                    }
                    onTextChanged: {
                        if (text !== row.line.text && activeFocus) {
                            pad.capture.setLineText(row.index, text);
                        }
                    }
                    Keys.onPressed: event => {
                        // The list changes after the key is answered, so
                        // this row is never taken while it answers.
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            event.accepted = true;
                            Qt.callLater(pad.nextItem, row.index);
                        } else if (event.key === Qt.Key_Backspace && input.text.length === 0 && pad.lines.length > 1) {
                            event.accepted = true;
                            Qt.callLater(pad.dropLine, row.index);
                        }
                    }
                }
            }
        }
    }
}
