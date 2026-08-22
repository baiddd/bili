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
                id: useDefaultFolderButton
                text: "Utiliser ce dossier"
                focus: true
                KeyNavigation.right: chooseFolderButton
                Component.onCompleted: forceActiveFocus()
                background: Rectangle {
                    color: useDefaultFolderButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: useDefaultFolderButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: {
                    if (RomSourcesStore.pathExists(defaultRomsPath)) {
                        RomSourcesStore.addSource(defaultRomsPath, "Principal")
                        ScreenManager.push("GameList")
                    } else {
                        createFolderConfirmDialog.open()
                    }
                }
            }
            Button {
                id: chooseFolderButton
                text: "Choisir un autre dossier"
                KeyNavigation.left: useDefaultFolderButton
                background: Rectangle {
                    color: chooseFolderButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                    border.color: Theme.focusBorderColor
                    border.width: chooseFolderButton.activeFocus ? Theme.focusBorderWidth : 1
                    radius: Theme.focusRadius
                }
                onClicked: folderDialog.open()
            }
        }
    }

    Dialog {
        id: createFolderConfirmDialog
        title: "Dossier introuvable"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No

        Text {
            text: "Ce dossier n'existe pas. Voulez-vous le créer ?"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
        }

        onAccepted: {
            if (RomSourcesStore.createPath(defaultRomsPath)) {
                RomSourcesStore.addSource(defaultRomsPath, "Principal")
                ScreenManager.push("GameList")
            } else {
                createFolderFailedDialog.open()
            }
        }
        onRejected: chooseFolderButton.forceActiveFocus()
    }

    Dialog {
        id: createFolderFailedDialog
        title: "Échec de la création"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok

        Text {
            text: "Impossible de créer ce dossier. Choisis un autre dossier."
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
        }

        onAccepted: chooseFolderButton.forceActiveFocus()
    }

    FolderDialog {
        id: folderDialog
        onAccepted: {
            var localPath = RomSourcesStore.toLocalPath(selectedFolder)
            RomSourcesStore.addSource(localPath, "Principal")
            ScreenManager.push("GameList")
        }
    }
}
