#pragma once
#include <QObject>
#include <QUrl>
#include <QDir>
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
    // Used by FirstLaunchSetupScreen before registering a proposed default
    // path that may not exist yet (a user-picked path via FolderDialog
    // always exists already, since the native picker only shows real
    // folders -- these two exist for the synthesized-default-path case).
    Q_INVOKABLE bool pathExists(const QString &path) const { return QDir(path).exists(); }
    Q_INVOKABLE bool createPath(const QString &path) const { return QDir().mkpath(path); }

private:
    ConfigStore *m_configStore;
};
