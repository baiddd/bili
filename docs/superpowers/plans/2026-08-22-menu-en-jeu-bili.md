# Menu en jeu Bili (bouton Home) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Appuyer sur le bouton Home/Guide de la manette pendant une partie met le jeu en pause et affiche un menu Bili (bouton "Quitter le jeu") par-dessus l'image figée du jeu.

**Architecture:** `GamepadBridge` détecte le bouton Home et émet un nouveau signal `InputManager`. `RetroArchNetworkCommand` pilote RetroArch à distance (pause/quit) via son interface de commandes réseau UDP officielle. `GameMenuOverlay` réattache un second `QQuickWindow` (le menu Bili) comme fenêtre sœur de celle de RetroArch, au-dessus dans l'ordre d'empilement, réutilisant `GameWindowEmbedder`'s technique de réattachement Win32.

**Tech Stack:** Qt6 (Core/Gui/Network/Quick), API Win32 (`<windows.h>`), `QUdpSocket`, QtTest.

**Spec:** `docs/superpowers/specs/2026-08-22-menu-en-jeu-bili-design.md`

## Global Constraints

- Aucune dépendance QtQuick ajoutée à `core/` au-delà de ce qui y est déjà (Qt6::Gui pour `WId`) — toute manipulation de `QQuickWindow` réelle reste dans `app/`.
- Fonctionnalité strictement Windows, comme le reste de l'intégration de fenêtre de jeu — code Win32 réel sous `#ifdef Q_OS_WIN`.
- Le menu n'a d'effet que si un jeu est actuellement lancé/intégré (`EmulatorProvider` sait déjà si un jeu tourne via `m_gameProcess`) ; sans jeu en cours, le bouton Home ne fait rien en V1.
- Format du protocole de commandes réseau RetroArch **confirmé par une source primaire réelle** (`docs.libretro.com/development/retroarch/network-control-interface/`, vérifié pendant le brainstorming) : commandes texte ASCII, une par ligne (terminées par `\n`), envoyées en UDP sur le port configuré via `network_cmd_port` (défaut RetroArch : `55355`). Commandes utilisées ici, confirmées sur cette même page : `PAUSE_TOGGLE`, `QUIT`.

---

## Task 1: Bouton Home/Guide manette → signal `InputManager`

**Files:**
- Modify: `core/input/InputManager.h`
- Modify: `core/input/GamepadBridge.cpp`

**Interfaces:**
- Produces: `InputManager::homeMenuRequested()` — nouveau signal, aucun paramètre.

### Step 1: Ajouter le signal à `InputManager.h`

Ajouter dans la section `signals:` (à la suite de `capture()`) :
```cpp
    void homeMenuRequested();
```

### Step 2: Émettre ce signal sur le bouton Guide dans `GamepadBridge.cpp`

Dans le `switch (event.cbutton.button)` existant (voir le cas
`SDL_CONTROLLER_BUTTON_BACK` juste avant `default:`), ajouter :
```cpp
                    case SDL_CONTROLLER_BUTTON_GUIDE:
                        emit m_inputManager->homeMenuRequested(); break;
```

### Step 3: Compiler et lancer la suite de tests

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: tous les tests passent (ce changement n'a pas de test dédié —
même limite que les autres boutons déjà gérés dans ce fichier, qui n'ont
pas non plus de test unitaire par bouton ; `GamepadBridgeTest.cpp`
existant ne fait que vérifier le démarrage/arrêt sans matériel réel).

### Step 4: Commit

```bash
git add core/input/InputManager.h core/input/GamepadBridge.cpp
git commit -m "feat: emit homeMenuRequested on the gamepad's Guide/Home button"
```

---

## Task 2: `RetroArchNetworkCommand` (commandes réseau UDP)

**Files:**
- Create: `core/emulators/RetroArchNetworkCommand.h`
- Create: `core/emulators/RetroArchNetworkCommand.cpp`
- Modify: `core/CMakeLists.txt` (ajouter la nouvelle source)
- Modify: `core/emulators/EmulatorProvider.cpp` (`writePortableRetroArchConfig()` : activer l'interface réseau)
- Test: `tests/emulators/RetroArchNetworkCommandTest.cpp`
- Modify: `tests/emulators/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `class RetroArchNetworkCommand` avec `bool send(const QString &command, quint16 port = kDefaultPort)` — envoie `command + "\n"` en UDP sur `127.0.0.1:port`, retourne `true` si l'envoi (pas la réception/l'exécution côté RetroArch, non confirmable de façon fiable via ce protocole fire-and-forget) a réussi.
  - `static constexpr quint16 RetroArchNetworkCommand::kDefaultPort = 55355;`

### Step 1: Écrire l'en-tête

```cpp
// core/emulators/RetroArchNetworkCommand.h
#pragma once
#include <QString>
#include <QtGlobal>

// Envoie une commande texte à une instance RetroArch en cours d'exécution
// via son interface de commandes réseau officielle (UDP, protocole ASCII
// une commande par ligne) -- confirmée sur
// docs.libretro.com/development/retroarch/network-control-interface/
// pendant le brainstorming, pas devinée. RetroArch doit avoir
// network_cmd_enable=true et network_cmd_port fixé dans son retroarch.cfg
// (voir EmulatorProvider::writePortableRetroArchConfig()) pour écouter.
class RetroArchNetworkCommand {
public:
    static constexpr quint16 kDefaultPort = 55355;

    // Envoie command (ex. "PAUSE_TOGGLE", "QUIT") en UDP vers
    // 127.0.0.1:port. Retourne true si l'envoi local a réussi (le
    // protocole est fire-and-forget -- aucune confirmation que RetroArch
    // a effectivement reçu/exécuté la commande n'est possible par ce
    // canal).
    bool send(const QString &command, quint16 port = kDefaultPort);
};
```

### Step 2: Écrire le test (rouge attendu, `.cpp` pas encore écrit)

```cpp
// tests/emulators/RetroArchNetworkCommandTest.cpp
#include <QTest>
#include <QUdpSocket>
#include "emulators/RetroArchNetworkCommand.h"

class RetroArchNetworkCommandTest : public QObject {
    Q_OBJECT
private slots:
    void sendDeliversNewlineTerminatedCommandOverUdp() {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        const quint16 port = receiver.localPort();

        RetroArchNetworkCommand command;
        QVERIFY(command.send("PAUSE_TOGGLE", port));

        QVERIFY(receiver.waitForReadyRead(2000));
        QByteArray datagram;
        datagram.resize(static_cast<int>(receiver.pendingDatagramSize()));
        receiver.readDatagram(datagram.data(), datagram.size());

        QCOMPARE(datagram, QByteArray("PAUSE_TOGGLE\n"));
    }

    void sendUsesDefaultPortWhenNoneGiven() {
        QCOMPARE(RetroArchNetworkCommand::kDefaultPort, static_cast<quint16>(55355));
    }
};

QTEST_MAIN(RetroArchNetworkCommandTest)
#include "RetroArchNetworkCommandTest.moc"
```

Run: `cmake --build build\windows-portable --target RetroArchNetworkCommandTest`
Expected: échec de compilation (`RetroArchNetworkCommand.cpp` n'existe pas
encore) -- confirme le câblage avant l'implémentation.

### Step 3: Implémenter

```cpp
// core/emulators/RetroArchNetworkCommand.cpp
#include "RetroArchNetworkCommand.h"
#include <QUdpSocket>
#include <QHostAddress>

bool RetroArchNetworkCommand::send(const QString &command, quint16 port) {
    QUdpSocket socket;
    const QByteArray payload = (command + "\n").toUtf8();
    const qint64 written = socket.writeDatagram(payload, QHostAddress::LocalHost, port);
    return written == payload.size();
}
```

### Step 4: Ajouter la source au build de `core/`

Modifier `core/CMakeLists.txt`, ajouter
`emulators/RetroArchNetworkCommand.cpp` à `target_sources(bili-core PRIVATE ...)`.

### Step 5: Câbler la cible de test

Modifier `tests/emulators/CMakeLists.txt`, ajouter à la suite des cibles existantes :
```cmake
qt_add_executable(RetroArchNetworkCommandTest RetroArchNetworkCommandTest.cpp)
target_link_libraries(RetroArchNetworkCommandTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME RetroArchNetworkCommandTest COMMAND RetroArchNetworkCommandTest)
```

### Step 6: Lancer les tests, vérifier qu'ils passent

Run: `ctest --test-dir build\windows-portable -R RetroArchNetworkCommandTest --output-on-failure`
Expected: `100% tests passed` (2/2 cas).

### Step 7: Activer l'interface réseau dans le `retroarch.cfg` généré

Dans `core/emulators/EmulatorProvider.cpp`, fonction
`writePortableRetroArchConfig()`, ajouter à la suite des lignes existantes
(après `input_quit_gamepad_combo`, déjà présent depuis le fix de session
précédente) :
```cpp
    out << "network_cmd_enable = \"true\"\n";
    out << "network_cmd_port = \"" << RetroArchNetworkCommand::kDefaultPort << "\"\n";
```
Ajouter `#include "RetroArchNetworkCommand.h"` en haut du fichier si pas
déjà présent (probablement pas, cette classe est nouvelle).

### Step 8: Lancer la suite complète, vérifier l'absence de régression

Run: `ctest --test-dir build\windows-portable --output-on-failure`
Expected: tous les tests au vert.

### Step 9: Vérification manuelle réelle

Installer un vrai RetroArch (si pas déjà fait), lancer un vrai jeu depuis
Bili, puis depuis un terminal externe envoyer manuellement
`echo PAUSE_TOGGLE | ncat -u 127.0.0.1 55355` (ou tout outil UDP
équivalent disponible) et confirmer que le jeu se met effectivement en
pause dans RetroArch. Documenter le résultat dans le rapport -- si la
commande ne produit pas l'effet attendu, c'est un signal à faire remonter
avant de continuer sur les tâches suivantes (ne pas deviner un nom de
commande alternatif sans revérifier la documentation).

### Step 10: Commit

```bash
git add core/emulators/RetroArchNetworkCommand.h core/emulators/RetroArchNetworkCommand.cpp \
        core/CMakeLists.txt core/emulators/EmulatorProvider.cpp \
        tests/emulators/RetroArchNetworkCommandTest.cpp tests/emulators/CMakeLists.txt
git commit -m "feat: add RetroArchNetworkCommand and enable RetroArch's network command interface"
```

---

## Task 3: `GameMenuOverlay` — fenêtre de menu Bili réattachée au-dessus du jeu

**Cette tâche comporte une inconnue technique réelle** (jamais tentée dans
ce projet) : faire coexister deux `QQuickWindow` réattachés comme enfants
de la même fenêtre hôte, et amener celui du menu au-dessus de celui de
RetroArch dans l'ordre d'empilement, tout en gérant correctement le focus
clavier/manette entre les deux. Le design ci-dessous est une proposition
de départ, pas une certitude -- **vérifier empiriquement chaque
affirmation marquée comme telle avant de considérer cette tâche
terminée**, et escalader (statut `BLOCKED`, pas une supposition) si le
comportement réel diverge de ce qui est décrit ici.

**Files:**
- Create: `core/emulators/GameMenuOverlay.h`
- Create: `core/emulators/GameMenuOverlay.cpp`
- Modify: `core/CMakeLists.txt`
- Create: `ui/screens/InGameMenuOverlay.qml` (contenu minimal : un bouton "Quitter le jeu")
- Modify: `tests/emulators/CMakeLists.txt` / nouveau fichier de test si une partie est isolément testable (voir Step 6)

**Interfaces:**
- Consumes: même mécanique de réattachement que `GameWindowEmbedder` (Task 1 du sous-projet précédent) -- lire `core/emulators/GameWindowEmbedder.cpp` avant de commencer, pour réutiliser exactement le même style (`SetParent`, `GetClientRect`+`MoveWindow`, gestion d'erreur qui retourne `false` sans crasher).
- Produces:
  - `class GameMenuOverlay` :
    - `bool show(QQuickWindow *menuWindow, WId hostWindowId, WId gameWindowId)` -- réattache `menuWindow` (déjà créé et chargé avec son QML, voir Step 2) comme enfant de `hostWindowId`, le redimensionne pour couvrir la même zone que `gameWindowId` (ou toute la zone cliente de l'hôte -- à trancher pendant l'implémentation selon ce qui rend le mieux visuellement), et le place au-dessus de `gameWindowId` dans l'ordre d'empilement des fenêtres sœurs (`SetWindowPos` avec `HWND_TOP` ou équivalent -- vérifier quelle constante/appel garantit réellement ce résultat plutôt que de deviner). Retourne `false` sur tout échec.
    - `void hide(QQuickWindow *menuWindow)` -- cache la fenêtre de menu (ne la détruit pas forcément -- à décider selon si le menu doit persister entre deux ouvertures ou être recréé à chaque fois).

### Step 1: Lire le code existant

Lire en entier `core/emulators/GameWindowEmbedder.h`/`.cpp` (le
réattachement de la fenêtre de RetroArch) et `app/main.cpp` (comment
`EmulatorProvider::setHostWindowId` reçoit le `WId` de la fenêtre
principale de Bili aujourd'hui). Cette tâche doit récupérer le `WId` de la
fenêtre RetroArch actuellement intégrée pour son propre positionnement --
vérifier si `EmulatorProvider`/`GameWindowEmbedder` exposent déjà ce `WId`
quelque part (probablement pas encore -- si c'est le cas, ce sera un ajout
mineur à faire remonter, à documenter dans le rapport plutôt que deviné en
silence).

### Step 2: Écrire le QML minimal du menu

```qml
// ui/screens/InGameMenuOverlay.qml
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: "#00000099" // semi-transparent, laisse voir l'image du jeu figée en dessous

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit

        Button {
            id: quitGameButton
            text: "Quitter le jeu"
            focus: true
            Component.onCompleted: forceActiveFocus()
            background: Rectangle {
                color: quitGameButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: quitGameButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            onClicked: EmulatorProvider.quitGame()
        }
    }

    // Requis par le spec (section Architecture/Flux) : un moyen de fermer
    // ce menu SANS quitter le jeu, sous peine de bloquer l'utilisateur
    // (Home est un aller simple sans ce chemin de retour). Réutilise le
    // signal InputManager.cancel() déjà émis par le bouton B/Circle de la
    // manette (voir GamepadBridge.cpp) et la touche Échap au clavier (voir
    // InputManager::handleKeyPress) -- ce Connections vit dans CETTE
    // fenêtre de menu, pas dans Main.qml, donc n'entre pas en conflit avec
    // la navigation d'écran habituelle tant que Main.qml lui-même est
    // informé de ne pas aussi réagir pendant que ce menu est ouvert (voir
    // Task 4 Step 4bis, garde côté Main.qml).
    Connections {
        target: InputManager
        function onCancel() { EmulatorProvider.resumeGame() }
    }
}
```

Note : `EmulatorProvider.quitGame()`/`::resumeGame()` sont ajoutés dans la
Task 4 -- ce fichier QML seul ne compile/ne fonctionne pas tant que ces
méthodes n'existent pas, c'est attendu à ce stade (voir l'ordre des
tâches).

À ajouter à `tests/CMakeLists.txt`'s équivalent `app/CMakeLists.txt`'s
`BILI_QML_FILES` (voir ce fichier existant) une fois cette tâche
intégrée -- **cette étape appartient à la Task 4** (câblage complet),
notée ici pour mémoire seulement.

### Step 3: Écrire l'en-tête `GameMenuOverlay.h`

```cpp
// core/emulators/GameMenuOverlay.h
#pragma once
#include <qwindowdefs.h> // WId
class QQuickWindow;

// Réattache une fenêtre de menu Bili (un QQuickWindow créé et possédé par
// l'appelant, voir app/main.cpp) comme fenêtre soeur de celle du jeu
// actuellement intégré (GameWindowEmbedder), au-dessus dans l'ordre
// d'empilement -- pas une fenêtre séparée flottante. Fonctionnalité
// strictement Windows, comme GameWindowEmbedder (no-op ailleurs).
class GameMenuOverlay {
public:
    bool show(QQuickWindow *menuWindow, WId hostWindowId, WId gameWindowId);
    void hide(QQuickWindow *menuWindow);
};
```

### Step 4: Implémenter (proposition de départ -- à vérifier empiriquement)

```cpp
// core/emulators/GameMenuOverlay.cpp
#include "GameMenuOverlay.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <QQuickWindow>

bool GameMenuOverlay::show(QQuickWindow *menuWindow, WId hostWindowId, WId gameWindowId) {
    if (!menuWindow) return false;

    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);
    const HWND gameHwnd = reinterpret_cast<HWND>(gameWindowId);
    const HWND menuHwnd = reinterpret_cast<HWND>(menuWindow->winId());

    // VÉRIFIER EMPIRIQUEMENT : SetParent sur la fenêtre native d'un
    // QQuickWindow fonctionne-t-il de la même façon que sur la fenêtre
    // d'un process tiers (GameWindowEmbedder) ? Un QQuickWindow gère son
    // propre rendu OpenGL/D3D -- réattacher sa fenêtre native ne devrait
    // pas perturber ce rendu (il continue de peindre sa propre surface),
    // mais ce n'est pas vérifié dans ce projet avant cette tâche.
    if (SetParent(menuHwnd, hostHwnd) == nullptr) return false;

    RECT clientRect;
    if (!GetClientRect(hostHwnd, &clientRect)) return false;
    MoveWindow(menuHwnd, 0, 0,
               clientRect.right - clientRect.left,
               clientRect.bottom - clientRect.top,
               TRUE);

    // VÉRIFIER EMPIRIQUEMENT : HWND_TOP place-t-il bien menuHwnd au-dessus
    // de gameHwnd dans l'ordre d'empilement des fenêtres soeurs sous le
    // même parent, ou faut-il positionner explicitement relatif à
    // gameHwnd (ex. SetWindowPos(menuHwnd, gameHwnd, ...) avec un ordre
    // précis, une valeur HWND_TOP plaçant seulement en tête de la liste
    // globale des enfants) ? gameHwnd est passé à cette fonction
    // précisément pour permettre un positionnement relatif si HWND_TOP
    // seul ne suffit pas -- ajuster ce step selon le résultat réel observé.
    SetWindowPos(menuHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(menuHwnd, SW_SHOW);
    SetFocus(menuHwnd);
    return true;
}

void GameMenuOverlay::hide(QQuickWindow *menuWindow) {
    if (!menuWindow) return;
    ShowWindow(reinterpret_cast<HWND>(menuWindow->winId()), SW_HIDE);
}

#else // !Q_OS_WIN

bool GameMenuOverlay::show(QQuickWindow *, WId, WId) { return false; }
void GameMenuOverlay::hide(QQuickWindow *) {}

#endif
```

Cette classe a besoin de `QQuickWindow` (module QtQuick) -- contrairement
à `GameWindowEmbedder`, qui n'en a pas besoin puisqu'il ne manipule que des
`WId`/`HWND` bruts. `GameMenuOverlay` manipule directement un
`QQuickWindow*` (créé côté `app/`, voir Task 4), donc cette classe **ne
peut pas vivre dans `core/` sans y introduire une dépendance QtQuick** --
point à trancher au moment de l'implémentation : soit ajouter Qt6::Quick à
`bili-core` (déviation du constraint existant, à documenter et faire
remonter explicitement plutôt qu'à faire silencieusement), soit déplacer
cette classe dans `app/` à la place de `core/emulators/` (probablement
préférable, cohérent avec pourquoi `app/main.cpp` récupère lui-même le
`WId` de la fenêtre racine dans le sous-projet précédent plutôt que de le
faire depuis `core/`). **Ne pas trancher silencieusement en copiant le
chemin `core/emulators/` proposé ci-dessus si Qt6::Quick doit être ajouté
à `bili-core` pour que ça compile -- escalader ce choix dans le rapport.**

### Step 5: Compiler et vérifier l'emplacement retenu

Run: `cmake --build build\windows-portable`
Expected : compile sans ajouter Qt6::Quick à `bili-core` (déplacer le
fichier vers `app/` si nécessaire, voir Step 4) -- documenter le chemin
final réellement utilisé dans le rapport s'il diffère de
`core/emulators/GameMenuOverlay.{h,cpp}` proposé plus haut.

### Step 6: Vérification manuelle réelle (le seul moyen fiable ici)

Cette classe manipule deux fenêtres natives réelles simultanément -- pas
raisonnablement testable en automatique sans un vrai environnement de
fenêtrage Windows actif (contrairement à `GameWindowEmbedderTest`, qui
peut au moins créer un `QWindow` hôte de test ; ici il faudrait EN PLUS un
vrai `QQuickWindow` avec un moteur QML chargé, plus lourd à mettre en
place pour un gain de couverture incertain vu l'aspect purement visuel de
ce qui est vérifié). Skip le test automatisé pour cette tâche seule (sera
couvert par la vérification manuelle de bout en bout en Task 4) --
signaler ce choix dans le rapport plutôt que le passer sous silence.

### Step 7: Commit

```bash
git add <fichiers réellement créés selon l'emplacement retenu au Step 4> \
        ui/screens/InGameMenuOverlay.qml
git commit -m "feat: add GameMenuOverlay to reattach a Bili menu window above the embedded game"
```

---

## Task 4: Câblage complet (Home → pause+menu, Quitter → QUIT+cleanup)

**Files:**
- Modify: `core/emulators/EmulatorProvider.h`
- Modify: `core/emulators/EmulatorProvider.cpp`
- Modify: `app/main.cpp`
- Modify: `app/CMakeLists.txt` (ajouter `ui/screens/InGameMenuOverlay.qml` à `BILI_QML_FILES`)

**Interfaces:**
- Consumes: `InputManager::homeMenuRequested()` (Task 1), `RetroArchNetworkCommand::send()` (Task 2), `GameMenuOverlay::show()`/`::hide()` (Task 3).
- Produces:
  - `EmulatorProvider::openGameMenu()` (`Q_INVOKABLE`) -- appelé depuis `app/main.cpp` sur `homeMenuRequested()`. No-op si aucun jeu n'est en cours ou si le menu est déjà ouvert. Envoie `PAUSE_TOGGLE`, passe `m_gameMenuOpen` à `true`, émet `gameMenuOpened()`.
  - `EmulatorProvider::quitGame()` (`Q_INVOKABLE`) -- appelé depuis `ui/screens/InGameMenuOverlay.qml`. Émet aussi `gameMenuClosed()`.
  - `EmulatorProvider::resumeGame()` (`Q_INVOKABLE`) -- ferme le menu et relance l'exécution du jeu (`PAUSE_TOGGLE` à nouveau) ; appelé depuis `ui/screens/InGameMenuOverlay.qml` sur Cancel/B/Échap. Émet `gameMenuClosed()`.
  - `EmulatorProvider::isGameMenuOpen()` (`Q_INVOKABLE`) -- lu par `Main.qml` pour ne pas dupliquer la navigation d'écran habituelle pendant que ce menu est affiché (voir Step 4bis).
  - Signaux `gameMenuOpened()`/`gameMenuClosed()` -- `app/main.cpp` s'y connecte pour appeler `GameMenuOverlay::show()`/`::hide()` avec la vraie fenêtre de menu (cette classe ne possède pas cette fenêtre elle-même, seulement l'état logique côté RetroArch/pause).

### Step 1: Écrire le test rouge pour `quitGame()`

Lire `tests/emulators/EmulatorProviderTest.cpp`'s tests de `launchGame()`
existants (le stand-in `TestGuiWindowStandIn.exe`, voir sous-projet
précédent) pour réutiliser exactement le même pattern de setup.

```cpp
// À ajouter dans tests/emulators/EmulatorProviderTest.cpp :
void quitGameKillsTheRunningProcessAndFiresGameExited() {
    const QString standIn = QCoreApplication::applicationDirPath() + "/TestGuiWindowStandIn.exe";
    QVERIFY(QFile::exists(standIn));

    QTemporaryDir dir;
    NetworkManager networkManager;
    EmulatorProvider provider(dir.path(), &networkManager);
    provider.setEmbedPollTimeoutForTesting(3000);

    QDir().mkpath(provider.coresDir());
    QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
    QDir().mkpath(provider.retroArchDir());
    QVERIFY(QFile::copy(standIn, provider.retroArchExecutablePath()));

    QJsonObject cores; cores["nes"] = "fceumm";
    QJsonObject state; state["retroarch"] = true; state["cores"] = cores;
    QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
    QFile stateFile(provider.installedStatePath());
    QVERIFY(stateFile.open(QIODevice::WriteOnly));
    stateFile.write(QJsonDocument(state).toJson());
    stateFile.close();

    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    provider.setHostWindowId(host.winId());

    QSignalSpy gameLaunchedSpy(&provider, &EmulatorProvider::gameLaunched);
    QSignalSpy gameExitedSpy(&provider, &EmulatorProvider::gameExited);

    provider.launchGame("dummy.nes", "nes");
    QVERIFY(gameLaunchedSpy.wait(3000));

    provider.quitGame();

    QVERIFY(gameExitedSpy.wait(3000));
}

void openGameMenuThenResumeGameToggleIsGameMenuOpen() {
    const QString standIn = QCoreApplication::applicationDirPath() + "/TestGuiWindowStandIn.exe";
    QVERIFY(QFile::exists(standIn));

    QTemporaryDir dir;
    NetworkManager networkManager;
    EmulatorProvider provider(dir.path(), &networkManager);
    provider.setEmbedPollTimeoutForTesting(3000);

    QDir().mkpath(provider.coresDir());
    QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
    QDir().mkpath(provider.retroArchDir());
    QVERIFY(QFile::copy(standIn, provider.retroArchExecutablePath()));

    QJsonObject cores; cores["nes"] = "fceumm";
    QJsonObject state; state["retroarch"] = true; state["cores"] = cores;
    QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
    QFile stateFile(provider.installedStatePath());
    QVERIFY(stateFile.open(QIODevice::WriteOnly));
    stateFile.write(QJsonDocument(state).toJson());
    stateFile.close();

    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    provider.setHostWindowId(host.winId());

    QSignalSpy gameLaunchedSpy(&provider, &EmulatorProvider::gameLaunched);
    QSignalSpy openedSpy(&provider, &EmulatorProvider::gameMenuOpened);
    QSignalSpy closedSpy(&provider, &EmulatorProvider::gameMenuClosed);

    provider.launchGame("dummy.nes", "nes");
    QVERIFY(gameLaunchedSpy.wait(3000));

    QVERIFY(!provider.isGameMenuOpen());
    provider.openGameMenu();
    QCOMPARE(openedSpy.count(), 1);
    QVERIFY(provider.isGameMenuOpen());

    // Un deuxième appel pendant que le menu est déjà ouvert doit être un
    // no-op (pas de double PAUSE_TOGGLE, pas de deuxième gameMenuOpened).
    provider.openGameMenu();
    QCOMPARE(openedSpy.count(), 1);

    provider.resumeGame();
    QCOMPARE(closedSpy.count(), 1);
    QVERIFY(!provider.isGameMenuOpen());
}
```

Run: `cmake --build build\windows-portable --target EmulatorProviderTest`
Expected: échec de compilation (`quitGame()`/`openGameMenu()`/`resumeGame()`/
`isGameMenuOpen()` n'existent pas encore).

### Step 2: Ajouter `quitGame()` à `EmulatorProvider.h`/`.cpp`

Dans `EmulatorProvider.h`, section publique (à la suite de `launchGame`) :
```cpp
    // Ouvre le menu en jeu (bouton Home, voir InputManager::homeMenuRequested
    // et app/main.cpp) : met RetroArch en pause via RetroArchNetworkCommand
    // ("PAUSE_TOGGLE") et marque le menu comme ouvert. No-op si aucun jeu
    // n'est en cours, ou si le menu est déjà ouvert (Home appuyé deux fois
    // ne doit pas re-basculer la pause). app/main.cpp écoute
    // gameMenuOpened() pour afficher réellement la fenêtre de
    // GameMenuOverlay -- cette classe ne possède pas cette fenêtre.
    Q_INVOKABLE void openGameMenu();

    // Termine volontairement la partie en cours (bouton "Quitter le jeu"
    // du menu en jeu). No-op si aucun jeu n'est en cours. Tente d'abord un
    // arrêt propre de RetroArch via RetroArchNetworkCommand ("QUIT") ; si
    // le process est toujours en vie après un court délai, retombe sur le
    // kill() déjà utilisé ailleurs dans cette classe (chemin d'échec
    // d'embed(), destructeur).
    Q_INVOKABLE void quitGame();

    // Ferme le menu en jeu sans quitter (Cancel/B/Échap depuis
    // InGameMenuOverlay.qml, Task 3) : renvoie PAUSE_TOGGLE pour reprendre
    // l'exécution, marque le menu comme fermé. No-op si le menu n'est pas
    // actuellement ouvert.
    Q_INVOKABLE void resumeGame();

    // Vrai entre un openGameMenu() et le resumeGame()/quitGame() qui le
    // referme -- lu par Main.qml pour ne pas laisser sa propre gestion de
    // Cancel (navigation d'écran) se déclencher en même temps que celle
    // d'InGameMenuOverlay.qml sur le même signal InputManager::cancel()
    // (les deux QML sont dans des fenêtres distinctes, donc tous deux
    // reçoivent ce signal Qt sans distinction de fenêtre active -- voir
    // Step 4bis).
    Q_INVOKABLE bool isGameMenuOpen() const { return m_gameMenuOpen; }
```

Dans la section `signals:` :
```cpp
    void gameMenuOpened();
    void gameMenuClosed();
```

Dans la section privée (à la suite de `m_hostWindowId`/`m_windowEmbedder`) :
```cpp
    bool m_gameMenuOpen = false;
```

Dans `EmulatorProvider.cpp` :
```cpp
void EmulatorProvider::openGameMenu() {
    if (!m_gameProcess || m_gameProcess->state() == QProcess::NotRunning) return;
    if (m_gameMenuOpen) return;

    RetroArchNetworkCommand command;
    command.send("PAUSE_TOGGLE");
    m_gameMenuOpen = true;
    emit gameMenuOpened();
}

void EmulatorProvider::quitGame() {
    if (!m_gameProcess || m_gameProcess->state() == QProcess::NotRunning) return;

    RetroArchNetworkCommand command;
    command.send("QUIT");

    // Laisse une seconde à RetroArch pour s'arrêter proprement suite à la
    // commande réseau (fire-and-forget, aucune confirmation de réception
    // possible par ce protocole) avant de retomber sur un arrêt forcé --
    // évite qu'un jeu reste bloqué indéfiniment si la commande n'a pas
    // été reçue/traitée.
    if (!m_gameProcess->waitForFinished(1000)) {
        m_gameProcess->kill();
        m_gameProcess->waitForFinished(3000);
    }

    m_gameMenuOpen = false;
    emit gameMenuClosed();
}

void EmulatorProvider::resumeGame() {
    if (!m_gameMenuOpen) return;

    RetroArchNetworkCommand command;
    command.send("PAUSE_TOGGLE");
    m_gameMenuOpen = false;
    emit gameMenuClosed();
}
```

Ajouter `#include "RetroArchNetworkCommand.h"` en haut d'`EmulatorProvider.cpp`.

Note pour l'implémenteur : le `finished`-handler déjà existant émet
`gameExited(exitCode)` inconditionnellement dès que `m_gameProcess`
se termine (par la commande réseau ou par le kill de repli dans
`quitGame()`) -- ne pas dupliquer cette émission, ni réinitialiser
`m_gameMenuOpen` deux fois (le `finished`-handler n'a pas besoin d'y
toucher, `quitGame()` s'en charge déjà avant que le process ne se
termine).

### Step 3: Lancer le test, vérifier qu'il passe

Run: `ctest --test-dir build\windows-portable -R EmulatorProviderTest --output-on-failure`
Expected: PASS, y compris `quitGameKillsTheRunningProcessAndFiresGameExited`
et `openGameMenuThenResumeGameToggleIsGameMenuOpen`.

### Step 4: Câbler le déclenchement du menu dans `app/main.cpp`

Lire l'état actuel réel d'`app/main.cpp` après les Tasks 1-3 du
sous-projet précédent (fenêtre de jeu intégrée) avant d'écrire ce step --
ce fichier a déjà été modifié depuis la dernière fois qu'il a été décrit
dans un plan.

Ajouter, après la construction d'`emulatorProvider` : la connexion entre
`InputManager::homeMenuRequested` et `EmulatorProvider::openGameMenu()`
(directe, aucune logique supplémentaire nécessaire ici) ; un `QQuickWindow`
propre pour le menu (créé une fois, réutilisé à chaque ouverture -- pas
recréé à chaque `openGameMenu()`), chargeant `InGameMenuOverlay.qml` via
son propre `QQmlEngine` ou en réutilisant le moteur existant (`engine`) --
**vérifier à l'implémentation si `QQuickWindow::setSource`-équivalent
suffit avec le moteur `Bili` existant, ou si un moteur séparé est
nécessaire ; ce projet n'a jamais chargé un second QML root dans une
fenêtre distincte, pas de précédent à copier ici** ; et les connexions
`gameMenuOpened()`/`gameMenuClosed()` -> `GameMenuOverlay::show()`/
`::hide()` sur ce `QQuickWindow`, avec le `WId` de la fenêtre de jeu
actuellement intégrée (voir Task 3 Step 1 -- si `GameWindowEmbedder`/
`EmulatorProvider` n'exposaient pas encore ce `WId`, la Task 3 a dû
l'ajouter ; utiliser cet ajout réel, pas une supposition).

Cette étape dépend directement de la forme finale retenue en Task 3 (où
vit `GameMenuOverlay`, comment obtenir le `WId` de la fenêtre de jeu) --
**écrire le câblage réel une fois la Task 3 terminée et son rapport lu,
pas en devinant par avance la forme exacte de `GameMenuOverlay`'s API
telle que proposée dans son brief** (le brief de cette Task 4 doit inclure
un pointeur explicite vers le rapport de la Task 3).

### Step 4bis: Empêcher `Main.qml` de réagir à Cancel pendant que le menu est ouvert

Requis par le spec (voir Task 3 Step 2 -- `InGameMenuOverlay.qml` a son
propre `Connections` sur `InputManager.cancel` pour fermer le menu, mais
`Main.qml` a déjà son propre `Connections { function onCancel() {...} }`
pour la navigation d'écran habituelle -- **les deux vivent dans des
fenêtres QML distinctes mais reçoivent le même signal Qt, sans
distinction de fenêtre active**). Modifier `ui/Main.qml`'s `onCancel`
existant pour ignorer l'appel pendant que le menu en jeu est ouvert :

```qml
        function onCancel() {
            if (EmulatorProvider.isGameMenuOpen()) return
            if (ScreenManager.currentScreen === "GameList") {
                ScreenManager.push("MainMenu")
            } else {
                ScreenManager.pop()
            }
        }
```

### Step 5: Ajouter le QML du menu au build

Modifier `app/CMakeLists.txt`, ajouter `../ui/screens/InGameMenuOverlay.qml`
à `BILI_QML_FILES`, suivant exactement le même pattern (alias de ressource)
que les fichiers QML déjà listés.

### Step 6: Compiler, lancer la suite complète

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: tous les tests au vert, `Bili.exe` compile.

### Step 7: Vérification manuelle réelle de bout en bout

Avec un vrai RetroArch + core installés et un vrai jeu :
1. Lancer le jeu, confirmer qu'il s'affiche intégré (comme avant).
2. Appuyer sur le bouton Home/Guide de la manette.
3. Confirmer que le jeu se fige réellement (pas juste visuellement --
   ex. si le jeu a un élément animé, vérifier qu'il arrête de bouger) et
   que le menu Bili apparaît par-dessus, image du jeu toujours visible
   dessous.
4. Avant de quitter : appuyer sur Cancel/B (manette) ou Échap (clavier)
   pour fermer le menu SANS quitter -- confirmer que le jeu reprend
   réellement son exécution (pas juste que le menu disparaît) et
   qu'appuyer de nouveau sur Home rouvre correctement le menu.
5. Rouvrir le menu, naviguer jusqu'à "Quitter le jeu" (clavier et
   manette), valider.
6. Confirmer un arrêt propre (pas de fenêtre RetroArch orpheline, vérifier
   via le Gestionnaire des tâches) et un retour normal à l'UI de Bili.
7. Documenter tout écart avec le comportement attendu ci-dessus dans le
   rapport, honnêtement -- ne pas déclarer cette étape réussie si un
   élément ne correspond pas exactement.

### Step 8: Commit

```bash
git add core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp \
        app/main.cpp app/CMakeLists.txt ui/Main.qml \
        tests/emulators/EmulatorProviderTest.cpp
git commit -m "feat: wire the Home button to pause+overlay the game, with a Quitter le jeu action"
```
