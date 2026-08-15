#include "ConfigStore.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>

ConfigStore::ConfigStore(QString dataDir, QObject *parent)
    : QObject(parent), m_dataDir(std::move(dataDir)) {}

bool ConfigStore::save() const {
    QDir().mkpath(m_dataDir);
    QFile file(m_dataDir + "/config.json");
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(m_data).toJson());
    return true;
}

bool ConfigStore::load() {
    QFile file(m_dataDir + "/config.json");
    if (!file.open(QIODevice::ReadOnly)) return false;
    m_data = QJsonDocument::fromJson(file.readAll()).object();
    return true;
}
