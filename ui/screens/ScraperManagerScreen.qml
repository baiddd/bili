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
        Button {
            id: scrapeLibraryButton
            text: "Scraper toute la bibliothèque"
            focus: true
            Component.onCompleted: forceActiveFocus()
            background: Rectangle {
                color: scrapeLibraryButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: scrapeLibraryButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: statusText.text = ScraperProvider.scrapeLibrary()
        }
        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
    }
}
