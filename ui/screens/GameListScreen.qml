import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Text {
        id: statusText
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: Theme.spacingUnit
        color: Theme.colorText
        font.pixelSize: Theme.fontSizeBody
        visible: text.length > 0
    }

    Text {
        anchors.centerIn: parent
        text: "Aucun jeu trouvé — vérifie tes dossiers ROMs dans Réglages."
        color: Theme.colorText
        font.pixelSize: Theme.fontSizeBody
        visible: gameGrid.count === 0 && !statusText.visible
    }

    GridView {
        id: gameGrid
        anchors.fill: parent
        anchors.topMargin: Theme.spacingUnit * 4
        anchors.margins: Theme.spacingUnit * 2
        cellWidth: 200
        cellHeight: 60
        model: LibraryModel
        focus: true
        keyNavigationEnabled: true
        highlightFollowsCurrentItem: true
        Component.onCompleted: forceActiveFocus()
        delegate: Rectangle {
            id: gameDelegate
            // GridView keeps activeFocus on the view itself, not on
            // individual delegates (none of which set focus: true), so
            // gameDelegate.activeFocus would never be true here. The
            // correct signal for "is this the currently-navigated cell" is
            // the view's isCurrentItem attached property (QQuickItemView,
            // shared base of GridView/ListView) -- same status
            // SettingsScreen's romList ListView highlight already tracks
            // via highlightFollowsCurrentItem.
            property bool highlighted: GridView.isCurrentItem
            property string romPath: model.romPath
            property string system: model.system
            property string gameTitle: model.title
            width: gameGrid.cellWidth - Theme.spacingUnit
            height: gameGrid.cellHeight - Theme.spacingUnit
            color: highlighted ? Theme.focusBorderColor : "#22222a"
            border.color: Theme.focusBorderColor
            border.width: highlighted ? Theme.focusBorderWidth : 0
            radius: Theme.focusRadius

            Text {
                anchors.centerIn: parent
                anchors.margins: Theme.spacingUnit
                text: model.title
                color: Theme.colorText
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
                width: parent.width - Theme.spacingUnit * 2
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Connections {
        target: LibraryScanner
        function onScanStarted() { statusText.text = "Scan en cours..." }
        function onSourceScanned(path, filesFound) {
            statusText.text = "Scan en cours... (" + filesFound + " trouvés dans " + path + ")"
        }
        function onScanFinished(totalFilesFound) {
            statusText.text = ""
        }
    }

    // ui/Main.qml's onAccept() GameList branch calls
    // EmulatorProvider.launchGame() directly once a core is installed, but
    // never checks isRetroArchInstalled() first - launchGame() itself
    // correctly emits launchFailed(...) in that case (e.g. RetroArch missing,
    // or the launch otherwise failing to start), but without this
    // Connections block nothing was listening for it, so pressing Accept
    // silently did nothing visible. Reuse statusText, same as the
    // LibraryScanner handlers above; it naturally gets overwritten by the
    // next scan message or cleared on the next scan finishing.
    Connections {
        target: EmulatorProvider
        function onLaunchFailed(errorString) {
            statusText.text = "Erreur : " + errorString
        }
    }
}
