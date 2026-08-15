import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit
        Text {
            text: "GameDetails"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }
        Button { text: "Retour"; onClicked: ScreenManager.pop() }
    }
}
