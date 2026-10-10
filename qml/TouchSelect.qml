// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

// Selecting words with a finger, laid over a piece of text: a hold on a word
// selects it, everything between two spaces, so a command or a code is taken
// whole; sliding on without lifting takes in more. A finger dragged over text
// otherwise scrolls the page or moves the note, so selecting starts only with
// the hold, and what would scroll or move is told to stand still
// (selecting) until the finger lifts. A mouse selects as it always does.
Item {
    id: touch

    // The TextEdit or TextArea the words are in.
    required property Item target
    readonly property bool selecting: held
    property bool held: false
    // The word first held, which stays selected however the finger slides.
    property int heldFrom: 0
    property int heldTo: 0
    property int from: 0
    property int to: 0
    readonly property point at: finger.point.position

    // A selection made by a finger has ended, with the finger lifted.
    signal selected()

    function positionAt(point) {
        const p = touch.mapToItem(target, point.x, point.y);
        return target.positionAt(p.x, p.y);
    }

    function wordAround(position) {
        const text = target.text;
        let start = Math.max(0, Math.min(position, text.length));
        let end = start;
        while (start > 0 && !/\s/.test(text.charAt(start - 1))) {
            --start;
        }
        while (end < text.length && !/\s/.test(text.charAt(end))) {
            ++end;
        }
        return [start, end];
    }

    function apply() {
        target.select(from, to);
    }

    onAtChanged: {
        if (!held) {
            return;
        }
        const word = wordAround(positionAt(at));
        from = Math.min(heldFrom, word[0]);
        to = Math.max(heldTo, word[1]);
        apply();
    }

    TapHandler {
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
        onLongPressed: {
            const word = touch.wordAround(touch.positionAt(point.position));
            if (word[0] === word[1]) {
                return;
            }
            touch.heldFrom = touch.from = word[0];
            touch.heldTo = touch.to = word[1];
            touch.held = true;
            // After whatever else answers the hold, such as a style's own
            // word selection.
            Qt.callLater(touch.apply);
        }
    }

    PointHandler {
        id: finger
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
        onActiveChanged: {
            if (!active && touch.held) {
                touch.held = false;
                // The text places its cursor where a finger lifts; the
                // selection is put back once it has.
                Qt.callLater(() => {
                    touch.apply();
                    touch.selected();
                });
            }
        }
    }
}
