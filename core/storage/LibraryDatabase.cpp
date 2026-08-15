#include "LibraryDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>

LibraryDatabase::LibraryDatabase(QString dbPath) : m_dbPath(std::move(dbPath)) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    m_db.setDatabaseName(m_dbPath);
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
