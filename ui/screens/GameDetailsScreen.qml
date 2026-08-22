import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    property bool coreInstalled: EmulatorProvider.isCoreInstalled(ScreenManager.selectedGameSystem)

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit * 2

        Text {
            text: ScreenManager.selectedGameTitle
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }
        Text {
            text: "Système : " + ScreenManager.selectedGameSystem
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
        }

        Button {
            id: actionButton
            text: coreInstalled ? "Lancer" : "Installer un core pour ce système"
            focus: true
            Component.onCompleted: forceActiveFocus()
            background: Rectangle {
                color: actionButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: actionButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: {
                if (coreInstalled) {
                    EmulatorProvider.launchGame(ScreenManager.selectedGameRomPath, ScreenManager.selectedGameSystem)
                } else {
                    ScreenManager.push("EmulatorManager")
                }
            }
        }

        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
    }

    Connections {
        target: EmulatorProvider
        function onGameLaunched() { statusText.text = "En cours de jeu..." }
        function onGameExited(exitCode) { statusText.text = "" }
        function onLaunchFailed(errorString) { statusText.text = "Erreur : " + errorString }
    }
}
