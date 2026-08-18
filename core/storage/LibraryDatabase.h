#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QSqlDatabase>

struct GameRow {
    qint64 id;
    QString romPath;
    QString system;
    QString title;
};

class LibraryDatabase {
public:
    // Its destructor calls QSqlDatabase::removeDatabase(m_connectionName);
    // an accidental copy would let two LibraryDatabase instances both
    // think they own the same connection name, and one's destructor could
    // yank the connection out from under the other still using it.
    Q_DISABLE_COPY(LibraryDatabase)

    explicit LibraryDatabase(QString dbPath);
    ~LibraryDatabase();
    bool open();
    bool isOpen() const { return m_db.isOpen(); }
    QString path() const { return m_dbPath; }
    int gameCount() const;
    qint64 insertGame(const QString &romPath, const QString &system,
                       const QString &title);
    QStringList allRomPaths() const;
    QList<GameRow> allGames() const;
    void removeGame(const QString &romPath);

private:
    QString m_dbPath;
    QString m_connectionName;
    QSqlDatabase m_db;
};
