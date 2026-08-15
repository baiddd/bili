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
        Button {
            id: installRetroArchButton
            text: "Installer RetroArch"
            focus: true
            KeyNavigation.down: installCoreButton
            Component.onCompleted: forceActiveFocus()
            background: Rectangle {
                color: installRetroArchButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: installRetroArchButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: statusText.text = EmulatorProvider.installRetroArch()
        }
        Button {
            id: installCoreButton
            text: "Installer un core"
            KeyNavigation.up: installRetroArchButton
            KeyNavigation.down: installStandaloneButton
            background: Rectangle {
                color: installCoreButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: installCoreButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: statusText.text = EmulatorProvider.installCore("nes")
        }
        Button {
            id: installStandaloneButton
            text: "Installer un émulateur autonome"
            KeyNavigation.up: installCoreButton
            background: Rectangle {
                color: installStandaloneButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: installStandaloneButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: statusText.text = EmulatorProvider.installStandaloneEmulator("Dolphin")
        }
        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
    }
}
