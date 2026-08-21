#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include <QMap>
#include <QMetaType>
#include "network/NetworkManager.h"

struct CoreCatalogEntry {
    QString core;
    QUrl url;
};

struct EmulatorCatalogData {
    QString retroArchVersion;
    QUrl retroArchUrl;
    QMap<QString, CoreCatalogEntry> coresBySystem;
};
Q_DECLARE_METATYPE(EmulatorCatalogData)

class EmulatorCatalog : public QObject {
    Q_OBJECT
public:
    explicit EmulatorCatalog(NetworkManager *networkManager, QObject *parent = nullptr);
    void fetch(const QUrl &manifestUrl);

    // Exposed for testing only: the temp file path used by the most recent
    // fetch() call, so tests can verify it gets cleaned up.
    QString tempPathForTesting() const { return m_tempPath; }

signals:
    void ready(const EmulatorCatalogData &data);
    void failed(const QString &errorString);

private:
    NetworkManager *m_networkManager;
    int m_pendingRequestId = -1;
    QString m_tempPath;
};
