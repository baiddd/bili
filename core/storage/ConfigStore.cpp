#include "ConfigStore.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

ConfigStore::ConfigStore(QString dataDir, QObject *parent)
    : QObject(parent), m_dataDir(std::move(dataDir)) {}

bool ConfigStore::save() const {
    QDir().mkpath(m_dataDir);
    QFile file(m_dataDir + "/config.json");
    if (!file.open(QIODevice::WriteOnly)) return false;

    const QByteArray json = QJsonDocument(m_data).toJson();
    const qint64 written = file.write(json);
    if (written != json.size()) return false;
    if (!file.flush()) return false;

    return true;
}

bool ConfigStore::load() {
    QFile file(m_dataDir + "/config.json");
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QByteArray raw = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError) return false;

    m_data = doc.object();
    return true;
}
