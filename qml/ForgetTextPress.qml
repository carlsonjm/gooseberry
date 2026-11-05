// SPDX-License-Identifier: GPL-2.0-or-later
import QtQml
import org.kde.desktop.private as DesktopPrivate

// Plasma's own style puts Cut, Copy and Paste over a text field in tablet
// mode after a finger presses it. It keeps that press, from any field in the
// program, until some field's words change, and shows the buttons over
// whichever field next takes the keys. A window that gives its own field the
// keys as it opens would have them up with nothing pressed, over the field.
// Made, this forgets the press. Only Plasma's style has the buttons, so it
// is made only where that style is installed; elsewhere it fails to load,
// and nothing is lost.
QtObject {
    Component.onCompleted: DesktopPrivate.MobileTextActionsToolBar.shouldBeVisible = false
}
