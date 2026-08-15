#pragma once
#include <QString>

class IScraperProvider {
public:
    virtual ~IScraperProvider() = default;
    virtual bool isImplemented() const = 0;
    virtual QString scrapeGame(qint64 gameId) = 0;
    virtual QString scrapeLibrary() = 0;
};
