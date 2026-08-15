#include "RomSourcesStore.h"
#include <QJsonArray>
#include <QJsonObject>

RomSourcesStore::RomSourcesStore(ConfigStore *configStore, QObject *parent)
    : QObject(parent), m_configStore(configStore) {}

QVariantList RomSourcesStore::sources() const {
    QVariantList result;
    for (const auto &v : m_configStore->data()["romSources"].toArray()) {
        result.append(v.toObject().toVariantMap());
    }
    return result;
}

void RomSourcesStore::addSource(const QString &path, const QString &label) {
    QJsonObject data = m_configStore->data();
    QJsonArray sources = data["romSources"].toArray();

    QJsonObject entry;
    entry["path"] = path;
    entry["label"] = label;
    entry["enabled"] = true;
    sources.append(entry);

    data["romSources"] = sources;
    m_configStore->setData(data);
    m_configStore->save();
}

void RomSourcesStore::removeSource(const QString &path) {
    QJsonObject data = m_configStore->data();
    QJsonArray sources = data["romSources"].toArray();
    QJsonArray filtered;
    for (const auto &v : sources) {
        if (v.toObject()["path"].toString() != path) filtered.append(v);
    }
    data["romSources"] = filtered;
    m_configStore->setData(data);
    m_configStore->save();
}

void RomSourcesStore::setEnabled(const QString &path, bool enabled) {
    QJsonObject data = m_configStore->data();
    QJsonArray sources = data["romSources"].toArray();
    for (int i = 0; i < sources.size(); ++i) {
        QJsonObject entry = sources[i].toObject();
        if (entry["path"].toString() == path) {
            entry["enabled"] = enabled;
            sources[i] = entry;
        }
    }
    data["romSources"] = sources;
    m_configStore->setData(data);
    m_configStore->save();
}

bool RomSourcesStore::hasAnySource() const {
    return !m_configStore->data()["romSources"].toArray().isEmpty();
}
