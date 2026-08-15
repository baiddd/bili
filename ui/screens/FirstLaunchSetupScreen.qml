import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    property string defaultRomsPath: applicationDirPath + "/ROMs"

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit * 2

        Text {
            text: "Où sont tes ROMs ?"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        Text {
            text: "Dossier proposé : " + defaultRomsPath
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
        }

        Row {
            spacing: Theme.spacingUnit
            Button {
                text: "Utiliser ce dossier"
                onClicked: {
                    RomSourcesStore.addSource(defaultRomsPath, "Principal")
                    ScreenManager.push("MainMenu")
                }
            }
            Button {
                text: "Choisir un autre dossier"
                onClicked: folderDialog.open()
            }
        }
    }

    FolderDialog {
        id: folderDialog
        onAccepted: {
            var localPath = selectedFolder.toString().replace(/^file:\/{2,3}/, "")
            RomSourcesStore.addSource(localPath, "Principal")
            ScreenManager.push("MainMenu")
        }
    }
}
