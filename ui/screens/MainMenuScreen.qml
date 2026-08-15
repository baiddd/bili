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
            text: "MainMenu"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        Button {
            id: emulatorsButton
            text: "Émulateurs"
            focus: true
            KeyNavigation.down: scraperButton
            Component.onCompleted: forceActiveFocus()
            background: Rectangle {
                color: emulatorsButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: emulatorsButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: ScreenManager.push("EmulatorManager")
        }
        Button {
            id: scraperButton
            text: "Scraper"
            KeyNavigation.up: emulatorsButton
            KeyNavigation.down: themeButton
            background: Rectangle {
                color: scraperButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: scraperButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: ScreenManager.push("ScraperManager")
        }
        Button {
            id: themeButton
            text: "Thème"
            KeyNavigation.up: scraperButton
            KeyNavigation.down: settingsButton
            background: Rectangle {
                color: themeButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: themeButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: ScreenManager.push("Theme")
        }
        Button {
            id: settingsButton
            text: "Réglages"
            KeyNavigation.up: themeButton
            KeyNavigation.down: restartButton
            background: Rectangle {
                color: settingsButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: settingsButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: ScreenManager.push("Settings")
        }
        Button {
            id: restartButton
            text: "Redémarrer le PC"
            KeyNavigation.up: settingsButton
            KeyNavigation.down: shutdownButton
            background: Rectangle {
                color: restartButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: restartButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: restartConfirmDialog.open()
        }
        Button {
            id: shutdownButton
            text: "Éteindre le PC"
            KeyNavigation.up: restartButton
            KeyNavigation.down: quitButton
            background: Rectangle {
                color: shutdownButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: shutdownButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: shutdownConfirmDialog.open()
        }
        Button {
            id: quitButton
            text: "Quitter l'application"
            KeyNavigation.up: shutdownButton
            background: Rectangle {
                color: quitButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: quitButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: quitConfirmDialog.open()
        }
    }

    Dialog {
        id: restartConfirmDialog
        anchors.centerIn: parent
        title: "Redémarrer le PC ?"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: SystemController.restartSystem()
    }

    Dialog {
        id: shutdownConfirmDialog
        anchors.centerIn: parent
        title: "Éteindre le PC ?"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: SystemController.shutdownSystem()
    }

    Dialog {
        id: quitConfirmDialog
        anchors.centerIn: parent
        title: "Quitter l'application ?"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: SystemController.quitApplication()
    }
}
