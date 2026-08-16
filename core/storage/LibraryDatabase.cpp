#include "LibraryDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>

LibraryDatabase::LibraryDatabase(QString dbPath) : m_dbPath(std::move(dbPath)) {
    m_connectionName = QUuid::createUuid().toString();
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);
}

LibraryDatabase::~LibraryDatabase() {
    // QSqlDatabase::removeDatabase() requires every QSqlDatabase handle
    // referencing this connection name to be released first, or it just
    // warns and leaves the connection registered -- reset m_db before
    // calling it. Without this, each LibraryDatabase instance (e.g. one
    // per LibraryScanner::startScan() call) permanently leaks a named
    // entry in Qt's global connection registry.
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool LibraryDatabase::open() {
    if (!m_db.open()) return false;
    QSqlQuery q(m_db);
    return q.exec(
        "CREATE TABLE IF NOT EXISTS games ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  rom_path TEXT NOT NULL UNIQUE,"
        "  system TEXT NOT NULL,"
        "  title TEXT NOT NULL,"
        "  boxart_path TEXT"
        ")");
}

int LibraryDatabase::gameCount() const {
    QSqlQuery q("SELECT COUNT(*) FROM games", m_db);
    q.next();
    return q.value(0).toInt();
}

qint64 LibraryDatabase::insertGame(const QString &romPath, const QString &system,
                                    const QString &title) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO games (rom_path, system, title) VALUES (?, ?, ?)");
    q.addBindValue(romPath);
    q.addBindValue(system);
    q.addBindValue(title);
    if (!q.exec()) return -1;
    return q.lastInsertId().toLongLong();
}

QStringList LibraryDatabase::allRomPaths() const {
    QStringList paths;
    QSqlQuery q("SELECT rom_path FROM games", m_db);
    while (q.next()) {
        paths.append(q.value(0).toString());
    }
    return paths;
}

void LibraryDatabase::removeGame(const QString &romPath) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM games WHERE rom_path = ?");
    q.addBindValue(romPath);
    q.exec();
}
