#pragma once
#include <QObject>
#include "IScraperProvider.h"

class ScraperProviderQmlBridge : public QObject {
    Q_OBJECT
public:
    explicit ScraperProviderQmlBridge(IScraperProvider *provider, QObject *parent = nullptr)
        : QObject(parent), m_provider(provider) {}

    Q_INVOKABLE QString scrapeGame(qint64 gameId) { return m_provider->scrapeGame(gameId); }
    Q_INVOKABLE QString scrapeLibrary() { return m_provider->scrapeLibrary(); }

private:
    IScraperProvider *m_provider;
};
