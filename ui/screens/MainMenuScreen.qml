import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit * 2

        Text {
            text: "Menu Principal"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        Button {
            text: "Réglages"
            onClicked: ScreenManager.push("Settings")
        }

        Button {
            text: "Quitter"
            onClicked: Qt.quit()
        }
    }
}
