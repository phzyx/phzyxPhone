// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#include "ConvStore.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static int s_connCounter = 0;

ConvStore::ConvStore(QObject *parent) : QObject(parent) {
    connName_ = QString("convstore_%1").arg(++s_connCounter);
}

ConvStore::~ConvStore() {
    close();
}

bool ConvStore::open(const QString &path, QString &err) {
    close();

    // Make sure the parent directory exists (the DB lives within the install,
    // e.g. an ignored data/ folder).
    const QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    db_ = QSqlDatabase::addDatabase("QSQLITE", connName_);
    db_.setDatabaseName(path);
    if (!db_.open()) {
        err = db_.lastError().text();
        QSqlDatabase::removeDatabase(connName_);
        db_ = QSqlDatabase();
        return false;
    }
    path_ = path;
    ensureSchema();
    trimAll();
    return true;
}

void ConvStore::close() {
    if (db_.isValid()) {
        if (db_.isOpen()) db_.close();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(connName_);
    }
    path_.clear();
}

void ConvStore::ensureSchema() {
    QSqlQuery q(db_);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec(
        "CREATE TABLE IF NOT EXISTS messages ("
        " id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        " peer      TEXT NOT NULL,"
        " display   TEXT,"
        " direction TEXT NOT NULL,"
        " body      TEXT NOT NULL,"
        " ts        INTEGER NOT NULL)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_messages_peer_ts"
           " ON messages(peer, ts)");
}

void ConvStore::setMaxDepth(int n) {
    maxDepth_ = n < 0 ? 0 : n;
    if (isOpen()) trimAll();
}

void ConvStore::addMessage(const QString &peer, const QString &display,
                           const QString &direction, const QString &body) {
    if (!isOpen() || peer.isEmpty()) return;
    QSqlQuery q(db_);
    q.prepare("INSERT INTO messages(peer, display, direction, body, ts)"
              " VALUES(?,?,?,?,?)");
    q.addBindValue(peer);
    q.addBindValue(display);
    q.addBindValue(direction);
    q.addBindValue(body);
    q.addBindValue((qint64)QDateTime::currentMSecsSinceEpoch());
    q.exec();
    trim(peer);
}

void ConvStore::trim(const QString &peer) {
    if (!isOpen() || maxDepth_ <= 0) return;
    // Delete all but the newest maxDepth_ rows for this peer.
    QSqlQuery q(db_);
    q.prepare(
        "DELETE FROM messages WHERE peer = :p AND id NOT IN ("
        " SELECT id FROM messages WHERE peer = :p2"
        " ORDER BY ts DESC, id DESC LIMIT :lim)");
    q.bindValue(":p", peer);
    q.bindValue(":p2", peer);
    q.bindValue(":lim", maxDepth_);
    q.exec();
}

void ConvStore::trimAll() {
    if (!isOpen() || maxDepth_ <= 0) return;
    QSqlQuery q(db_);
    q.exec("SELECT DISTINCT peer FROM messages");
    QStringList ps;
    while (q.next()) ps << q.value(0).toString();
    for (const QString &p : ps) trim(p);
}

QList<ConvPeer> ConvStore::peers() const {
    QList<ConvPeer> out;
    if (!isOpen()) return out;
    QSqlQuery q(db_);
    // One row per peer: latest display label, last activity, retained count.
    q.exec(
        "SELECT peer,"
        "       (SELECT display FROM messages m2 WHERE m2.peer = m.peer"
        "         ORDER BY ts DESC, id DESC LIMIT 1) AS display,"
        "       MAX(ts) AS lastTs, COUNT(*) AS n"
        " FROM messages m GROUP BY peer ORDER BY lastTs DESC");
    while (q.next()) {
        ConvPeer p;
        p.peer    = q.value(0).toString();
        p.display = q.value(1).toString();
        p.lastTs  = q.value(2).toLongLong();
        p.count   = q.value(3).toInt();
        if (p.display.isEmpty()) p.display = p.peer;
        out.push_back(p);
    }
    return out;
}

QList<ConvMessage> ConvStore::messages(const QString &peer) const {
    QList<ConvMessage> out;
    if (!isOpen()) return out;
    QSqlQuery q(db_);
    q.prepare("SELECT peer, direction, body, ts FROM messages"
              " WHERE peer = ? ORDER BY ts ASC, id ASC");
    q.addBindValue(peer);
    q.exec();
    while (q.next()) {
        ConvMessage m;
        m.peer      = q.value(0).toString();
        m.direction = q.value(1).toString();
        m.body      = q.value(2).toString();
        m.ts        = q.value(3).toLongLong();
        out.push_back(m);
    }
    return out;
}

void ConvStore::deletePeer(const QString &peer) {
    if (!isOpen()) return;
    QSqlQuery q(db_);
    q.prepare("DELETE FROM messages WHERE peer = ?");
    q.addBindValue(peer);
    q.exec();
}

void ConvStore::clear() {
    if (!isOpen()) return;
    QSqlQuery q(db_);
    q.exec("DELETE FROM messages");
}
