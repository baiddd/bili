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
            focus: true
            keyNavigationEnabled: true
            highlightFollowsCurrentItem: true
            highlight: Rectangle { color: Theme.focusBorderColor; opacity: 0.5; border.color: Theme.focusBorderColor; border.width: Theme.focusBorderWidth }
            Component.onCompleted: forceActiveFocus()
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
            id: addFolderButton
            text: "Ajouter un dossier"
            KeyNavigation.up: romList
            background: Rectangle {
                color: addFolderButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: addFolderButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: addFolderDialog.open()
        }
    }

    FolderDialog {
        id: addFolderDialog
        onAccepted: {
            var localPath = RomSourcesStore.toLocalPath(selectedFolder)
            RomSourcesStore.addSource(localPath, "Dossier " + (RomSourcesStore.sources().length + 1))
            romList.model = RomSourcesStore.sources()
        }
    }
}
