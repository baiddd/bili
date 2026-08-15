#pragma once
#include "IScraperProvider.h"

class StubScraperProvider : public IScraperProvider {
public:
    bool isImplemented() const override { return false; }
    QString scrapeGame(qint64 gameId) override;
    QString scrapeLibrary() override;
};
