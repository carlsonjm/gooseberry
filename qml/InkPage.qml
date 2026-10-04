// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import io.github.carlsonjm.gooseberry

// The ruled ink page under the note's words. In Pen the pen writes, pressing
// harder for a wider line, and a finger writes too until the pen comes near
// the screen; then a resting hand writes nothing and a finger scrolls. The
// pen's eraser end, or Eraser in the palette, takes each whole stroke it
// touches. Out of Pen the page only shows the ink.
Flickable {
    id: page

    required property QtObject capture
    // Writing, rather than only showing the ink.
    property bool writing: false
    property string ink: "black"
    property bool erasing: false
    // Room kept free at the bottom, for the palette and the reading.
    property real bottomRoom: 0
    // The pen is near the screen, or only just lifted.
    readonly property bool penNear: penHover.hovered || penLingers.running

    // Told when the eraser has taken strokes, with how many.
    signal erased(int count)
    // A pen touched the page while it was only showing ink.
    signal penArrived()

    readonly property alias canvas: canvas

    contentWidth: width
    contentHeight: canvas.height + bottomRoom
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    // A finger writing is not a finger scrolling.
    interactive: !writing || penNear

    function pagePoint(position) {
        return Qt.point(position.x / canvas.scale, position.y / canvas.scale);
    }

    // The line being written stays in sight as the page grows.
    function followWriting() {
        const bottom = canvas.height + bottomRoom;
        if (bottom > contentY + height) {
            contentY = Math.max(0, bottom - height);
        }
    }

    InkCanvas {
        id: canvas
        objectName: "inkCanvas"
        width: page.width
        height: Math.max(pageHeight, page.height - page.bottomRoom)
        capture: page.capture
        ruled: page.writing

        // While the pen is near, and a moment after it lifts, the hand
        // resting on the page writes nothing.
        HoverHandler {
            id: penHover
            acceptedDevices: PointerDevice.Stylus
            onHoveredChanged: {
                if (!hovered) {
                    penLingers.restart();
                }
            }
        }

        Timer {
            id: penLingers
            interval: 800
        }

        // The pen's own eraser end, in Pen or out of it.
        DragHandler {
            id: eraserEnd
            objectName: "eraserEnd"
            target: null
            dragThreshold: 0
            acceptedDevices: PointerDevice.Stylus
            acceptedPointerTypes: PointerDevice.Eraser
            enabled: !page.capture.readOnly
            onActiveChanged: {
                if (active) {
                    const at = page.pagePoint(centroid.pressPosition);
                    page.capture.eraseAt(at.x, at.y);
                } else {
                    page.erased(page.capture.endErase());
                }
            }
            onCentroidChanged: {
                if (active) {
                    const at = page.pagePoint(centroid.position);
                    page.capture.eraseAt(at.x, at.y);
                }
            }
        }

        // The pen's point, a finger with no pen near, and a mouse.
        DragHandler {
            id: draw
            objectName: "draw"
            target: null
            dragThreshold: 0
            acceptedDevices: page.penNear ? PointerDevice.Stylus
                                          : PointerDevice.Stylus | PointerDevice.TouchScreen | PointerDevice.Mouse
            acceptedPointerTypes: PointerDevice.Pen | PointerDevice.Finger | PointerDevice.GenericPointer
            enabled: page.writing && !page.capture.readOnly
            property bool rubbing: false
            function pressure() {
                return centroid.pressure > 0 ? centroid.pressure : 0.5;
            }
            onActiveChanged: {
                if (active) {
                    rubbing = page.erasing;
                    const at = page.pagePoint(centroid.pressPosition);
                    if (rubbing) {
                        page.capture.eraseAt(at.x, at.y);
                    } else {
                        page.capture.beginStroke(at.x, at.y, pressure(), page.ink);
                    }
                } else if (rubbing) {
                    page.erased(page.capture.endErase());
                } else {
                    page.capture.endStroke();
                    page.followWriting();
                }
            }
            onCentroidChanged: {
                if (!active) {
                    return;
                }
                const at = page.pagePoint(centroid.position);
                if (rubbing) {
                    page.capture.eraseAt(at.x, at.y);
                } else {
                    page.capture.extendStroke(at.x, at.y, pressure());
                }
            }
        }

        // A dot: the pen, or a finger, touching and lifting without moving.
        TapHandler {
            objectName: "dot"
            acceptedDevices: draw.acceptedDevices
            acceptedPointerTypes: draw.acceptedPointerTypes
            enabled: draw.enabled
            gesturePolicy: TapHandler.DragThreshold
            onTapped: eventPoint => {
                const at = page.pagePoint(eventPoint.position);
                if (page.erasing) {
                    page.capture.eraseAt(at.x, at.y);
                    page.erased(page.capture.endErase());
                } else {
                    page.capture.beginStroke(at.x, at.y, eventPoint.pressure > 0 ? eventPoint.pressure : 0.5, page.ink);
                    page.capture.endStroke();
                }
            }
        }

        // Out of Pen, a pen touching the page switches to Pen.
        TapHandler {
            objectName: "penArrives"
            acceptedDevices: PointerDevice.Stylus
            acceptedPointerTypes: PointerDevice.Pen
            enabled: !page.writing && !page.capture.readOnly
            onPressedChanged: {
                if (pressed) {
                    page.penArrived();
                }
            }
        }
    }

    QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
}
