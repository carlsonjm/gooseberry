// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami

// The sheet's surface: the room the panels leave, the work behind it dimmed.
// A tap on the work puts the sheet away; the note is already kept.
Window {
    id: root

    required property QtObject shell

    property bool shown: false

    title: i18n("Gooseberry")
    color: "transparent"
    visible: false

    function open() {
        hideTimer.stop();
        visible = true;
        shown = true;
        requestActivate();
        Qt.callLater(sheet.focusText);
    }

    function putAway() {
        if (!shown) {
            return;
        }
        shown = false;
        Qt.inputMethod.hide();
        hideTimer.restart();
    }

    Timer {
        id: hideTimer
        interval: Kirigami.Units.longDuration
        onTriggered: root.visible = false
    }

    Rectangle {
        id: dim
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.42)
        opacity: root.shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
        TapHandler {
            onTapped: root.shell.capture.finish()
        }
    }

    CaptureSheet {
        id: sheet
        capture: root.shell.capture
        projects: root.shell.places.projects
        width: Math.min(implicitWidth, root.width - 2 * Kirigami.Units.largeSpacing)
        height: implicitHeight
        anchors.horizontalCenter: parent.horizontalCenter
        // The keys, where the desktop says how tall they are, push the sheet up.
        readonly property real keys: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0
        y: root.shown ? root.height - height - keys : root.height
        Behavior on y {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
        onBoardRequested: {
            root.shell.capture.finish();
            root.shell.showBoard();
        }
    }

    // Where the desktop moves the keyboard elsewhere, the sheet goes down
    // with it, as when going back to the work.
    onActiveChanged: {
        if (!active && shown) {
            shell.capture.finish();
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: root.shell.capture.finish()
    }

    Connections {
        target: root.shell.capture
        function onFinished() {
            root.putAway();
        }
    }
}
