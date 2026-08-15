#pragma once
#include <QObject>
#include <QUrl>
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
    // Portable file:// URL -> local filesystem path conversion (QUrl::toLocalFile()
    // handles Windows/Unix differences correctly, unlike a hand-rolled regex).
    Q_INVOKABLE QString toLocalPath(const QUrl &url) const { return url.toLocalFile(); }

private:
    ConfigStore *m_configStore;
};
