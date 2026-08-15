import QtQuick
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit
        Text {
            text: "Thème"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }
        Text {
            text: "Bientôt disponible — un seul thème pour l'instant."
            color: Theme.colorAccent
            font.pixelSize: Theme.fontSizeBody
        }
    }
}
