// ui/screens/InGameMenuOverlay.qml
//
// Contenu QML de la fenêtre de menu en jeu -- chargé dans un SECOND
// QQuickWindow (créé côté app/main.cpp, voir Task 4), pas dans la fenêtre
// principale de Bili : la fenêtre de RetroArch étant réattachée comme
// fenêtre enfant de celle de Bili, rien de ce que Main.qml dessine ne peut
// apparaître par-dessus (voir le spec de design, section Contexte). C'est
// GameMenuOverlay (app/GameMenuOverlay.h) qui place la fenêtre portant ce
// QML au-dessus de celle du jeu dans l'ordre d'empilement.
//
// Ce composant est un PANNEAU OPAQUE aux dimensions propres, pas un voile
// plein écran : la fenêtre native qui le porte est dimensionnée à ce
// panneau puis centrée dans la fenêtre de Bili, ce qui laisse l'image
// figée du jeu visible tout autour (exigence du spec). Un voile
// translucide plein écran a été essayé et mesuré en Task 3 : il ne
// fonctionne pas au-dessus du vrai RetroArch, qui dessine dans sa propre
// surface DWM avec laquelle l'alpha d'une fenêtre soeur ne se compose pas
// -- voir task-3-report.md et le commentaire d'en-tête de GameMenuOverlay.h.
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    // Taille du panneau : reprise telle quelle par app/main.cpp pour
    // dimensionner la fenêtre native du menu (Task 4).
    implicitWidth: 480
    implicitHeight: 260

    color: Theme.colorBackground
    border.color: Theme.colorAccent
    border.width: Theme.focusBorderWidth

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit * 3

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Jeu en pause"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        Button {
            id: quitGameButton
            anchors.horizontalCenter: parent.horizontalCenter
            width: 260
            text: "Quitter le jeu"
            focus: true
            Component.onCompleted: forceActiveFocus()
            contentItem: Text {
                text: quitGameButton.text
                color: Theme.colorText
                font.pixelSize: Theme.fontSizeBody
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: quitGameButton.activeFocus ? Theme.colorAccent : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: quitGameButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            Keys.onReturnPressed: quitGameButton.clicked()
            Keys.onEnterPressed: quitGameButton.clicked()
            onClicked: EmulatorProvider.quitGame()
        }
    }

    // Un moyen de fermer ce menu SANS quitter le jeu est requis par le spec
    // (section Architecture/Flux) : sans ce chemin de retour, Home serait un
    // aller simple. Réutilise le signal InputManager.cancel() déjà émis par
    // le bouton B/Circle de la manette (voir GamepadBridge.cpp) et par la
    // touche Échap au clavier (voir InputManager::handleKeyPress). Ce
    // Connections vit dans CETTE fenêtre de menu, pas dans Main.qml, donc
    // n'entre pas en conflit avec la navigation d'écran habituelle tant que
    // Main.qml lui-même est informé de ne pas aussi réagir pendant que ce
    // menu est ouvert (garde côté Main.qml, Task 4).
    Connections {
        target: InputManager
        function onCancel() { EmulatorProvider.resumeGame() }
    }
}
