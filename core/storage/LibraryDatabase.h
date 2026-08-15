#pragma once
#include <QString>
#include <QSqlDatabase>

class LibraryDatabase {
public:
    explicit LibraryDatabase(QString dbPath);
    bool open();
    bool isOpen() const { return m_db.isOpen(); }
    int gameCount() const;
    qint64 insertGame(const QString &romPath, const QString &system,
                       const QString &title);

private:
    QString m_dbPath;
    QSqlDatabase m_db;
};
