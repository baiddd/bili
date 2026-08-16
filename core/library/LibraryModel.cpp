#include "LibraryModel.h"

LibraryModel::LibraryModel(LibraryDatabase *db, QObject *parent)
    : QAbstractListModel(parent), m_db(db) {
    m_games = m_db->allGames();
}

int LibraryModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_games.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_games.size()) return {};
    const GameRow &game = m_games.at(index.row());
    switch (role) {
        case TitleRole: return game.title;
        case SystemRole: return game.system;
        case RomPathRole: return game.romPath;
        default: return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const {
    return {
        {TitleRole, "title"},
        {SystemRole, "system"},
        {RomPathRole, "romPath"},
    };
}

void LibraryModel::refresh() {
    beginResetModel();
    m_games = m_db->allGames();
    endResetModel();
}
