#pragma once
#include <QObject>
#include <QVariantList>
#include "ConfigStore.h"

class RomSourcesStore : public QObject {
    Q_OBJECT
public:
    explicit RomSourcesStore(ConfigStore *configStore, QObject *parent = nullptr);
    Q_INVOKABLE QVariantList sources() const;
    Q_INVOKABLE void addSource(const QString &path, const QString &label);
    Q_INVOKABLE void removeSource(const QString &path);
    Q_INVOKABLE void setEnabled(const QString &path, bool enabled);
    Q_INVOKABLE bool hasAnySource() const;

private:
    ConfigStore *m_configStore;
};
