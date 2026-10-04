// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Capture.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

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
    // The card waits for the board's window to take its place.
    Q_PROPERTY(bool handingOff READ handingOff NOTIFY handingOffChanged)

public:
    Shell(NoteStore *store, QQmlEngine *engine, QObject *parent = nullptr);
    ~Shell() override;

    Capture *capture() const { return m_capture; }
    Places *places() const { return m_places; }
    PlaceNotes *notes() const { return m_notes; }
    NoteStore *store() const { return m_store; }
    SpreadGuest *guest() const { return m_guest; }
    Planner *planner() const { return m_planner; }
    Reminders *reminders() const { return m_reminders; }
    bool handingOff() const { return m_handingOff; }

    // Answers a start or a later tap: no arguments or --capture for the
    // quick-note card, --board for the board, --background to get ready
    // without showing anything.
    void handle(const QStringList &arguments);

    Q_INVOKABLE void showCapture();
    Q_INVOKABLE void showBoard();
    // The board as a window, on the place the note sits in, or on Today.
    void showBoardOn(const QString &noteId);
    // What is in front now, for a note started from the search.
    CaptureContext currentContext() const;
    // The application's name, as its desktop file gives it.
    Q_INVOKABLE QString applicationName() const;
    // All notes on the card. In Kadunce's Spread or over its Active card the
    // board opens as a window, which takes the card's place; elsewhere the
    // card shows the board itself, and nothing more happens here.
    Q_INVOKABLE void boardFromCard();
    // The card back at its own size, from the board.
    Q_INVOKABLE void collapseCard();
    // How long the card waits for the board to take its place.
    void setHandOffWait(int ms) { m_handOffWait = ms; }
    // The words for a reminder, and what Remind offers for the note on the
    // card (ReminderWords).
    Q_INVOKABLE QString reminderLabel(const QDateTime &remind, bool onOpen) const;
    Q_INVOKABLE QVariantList reminderChoices() const;
    Q_INVOKABLE void openNote(const QString &id);
    Q_INVOKABLE void newNoteIn(const QString &placeKey);

Q_SIGNALS:
    // The board's window drew its first frame after being asked for.
    void boardShown();
    void handingOffChanged();
    // The board has taken the card's place: the card fades away.
    void handOffDone();

private Q_SLOTS:
    void sleeping(bool goingToSleep);

private:
    QObject *captureObject() const;
    QObject *placesObject() const;
    QObject *notesObject() const;
    QObject *storeObject() const;
    QObject *guestObject() const;
    QObject *plannerObject() const;
    QString boardId() const;
    void setHandingOff(bool handingOff);
    void finishHandOff();
    void stopHandOff();
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
    bool m_handingOff = false;
    QString m_handOffToken;
    QTimer m_handOffTimeout;
    int m_handOffWait = 10000;
};

} // namespace Gooseberry
