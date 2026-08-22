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
    // Q_INVOKABLE (fix wave, sub-project 3 final review) so QML can call this
    // directly to re-fetch the catalog each time EmulatorManagerScreen opens,
    // not just once at boot in main.cpp - restores the design spec's original
    // "re-fetch à chaque ouverture de l'écran" intent and gives the user a
    // real way to retry after starting the app offline, short of relaunching.
    Q_INVOKABLE void fetch(const QUrl &manifestUrl);

    // Canonical manifest URL, exposed so QML doesn't need to hardcode a
    // second copy of this literal alongside main.cpp's boot-time fetch() call.
    Q_INVOKABLE static QUrl manifestUrl();

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
