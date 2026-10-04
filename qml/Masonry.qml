// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

// Notes in columns, each placed under the shortest column so different
// lengths sit together without gaps.
Item {
    id: masonry

    property alias model: repeater.model
    property alias delegate: repeater.delegate
    property real minimumColumnWidth: 240
    property real spacing: 12
    readonly property int columns: Math.max(1, Math.floor((width + spacing) / (minimumColumnWidth + spacing)))
    readonly property real columnWidth: (width - (columns - 1) * spacing) / columns
    property real contentHeight: 0

    implicitHeight: contentHeight

    function relayout() {
        Qt.callLater(layOut);
    }

    function layOut() {
        const heights = new Array(columns).fill(0);
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i);
            if (!item) {
                continue;
            }
            item.width = columnWidth;
            let shortest = 0;
            for (let c = 1; c < columns; ++c) {
                if (heights[c] < heights[shortest]) {
                    shortest = c;
                }
            }
            item.x = shortest * (columnWidth + spacing);
            item.y = heights[shortest];
            heights[shortest] += item.height + spacing;
        }
        contentHeight = Math.max(0, Math.max(...heights) - spacing);
    }

    onWidthChanged: relayout()
    onColumnsChanged: relayout()

    Repeater {
        id: repeater
        onItemAdded: (index, item) => {
            item.heightChanged.connect(masonry.relayout);
            masonry.relayout();
        }
        onItemRemoved: masonry.relayout()
    }

    Connections {
        target: repeater.model
        ignoreUnknownSignals: true
        function onRowsMoved() {
            masonry.relayout();
        }
    }
}
