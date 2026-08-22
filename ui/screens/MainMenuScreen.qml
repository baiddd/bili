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

    // Bug fix (manual testing, real DualSense controller): standardButtons'
    // auto-generated Yes/No buttons (1) have no KeyNavigation wired between
    // them and nothing gives either of them focus when the dialog opens --
    // so root.activeFocusItem stayed on whatever MainMenu Button was
    // focused BEFORE the dialog opened, and Main.qml's InputManager-driven
    // moveFocus()/onAccept kept operating on that background button instead
    // of the dialog -- and (2) are plain default-style Qt Quick Controls
    // buttons with no binding to Theme at all, unlike every other Button in
    // this app (background/border color tied to activeFocus), so even once
    // focus does move there's no visible indicator. Replacing
    // standardButtons with two hand-styled Buttons in a custom footer fixes
    // both: they follow this app's existing focus-styling convention
    // (background Rectangle bound to activeFocus) and get explicit
    // KeyNavigation.left/right, matching what moveFocus() expects
    // everywhere else in this codebase. background/header are also
    // reskinned so the popup matches Bili's dark theme instead of the
    // platform-default light dialog chrome.
    Dialog {
        id: restartConfirmDialog
        anchors.centerIn: parent
        modal: true
        focus: true
        background: Rectangle { color: Theme.colorBackground; border.color: Theme.focusBorderColor; border.width: 1 }
        header: Text {
            text: "Redémarrer le PC ?"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
            padding: Theme.spacingUnit
        }
        footer: Row {
            spacing: Theme.spacingUnit
            padding: Theme.spacingUnit
            anchors.right: parent.right
            Button {
                id: restartNoButton
                text: "Non"
                KeyNavigation.right: restartYesButton
                // Bug fix (manual testing): this footer's buttons live under
                // the Popup's own Overlay.overlay branch, not under Main.qml's
                // root Item -- Enter/Return bubbling never reaches that
                // Item's global Keys.onPressed/InputManager routing from
                // here, so keyboard Enter must be handled locally. Left/Right
                // arrow navigation is unaffected since it's native
                // KeyNavigation, not routed through InputManager either.
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                background: Rectangle {
                    color: restartNoButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: restartNoButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: restartConfirmDialog.reject()
            }
            Button {
                id: restartYesButton
                text: "Oui"
                KeyNavigation.left: restartNoButton
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                background: Rectangle {
                    color: restartYesButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: restartYesButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: restartConfirmDialog.accept()
            }
        }
        onOpened: restartNoButton.forceActiveFocus()
        onAccepted: SystemController.restartSystem()
    }

    Dialog {
        id: shutdownConfirmDialog
        anchors.centerIn: parent
        modal: true
        focus: true
        background: Rectangle { color: Theme.colorBackground; border.color: Theme.focusBorderColor; border.width: 1 }
        header: Text {
            text: "Éteindre le PC ?"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
            padding: Theme.spacingUnit
        }
        footer: Row {
            spacing: Theme.spacingUnit
            padding: Theme.spacingUnit
            anchors.right: parent.right
            Button {
                id: shutdownNoButton
                text: "Non"
                KeyNavigation.right: shutdownYesButton
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                background: Rectangle {
                    color: shutdownNoButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: shutdownNoButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: shutdownConfirmDialog.reject()
            }
            Button {
                id: shutdownYesButton
                text: "Oui"
                KeyNavigation.left: shutdownNoButton
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                background: Rectangle {
                    color: shutdownYesButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: shutdownYesButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: shutdownConfirmDialog.accept()
            }
        }
        onOpened: shutdownNoButton.forceActiveFocus()
        onAccepted: SystemController.shutdownSystem()
    }

    Dialog {
        id: quitConfirmDialog
        anchors.centerIn: parent
        modal: true
        focus: true
        background: Rectangle { color: Theme.colorBackground; border.color: Theme.focusBorderColor; border.width: 1 }
        header: Text {
            text: "Quitter l'application ?"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
            padding: Theme.spacingUnit
        }
        footer: Row {
            spacing: Theme.spacingUnit
            padding: Theme.spacingUnit
            anchors.right: parent.right
            Button {
                id: quitNoButton
                text: "Non"
                KeyNavigation.right: quitYesButton
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                background: Rectangle {
                    color: quitNoButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: quitNoButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: quitConfirmDialog.reject()
            }
            Button {
                id: quitYesButton
                text: "Oui"
                KeyNavigation.left: quitNoButton
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                background: Rectangle {
                    color: quitYesButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: quitYesButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: quitConfirmDialog.accept()
            }
        }
        onOpened: quitNoButton.forceActiveFocus()
        onAccepted: SystemController.quitApplication()
    }
}
