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
// LE VOILE. L'image figée du jeu affichée ici n'est PAS la fenêtre de
// RetroArch vue par transparence -- ça ne marche pas au-dessus de
// RetroArch (voir GameMenuOverlay.h et task-3-report.md). C'est une
// CAPTURE de sa dernière image, prise par GameMenuOverlay::show() et
// publiée en contenu QML via GameFrameImageProvider. Le voile
// semi-transparent et le panneau sont donc composés par Qt lui-même,
// par-dessus cette image, dans une seule et même surface.
//
// Si la capture échoue, GameMenuOverlay se replie sur un panneau opaque
// centré (hasGameFrame devient false) et le jeu reste visible autour du
// panneau plutôt qu'à travers le voile.
import QtQuick
import QtQuick.Controls
import Bili

Item {
    // Taille du panneau de repli : reprise par app/main.cpp pour
    // dimensionner la fenêtre native du menu au premier affichage
    // (GameMenuOverlay la mémorise et l'agrandit ensuite à toute la zone
    // cliente quand une capture est disponible).
    implicitWidth: 480
    implicitHeight: 260

    // Cette fenêtre de menu (voir app/main.cpp, Task 4) reçoit le focus
    // clavier Win32 natif dès que GameMenuOverlay::show() l'affiche (elle
    // appelle SetFocus() sur son HWND) -- Main.qml, qui vit dans la fenêtre
    // PRINCIPALE de Bili, ne reçoit donc plus aucun évènement clavier tant
    // que ce menu est ouvert, et son propre Keys.onPressed (qui relaie vers
    // InputManager.handleKeyPress()) ne voit jamais passer l'Échap.
    // Reproduit ici le même relais, sinon la touche Échap au clavier
    // resterait sans effet (constaté à la vérification manuelle de cette
    // tâche) alors que le bouton B/Circle d'une manette, lui, fonctionne
    // déjà : GamepadBridge lit la manette sur son propre thread de polling
    // SDL, indépendamment du focus de fenêtre. Sans incidence sur le bouton
    // "Quitter le jeu" : Keys.onReturnPressed dessus consomme déjà Entrée
    // avant qu'elle ne remonte jusqu'ici.
    focus: true
    Keys.onPressed: (event) => {
        InputManager.handleKeyPress(event.key)
        event.accepted = true
    }

    // L'image figée du jeu. La révision dans l'URL force le rechargement à
    // chaque ouverture du menu -- sans elle, QML réafficherait la capture
    // précédente.
    Image {
        anchors.fill: parent
        visible: GameMenuOverlay.hasGameFrame
        source: "image://gameframe/" + GameMenuOverlay.frameRevision
        cache: false
        // La capture fait exactement la taille de la fenêtre du jeu, qui
        // remplit déjà la zone cliente de l'hôte : même géométrie que cette
        // fenêtre de menu, donc pas de déformation.
        fillMode: Image.Stretch
    }

    // LE VOILE : c'est lui qui laisse voir le jeu à travers le menu.
    // Attention au format : QML lit les littéraux couleur en #AARRGGBB, pas
    // en #RRGGBBAA comme le CSS. "#00000099" (la valeur d'origine du design)
    // vaut donc « bleu totalement transparent » et ne masque rien du tout --
    // constaté à l'écran pendant la vérification (Task 3). Ici : noir à 60%.
    Rectangle {
        anchors.fill: parent
        visible: GameMenuOverlay.hasGameFrame
        color: "#99000000"
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, 480)
        height: Math.min(parent.height, 260)
        color: Theme.colorBackground
        border.color: Theme.colorAccent
        border.width: Theme.focusBorderWidth
        radius: GameMenuOverlay.hasGameFrame ? Theme.focusRadius * 2 : 0

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
    // IMPORTANT : ce Connections est VIVANT dès le chargement de ce QML par
    // app/main.cpp (au démarrage de l'appli, pour préparer la fenêtre de
    // menu à l'avance -- voir main.cpp), pas seulement pendant que le menu
    // est visuellement ouvert. InputManager.accept()/cancel() sont donc reçus
    // ici en permanence, y compris quand aucun jeu n'est en cours. Chaque
    // handler doit donc se garder lui-même avec isGameMenuOpen() plutôt que
    // de compter sur le no-op interne de resumeGame()/quitGame() -- trouvé de
    // la manière la plus concrète possible (Task 4 review fix-wave) :
    // ajouter onAccept() sans cette garde faisait qu'un Accept sur
    // GameList (aucun menu ouvert) déclenchait `Main.qml`'s onAccept
    // (lance le jeu) PUIS, dans la même émission synchrone du signal,
    // celui-ci (quitGameButton.clicked() -> EmulatorProvider.quitGame()),
    // qui tuait le jeu qui venait tout juste d'être lancé -- reproduit et
    // confirmé par vérification manuelle avant d'être corrigé.
    Connections {
        target: InputManager
        function onCancel() {
            if (!EmulatorProvider.isGameMenuOpen()) return
            EmulatorProvider.resumeGame()
        }
        // Sans ceci, le bouton Accept/A d'une manette (qui n'émet que
        // InputManager.accept(), voir GamepadBridge.cpp -- pas d'évènement
        // clavier synthétique) n'avait aucun moyen d'atteindre "Quitter le
        // jeu" : seul un clic souris ou une touche Entrée/Return livrée avec
        // le focus clavier RÉEL sur ce bouton (peu fiable une fois le jeu en
        // pause, RetroArch reprenant parfois le focus Win32 de lui-même,
        // voir task-4-report.md) le déclenchaient. Trouvé en revue de code
        // (Task 4 fix wave), pas repéré à l'ouverture de cette tâche.
        function onAccept() {
            if (!EmulatorProvider.isGameMenuOpen()) return
            quitGameButton.clicked()
        }
    }
}
