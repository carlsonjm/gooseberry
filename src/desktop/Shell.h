// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>

class QQmlEngine;
class QQuickWindow;

namespace Gooseberry {

class Capture;
class NoteStore;
class PlaceNotes;
class Places;
class WindowContext;

// The desktop around the core: the capture sheet rising from the bottom of
// the screen, the board window, and the window in front. One running
// Gooseberry answers every tap of its button.
class Shell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *capture READ captureObject CONSTANT)
    Q_PROPERTY(QObject *places READ placesObject CONSTANT)
    Q_PROPERTY(QObject *notes READ notesObject CONSTANT)
    Q_PROPERTY(QObject *store READ storeObject CONSTANT)

public:
    Shell(NoteStore *store, QQmlEngine *engine, QObject *parent = nullptr);
    ~Shell() override;

    Capture *capture() const { return m_capture; }
    Places *places() const { return m_places; }
    PlaceNotes *notes() const { return m_notes; }
    NoteStore *store() const { return m_store; }

    // Answers a start or a later tap: no arguments for capture, --board for
    // the board, --background to get ready without showing anything.
    void handle(const QStringList &arguments);

    Q_INVOKABLE void showCapture();
    Q_INVOKABLE void showBoard();
    Q_INVOKABLE void openNote(const QString &id);
    Q_INVOKABLE void newNoteIn(const QString &placeKey);

private:
    QObject *captureObject() const;
    QObject *placesObject() const;
    QObject *notesObject() const;
    QObject *storeObject() const;
    QQuickWindow *captureWindow();
    QQuickWindow *boardWindow();
    QQuickWindow *create(const QString &name);
    void raiseSheet();

    NoteStore *m_store;
    QQmlEngine *m_engine;
    Capture *m_capture;
    Places *m_places;
    PlaceNotes *m_notes;
    WindowContext *m_context;
    QPointer<QQuickWindow> m_captureWindow;
    QPointer<QQuickWindow> m_boardWindow;
};

} // namespace Gooseberry
