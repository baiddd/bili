#pragma once
#include <QAbstractListModel>
#include "storage/LibraryDatabase.h"

class LibraryModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { TitleRole = Qt::UserRole + 1, SystemRole, RomPathRole };

    explicit LibraryModel(LibraryDatabase *db, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void refresh();

private:
    LibraryDatabase *m_db;
    QList<GameRow> m_games;
};
