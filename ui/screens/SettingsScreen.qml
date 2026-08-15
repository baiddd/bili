import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingUnit * 2
        spacing: Theme.spacingUnit

        Text {
            text: "Réglages — Dossiers ROMs"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        ListView {
            id: romList
            width: parent.width
            height: 300
            model: RomSourcesStore.sources()
            delegate: Row {
                spacing: Theme.spacingUnit
                Text {
                    text: modelData.label + " — " + modelData.path
                    color: Theme.colorText
                    font.pixelSize: Theme.fontSizeBody
                }
                CheckBox {
                    checked: modelData.enabled
                    onToggled: {
                        RomSourcesStore.setEnabled(modelData.path, checked)
                        romList.model = RomSourcesStore.sources()
                    }
                }
                Button {
                    text: "Retirer"
                    onClicked: {
                        RomSourcesStore.removeSource(modelData.path)
                        romList.model = RomSourcesStore.sources()
                    }
                }
            }
        }

        Button {
            text: "Ajouter un dossier"
            onClicked: addFolderDialog.open()
        }

        Button {
            text: "Retour"
            onClicked: ScreenManager.pop()
        }
    }

    FolderDialog {
        id: addFolderDialog
        onAccepted: {
            var localPath = selectedFolder.toString().replace(/^file:\/{2,3}/, "")
            RomSourcesStore.addSource(localPath, "Dossier " + (RomSourcesStore.sources().length + 1))
        }
    }
}
