// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Capture.h"

#include <QObject>
#include <QPointer>

class QQmlEngine;
class QQuickWindow;

namespace Gooseberry {

class NoteStore;
class Notifier;
class PlaceNotes;
class Places;
class Planner;
class Reminders;
class SpreadGuest;
class StuckNotes;
class WindowContext;

// The desktop around the core: the quick-note card centred over the work,
// the board window, and the window in front. One running
// Gooseberry answers every tap of its button.
class Shell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *capture READ captureObject CONSTANT)
    Q_PROPERTY(QObject *places READ placesObject CONSTANT)
    Q_PROPERTY(QObject *notes READ notesObject CONSTANT)
    Q_PROPERTY(QObject *store READ storeObject CONSTANT)
    Q_PROPERTY(QObject *guest READ guestObject CONSTANT)
    Q_PROPERTY(QObject *planner READ plannerObject CONSTANT)
    Q_PROPERTY(QObject *stuck READ stuckObject CONSTANT)
    // Words for a bar on the card leading back to whoever opened the note
    // there, such as a search's Notes; empty for none.
    Q_PROPERTY(QString backLabel READ backLabel NOTIFY backLabelChanged)

public:
    Shell(NoteStore *store, QQmlEngine *engine, QObject *parent = nullptr);
    ~Shell() override;

    Capture *capture() const { return m_capture; }
    Places *places() const { return m_places; }
    PlaceNotes *notes() const { return m_notes; }
    NoteStore *store() const { return m_store; }
    SpreadGuest *guest() const { return m_guest; }
    Planner *planner() const { return m_planner; }
    StuckNotes *stuck() const { return m_stuck; }
    Reminders *reminders() const { return m_reminders; }
    QString backLabel() const { return m_backLabel; }

    // Answers a start or a later tap: no arguments or --capture for the
    // quick-note card, --board for the board, --background to get ready
    // without showing anything.
    void handle(const QStringList &arguments);

    Q_INVOKABLE void showCapture();
    // The card on a new note holding those words, with a way back named
    // back, or none.
    void showCaptureWith(const QString &text, const QString &back = {});
    Q_INVOKABLE void showBoard();
    // The board as a window, on the place the note sits in, or on Today.
    void showBoardOn(const QString &noteId);
    // What is in front now, for a note started from the search.
    CaptureContext currentContext() const;
    // The windows open now, the one in front first (WindowContext).
    Q_INVOKABLE QVariantList openWindows() const;
    // The workspace in front now; empty where the desktop does not say.
    Q_INVOKABLE QString currentWorkspace() const;
    // The application's name, as its desktop file gives it.
    Q_INVOKABLE QString applicationName() const;
    // All notes on the card: the board opens as Gooseberry's own window, on
    // the place the note sits in, and the card goes.
    Q_INVOKABLE void boardFromCard();
    // The card's way back: backRequested, and the card goes.
    Q_INVOKABLE void goBack();
    // The words for a reminder, and what Remind offers for the note on the
    // card (ReminderWords).
    Q_INVOKABLE QString reminderLabel(const QDateTime &remind, bool onOpen) const;
    Q_INVOKABLE QVariantList reminderChoices() const;
    Q_INVOKABLE void openNote(const QString &id);
    // The note on the card, with a way back named back, or none; false when
    // there is no such note.
    bool openNoteWith(const QString &id, const QString &back);
    Q_INVOKABLE void newNoteIn(const QString &placeKey);

Q_SIGNALS:
    // The board's window drew its first frame after being asked for.
    void boardShown();
    void backLabelChanged();
    // The card's way back was taken.
    void backRequested();

private Q_SLOTS:
    void sleeping(bool goingToSleep);

private:
    QObject *captureObject() const;
    QObject *placesObject() const;
    QObject *notesObject() const;
    QObject *storeObject() const;
    QObject *guestObject() const;
    QObject *plannerObject() const;
    QObject *stuckObject() const;
    void setBack(const QString &label);
    QQuickWindow *captureWindow();
    QQuickWindow *boardWindow();
    QQuickWindow *create(const QString &name);
    void raiseCard();

    NoteStore *m_store;
    QQmlEngine *m_engine;
    Capture *m_capture;
    Places *m_places;
    PlaceNotes *m_notes;
    WindowContext *m_context;
    QPointer<QQuickWindow> m_captureWindow;
    QPointer<QQuickWindow> m_boardWindow;
    SpreadGuest *m_guest;
    Planner *m_planner;
    Reminders *m_reminders;
    Notifier *m_notifier;
    StuckNotes *m_stuck;
    QString m_backLabel;
};

} // namespace Gooseberry
