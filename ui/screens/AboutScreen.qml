import QtQuick
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit
        Text {
            text: "About"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }
    }
}
