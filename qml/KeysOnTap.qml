// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick

// A finger's tap on a piece of text asks for the on-screen keys again once it
// has lifted. A text field asks for the keys as the finger comes down, while
// its cursor still stands where it was, and puts the cursor under the finger
// only as it lifts. A desktop that tells a tap on the text from a program
// focusing its own field by the line the cursor is on, as Kadunce does, reads
// that first ask as the program's own and keeps the keys down; asked again
// with the cursor where the finger lifted, they come. A mouse asks for no keys.
Item {
    id: keys

    // The TextEdit or TextArea the words are in.
    required property Item target

    // The keys were asked for again, after a tap.
    signal asked()

    function ask() {
        if (!target.activeFocus || target.readOnly) {
            return;
        }
        Qt.inputMethod.update(Qt.ImQueryAll);
        Qt.inputMethod.show();
        asked();
    }

    TapHandler {
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
        // After the text has answered the lift and moved its cursor.
        onTapped: Qt.callLater(keys.ask)
    }
}
