#pragma once
#include <QString>
#include <QStringList>
#include <QSqlDatabase>

class LibraryDatabase {
public:
    explicit LibraryDatabase(QString dbPath);
    bool open();
    bool isOpen() const { return m_db.isOpen(); }
    QString path() const { return m_dbPath; }
    int gameCount() const;
    qint64 insertGame(const QString &romPath, const QString &system,
                       const QString &title);
    QStringList allRomPaths() const;
    void removeGame(const QString &romPath);

private:
    QString m_dbPath;
    QSqlDatabase m_db;
};
