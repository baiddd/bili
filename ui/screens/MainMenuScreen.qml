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

        Button { text: "Émulateurs"; onClicked: ScreenManager.push("EmulatorManager") }
        Button { text: "Scraper"; onClicked: ScreenManager.push("ScraperManager") }
        Button { text: "Thème"; onClicked: ScreenManager.push("Theme") }
        Button { text: "Réglages"; onClicked: ScreenManager.push("Settings") }
        Button { text: "Redémarrer le PC"; onClicked: restartConfirmDialog.open() }
        Button { text: "Éteindre le PC"; onClicked: shutdownConfirmDialog.open() }
        Button { text: "Quitter l'application"; onClicked: quitConfirmDialog.open() }
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
