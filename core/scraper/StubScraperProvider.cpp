#include "StubScraperProvider.h"

QString StubScraperProvider::scrapeGame(qint64 gameId) {
    return "Scraping jeu #" + QString::number(gameId) + " : non implémenté (sous-projet 4)";
}
QString StubScraperProvider::scrapeLibrary() {
    return "Scraping bibliothèque : non implémenté (sous-projet 4)";
}
