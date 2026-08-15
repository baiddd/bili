import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit
        Text { text: "Scraper"; color: Theme.colorText; font.pixelSize: Theme.fontSizeTitle }
        Button { text: "Scraper toute la bibliothèque"; onClicked: statusText.text = ScraperProvider.scrapeLibrary() }
        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
        Button { text: "Retour"; onClicked: ScreenManager.pop() }
    }
}
