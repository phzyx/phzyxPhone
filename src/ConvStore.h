// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#pragma once

// ConvStore: a small SQLite-backed store for real-time-text (RTT)
// conversations. Messages are grouped per remote peer (a normalised SIP
// address), so all RTT exchanged with a given party forms one persistent
// thread that survives across calls and across application restarts.
//
// Each loaded profile points at its own database file (path recorded in the
// profile YAML), so loading a profile automatically reopens its history.
// Access must happen on the GUI thread - the RTT signals from SipCore are
// delivered there via queued connections, keeping all DB use single-threaded.

#include <QObject>
#include <QString>
#include <QList>
#include <QSqlDatabase>

struct ConvMessage {
    QString peer;        // normalised remote address (conversation key)
    QString direction;   // "in" or "out"
    QString body;        // the text block
    qint64  ts = 0;      // epoch milliseconds
};

// One entry per conversation, for the list on the left of the tab.
struct ConvPeer {
    QString peer;        // normalised remote address
    QString display;     // human-friendly label (last-seen display form)
    qint64  lastTs = 0;  // most recent message time
    int     count = 0;   // messages retained
};

class ConvStore : public QObject {
    Q_OBJECT
public:
    explicit ConvStore(QObject *parent = nullptr);
    ~ConvStore() override;

    // Open (creating if needed) the database at path. Closes any previous one.
    // Returns false and sets err on failure.
    bool open(const QString &path, QString &err);
    void close();
    bool isOpen() const { return db_.isValid() && db_.isOpen(); }
    QString path() const { return path_; }

    // Maximum messages retained per conversation (0 = unlimited). Changing it
    // trims every conversation immediately.
    void setMaxDepth(int n);
    int  maxDepth() const { return maxDepth_; }

    // Record a message and trim the conversation to maxDepth. display is the
    // friendly label to remember for the peer (e.g. the raw remote URI).
    void addMessage(const QString &peer, const QString &display,
                    const QString &direction, const QString &body);

    QList<ConvPeer>    peers() const;                  // newest activity first
    QList<ConvMessage> messages(const QString &peer) const;  // oldest first
    void deletePeer(const QString &peer);              // drop a whole thread
    void clear();                                      // drop everything

private:
    void ensureSchema();
    void trim(const QString &peer);
    void trimAll();

    QSqlDatabase db_;
    QString      path_;
    QString      connName_;
    int          maxDepth_ = 500;
};
