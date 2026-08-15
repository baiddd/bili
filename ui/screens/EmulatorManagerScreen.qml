import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit
        Text { text: "Installer"; color: Theme.colorText; font.pixelSize: Theme.fontSizeTitle }
        Button { text: "Installer RetroArch"; onClicked: statusText.text = EmulatorProvider.installRetroArch() }
        Button { text: "Installer un core"; onClicked: statusText.text = EmulatorProvider.installCore("nes") }
        Button { text: "Installer un émulateur autonome"; onClicked: statusText.text = EmulatorProvider.installStandaloneEmulator("Dolphin") }
        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
    }
}
