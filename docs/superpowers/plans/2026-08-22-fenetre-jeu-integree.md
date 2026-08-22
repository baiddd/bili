# Fenêtre de jeu intégrée dans Bili — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lancer un jeu réattache la fenêtre de RetroArch dans la fenêtre de Bili elle-même (Win32 `SetParent`), au lieu d'ouvrir une fenêtre séparée.

**Architecture:** Une nouvelle classe `GameWindowEmbedder` (isolée, code Win32 réel sous `#ifdef Q_OS_WIN`, no-op ailleurs) trouve la fenêtre du process RetroArch par son PID, la réattache et la redimensionne dans la fenêtre hôte. `EmulatorProvider::launchGame()` l'utilise après le démarrage du process ; `app/main.cpp` fournit le handle natif de la fenêtre de Bili et relaie les redimensionnements.

**Tech Stack:** Qt6 (Core/Gui/Network/Quick), API Win32 (`<windows.h>`), QtTest.

**Spec:** `docs/superpowers/specs/2026-08-22-fenetre-jeu-integree-design.md`

## Global Constraints

- Aucune dépendance QtQuick ajoutée à `core/` (lie aujourd'hui Qt6::Core/Sql/Network/Gui/Concurrent + SDL2 uniquement) — le handle de fenêtre hôte est injecté depuis `app/main.cpp`, jamais résolu depuis `core/`.
- Fonctionnalité strictement Windows : tout code Win32 réel est sous `#ifdef Q_OS_WIN` ; sur toute autre plateforme, les fonctions concernées sont des no-op qui retournent `false`/ne font rien — jamais une erreur de compilation (presets Linux/RPi/Android existants dans `CMakePresets.json`).
- Échec du réattachement → le lancement échoue (`launchFailed`), le process RetroArch déjà démarré est tué — pas de repli en fenêtre séparée.
- Cette fonctionnalité ne pilote jamais l'état fenêtré/plein écran de la fenêtre de Bili elle-même ; elle suit uniquement son état courant.

---

## Task 1: `GameWindowEmbedder` (Win32 core logic) + stand-in de test + tests unitaires

**Files:**
- Create: `core/emulators/GameWindowEmbedder.h`
- Create: `core/emulators/GameWindowEmbedder.cpp`
- Modify: `core/CMakeLists.txt` (ajouter `emulators/GameWindowEmbedder.cpp` à `target_sources(bili-core ...)`)
- Create: `tests/emulators/TestGuiWindowStandIn.cpp` (petit exécutable Win32 pur, sans Qt — crée une vraie fenêtre top-level, se ferme tout seul après ~2s ou plus tôt si tué)
- Create: `tests/emulators/CMakeLists.txt` — modifier pour ajouter la cible `TestGuiWindowStandIn` (Windows uniquement)
- Test: `tests/emulators/GameWindowEmbedderTest.cpp`
- Modify: `tests/emulators/CMakeLists.txt` (ajouter la cible de test `GameWindowEmbedderTest`)

**Interfaces:**
- Produces:
  - `class GameWindowEmbedder` avec :
    - `bool embed(qint64 processId, WId hostWindowId)` — bloquant, sonde jusqu'à `m_pollTimeoutMs` (défaut 5000 ms, pas de 100 ms), retourne `true` seulement si la fenêtre du process a été trouvée, restylée, réattachée, redimensionnée et mise au focus.
    - `void resizeToHost(WId hostWindowId)` — no-op si aucune fenêtre n'est actuellement intégrée (i.e. `embed()` n'a jamais réussi, ou pas depuis le dernier échec).
    - `void setPollTimeoutForTesting(int ms)`.
  - `WId` vient de `<qwindowdefs.h>` (déjà transitively disponible via Qt6::Gui, déjà lié par `bili-core`).

### Step 1: Écrire le stand-in Win32 (fenêtre de test réelle)

Ce petit exécutable simule RetroArch pour les tests : un vrai process qui
crée une vraie fenêtre top-level visible avec un titre connu, et se ferme
tout seul après ~2s (ou plus tôt si le test le tue explicitement via
`QProcess::kill()`).

```cpp
// tests/emulators/TestGuiWindowStandIn.cpp
//
// Stand-in Win32 minimal pour GameWindowEmbedderTest (Task 1) et pour le
// test de nettoyage-à-la-sortie de EmulatorProviderTest (Task 2) : simule
// une application graphique tierce (RetroArch) en créant une vraie fenêtre
// top-level avec un titre connu. Se ferme tout seul après ~2s (WM_TIMER ->
// DestroyWindow), pour que le test de nettoyage à la sortie (qui a besoin
// d'un process qui se termine de lui-même une fois lancé via le vrai
// EmulatorProvider::launchArgs(), donc sans pouvoir lui passer un argument
// personnalisé) observe une vraie sortie de process sans attendre
// longtemps. 2s laisse largement le temps aux tests Task 1
// (embed()/resizeToHost(), qui font leurs assertions en quelques ms puis
// tuent le process explicitement) de terminer avant l'auto-fermeture --
// un kill() explicite sur un process déjà en train de se fermer tout seul
// est un no-op sans danger. Volontairement sans dépendance Qt -- ce n'est
// pas Bili, c'est le "process externe" que GameWindowEmbedder doit savoir
// retrouver et réattacher.
#include <windows.h>

constexpr UINT_PTR kAutoCloseTimerId = 1;
constexpr UINT kAutoCloseDelayMs = 2000;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_TIMER && wParam == kAutoCloseTimerId) {
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"BiliTestStandInWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"BiliTestStandInWindow",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    SetTimer(hwnd, kAutoCloseTimerId, kAutoCloseDelayMs, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
```

### Step 2: Câbler la cible CMake du stand-in

Lire `tests/emulators/CMakeLists.txt` existant (3 cibles `qt_add_executable`
déjà présentes) et ajouter, réservé à Windows :

```cmake
if(WIN32)
    add_executable(TestGuiWindowStandIn WIN32 TestGuiWindowStandIn.cpp)
endif()
```

Placer ce bloc avant les cibles de test qui en dépendent (voir Step 8).
Aucun lien Qt nécessaire pour cette cible — c'est un exécutable Win32 pur.

### Step 3: Écrire l'en-tête `GameWindowEmbedder.h`

```cpp
// core/emulators/GameWindowEmbedder.h
#pragma once
#include <qwindowdefs.h> // WId
#include <QtGlobal>      // qint64

// Réattache la fenêtre d'un process externe (RetroArch) dans une fenêtre
// hôte (celle de Bili), via l'API Win32 SetParent -- pas une imitation
// visuelle. Fonctionnalité strictement Windows : sur toute autre
// plateforme, embed()/resizeToHost() sont des no-op qui échouent
// silencieusement (voir GameWindowEmbedder.cpp), jamais une erreur de
// compilation, pour ne pas casser les presets Linux/RPi/Android.
class GameWindowEmbedder {
public:
    // Bloquant : sonde jusqu'à m_pollTimeoutMs (par pas de 100 ms) pour
    // trouver la première fenêtre top-level visible appartenant à
    // processId, la restyle (retire bordure/barre de titre), la réattache
    // dans hostWindowId, la redimensionne pour remplir sa zone cliente, et
    // lui donne le focus clavier. Retourne false sur tout échec (fenêtre
    // jamais trouvée dans le budget de temps, ou un appel Win32 échoue) --
    // ne lève jamais d'exception.
    bool embed(qint64 processId, WId hostWindowId);

    // Repositionne/redimensionne la fenêtre déjà intégrée pour remplir la
    // zone cliente actuelle de hostWindowId. No-op silencieux si embed()
    // n'a jamais réussi (ou a échoué depuis).
    void resizeToHost(WId hostWindowId);

    // Réduit le budget de temps d'embed() pour la suite de tests -- sans
    // ça, un test du cas d'échec attendrait le vrai timeout de 5s.
    void setPollTimeoutForTesting(int ms) { m_pollTimeoutMs = ms; }

private:
    WId m_embeddedWindowId = 0; // 0 = aucune fenêtre actuellement intégrée
    int m_pollTimeoutMs = 5000;
};
```

### Step 4: Écrire les tests (rouge attendu, la classe n'existe pas encore côté .cpp)

```cpp
// tests/emulators/GameWindowEmbedderTest.cpp
#include <windows.h> // HWND/RECT/GetWindow/GetParent/GetClientRect, pour les assertions
#include <QTest>
#include <QProcess>
#include <QWindow>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QFile>
#include "emulators/GameWindowEmbedder.h"

class GameWindowEmbedderTest : public QObject {
    Q_OBJECT
private slots:
    void embedFindsAndReparentsAndResizesRealWindow();
    void embedFailsWhenNoWindowEverAppears();
    void resizeToHostFollowsHostClientRectChange();
};

// Chemin du stand-in Win32 : compilé dans le même dossier de build que ce
// test (voir tests/emulators/CMakeLists.txt), donc à côté de ce binaire.
static QString standInPath() {
    return QCoreApplication::applicationDirPath() + "/TestGuiWindowStandIn.exe";
}

void GameWindowEmbedderTest::embedFindsAndReparentsAndResizesRealWindow() {
    QVERIFY(QFile::exists(standInPath()));

    // Fenêtre hôte réelle pour ce test (joue le rôle de la fenêtre de Bili).
    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    host.show();

    QProcess process;
    process.start(standInPath(), {});
    QVERIFY(process.waitForStarted(3000));

    GameWindowEmbedder embedder;
    embedder.setPollTimeoutForTesting(3000);
    QVERIFY(embedder.embed(process.processId(), host.winId()));

    // La zone cliente de l'hôte fait 800x600 -- la fenêtre intégrée doit
    // avoir été redimensionnée pour la remplir exactement.
    HWND childHwnd = nullptr;
    for (HWND candidate = GetWindow(reinterpret_cast<HWND>(host.winId()), GW_CHILD);
         candidate; candidate = GetWindow(candidate, GW_HWNDNEXT)) {
        childHwnd = candidate;
    }
    QVERIFY(childHwnd != nullptr);
    QVERIFY(GetParent(childHwnd) == reinterpret_cast<HWND>(host.winId()));

    RECT childRect;
    QVERIFY(GetClientRect(childHwnd, &childRect));
    QCOMPARE(childRect.right - childRect.left, 800);
    QCOMPARE(childRect.bottom - childRect.top, 600);

    process.kill();
    process.waitForFinished(3000);
}

void GameWindowEmbedderTest::embedFailsWhenNoWindowEverAppears() {
    // whoami.exe est un vrai exécutable système qui démarre et se termine
    // quasi immédiatement sans jamais créer de fenêtre -- même stand-in
    // "process réel sans fenêtre" que celui déjà utilisé par
    // EmulatorProviderTest pour un autre scénario.
    const QString systemRoot = qEnvironmentVariable("SystemRoot", "C:/Windows");
    const QString whoami = systemRoot + "/System32/whoami.exe";
    QVERIFY(QFile::exists(whoami));

    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    host.show();

    QProcess process;
    process.start(whoami, {});
    QVERIFY(process.waitForStarted(3000));
    process.waitForFinished(3000); // whoami.exe se termine vite -- PID toujours valide pour embed()

    GameWindowEmbedder embedder;
    embedder.setPollTimeoutForTesting(500); // court, pour ne pas ralentir la suite
    QVERIFY(!embedder.embed(process.processId(), host.winId()));
}

void GameWindowEmbedderTest::resizeToHostFollowsHostClientRectChange() {
    QVERIFY(QFile::exists(standInPath()));

    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    host.show();

    QProcess process;
    process.start(standInPath(), {});
    QVERIFY(process.waitForStarted(3000));

    GameWindowEmbedder embedder;
    embedder.setPollTimeoutForTesting(3000);
    QVERIFY(embedder.embed(process.processId(), host.winId()));

    host.setGeometry(0, 0, 400, 300);
    embedder.resizeToHost(host.winId());

    HWND childHwnd = GetWindow(reinterpret_cast<HWND>(host.winId()), GW_CHILD);
    QVERIFY(childHwnd != nullptr);
    RECT childRect;
    QVERIFY(GetClientRect(childHwnd, &childRect));
    QCOMPARE(childRect.right - childRect.left, 400);
    QCOMPARE(childRect.bottom - childRect.top, 300);

    process.kill();
    process.waitForFinished(3000);
}

QTEST_MAIN(GameWindowEmbedderTest)
#include "GameWindowEmbedderTest.moc"
```

### Step 5: Lancer les tests, vérifier l'échec attendu

Run: `cmake --build build\windows-portable --target GameWindowEmbedderTest`
Expected: échec de compilation (`GameWindowEmbedder.cpp` n'existe pas encore /
la classe n'a pas d'implémentation) -- confirme que le test est bien câblé
avant d'écrire l'implémentation.

### Step 6: Implémenter `GameWindowEmbedder.cpp`

```cpp
// core/emulators/GameWindowEmbedder.cpp
#include "GameWindowEmbedder.h"

#ifdef Q_OS_WIN
#include <windows.h>

namespace {

struct FindWindowData {
    DWORD processId = 0;
    HWND result = nullptr;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto *data = reinterpret_cast<FindWindowData *>(lParam);
    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId == data->processId && IsWindowVisible(hwnd)) {
        data->result = hwnd;
        return FALSE; // trouvé, arrêter l'énumération
    }
    return TRUE;
}

HWND findTopLevelWindowForProcess(DWORD processId) {
    FindWindowData data{processId, nullptr};
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.result;
}

} // namespace

bool GameWindowEmbedder::embed(qint64 processId, WId hostWindowId) {
    const DWORD pid = static_cast<DWORD>(processId);
    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);

    HWND targetHwnd = nullptr;
    const int stepMs = 100;
    int elapsed = 0;
    while (elapsed <= m_pollTimeoutMs) {
        targetHwnd = findTopLevelWindowForProcess(pid);
        if (targetHwnd) break;
        Sleep(stepMs);
        elapsed += stepMs;
    }
    if (!targetHwnd) return false;

    // Retire bordure/barre de titre/boutons système, ajoute WS_CHILD.
    LONG_PTR style = GetWindowLongPtr(targetHwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP);
    style |= WS_CHILD;
    SetWindowLongPtr(targetHwnd, GWL_STYLE, style);

    if (SetParent(targetHwnd, hostHwnd) == nullptr) return false;

    m_embeddedWindowId = reinterpret_cast<WId>(targetHwnd);
    resizeToHost(hostWindowId);
    SetWindowPos(targetHwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    SetFocus(targetHwnd);
    return true;
}

void GameWindowEmbedder::resizeToHost(WId hostWindowId) {
    if (!m_embeddedWindowId) return;
    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);
    const HWND childHwnd = reinterpret_cast<HWND>(m_embeddedWindowId);

    RECT clientRect;
    if (!GetClientRect(hostHwnd, &clientRect)) return;
    MoveWindow(childHwnd, 0, 0,
               clientRect.right - clientRect.left,
               clientRect.bottom - clientRect.top,
               TRUE);
}

#else // !Q_OS_WIN

bool GameWindowEmbedder::embed(qint64, WId) { return false; }
void GameWindowEmbedder::resizeToHost(WId) {}

#endif
```

### Step 7: Ajouter la source au build de `core/`

Modifier `core/CMakeLists.txt`, dans la liste `target_sources(bili-core PRIVATE ...)` :
ajouter `emulators/GameWindowEmbedder.cpp` à la suite de
`emulators/EmulatorProvider.cpp`.

### Step 8: Câbler la cible de test

Modifier `tests/emulators/CMakeLists.txt`, ajouter à la suite des 3 cibles
existantes (après le bloc `if(WIN32)` du Step 2) :

```cmake
if(WIN32)
    qt_add_executable(GameWindowEmbedderTest GameWindowEmbedderTest.cpp)
    target_link_libraries(GameWindowEmbedderTest PRIVATE Qt6::Test Qt6::Gui bili-core)
    add_test(NAME GameWindowEmbedderTest COMMAND GameWindowEmbedderTest)
    add_dependencies(GameWindowEmbedderTest TestGuiWindowStandIn)
endif()
```

`add_dependencies` garantit que `TestGuiWindowStandIn.exe` est bien
construit avant que `GameWindowEmbedderTest` ne s'exécute.

### Step 9: Lancer les tests, vérifier qu'ils passent

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R GameWindowEmbedderTest --output-on-failure`
Expected: `100% tests passed` pour `GameWindowEmbedderTest` (3/3 cas).

### Step 10: Lancer la suite complète, vérifier l'absence de régression

Run: `ctest --test-dir build\windows-portable --output-on-failure`
Expected: tous les tests existants toujours au vert, plus ce nouveau
binaire.

### Step 11: Commit

```bash
git add core/emulators/GameWindowEmbedder.h core/emulators/GameWindowEmbedder.cpp \
        core/CMakeLists.txt \
        tests/emulators/TestGuiWindowStandIn.cpp tests/emulators/GameWindowEmbedderTest.cpp \
        tests/emulators/CMakeLists.txt
git commit -m "feat: add GameWindowEmbedder for Win32 window reparenting"
```

---

## Task 2: Intégrer `GameWindowEmbedder` dans `EmulatorProvider::launchGame()`

**Files:**
- Modify: `core/emulators/EmulatorProvider.h`
- Modify: `core/emulators/EmulatorProvider.cpp`
- Test: `tests/emulators/EmulatorProviderTest.cpp`

**Interfaces:**
- Consumes: `GameWindowEmbedder::embed(qint64, WId)` / `::resizeToHost(WId)` (Task 1).
- Produces:
  - `EmulatorProvider::setHostWindowId(WId id)` — nouveau setter public, appelé une fois par `app/main.cpp` (Task 3).
  - `EmulatorProvider::handleHostWindowResized()` — nouvelle méthode publique (non-`Q_INVOKABLE`, appelée depuis C++ uniquement par `app/main.cpp`, jamais depuis QML), forwarde vers `m_windowEmbedder.resizeToHost(m_hostWindowId)`.
  - Comportement changé : `gameLaunched()` n'est plus émis directement sur `QProcess::started` -- il n'est émis qu'après un `embed()` réussi. Un échec d'`embed()` tue `m_gameProcess` et émet `launchFailed("Impossible d'intégrer la fenêtre du jeu.")`.

### Step 1: Écrire le test rouge (embed échoue → launchFailed, pas de gameLaunched)

Ce test suit exactement le pattern déjà utilisé par le test existant
`launchGameCleansUpTempDirWhenTheGameExits()` (`tests/emulators/EmulatorProviderTest.cpp`,
autour de la ligne 512) : `QTemporaryDir`/`NetworkManager` locaux à la
fonction (pas de fixture partagée dans cette suite), état installé écrit
directement en JSON à `provider.installedStatePath()`, `whoami.exe` copié
comme substitut de `retroarch.exe`.

D'abord, `EmulatorProvider` doit pouvoir réduire le budget de temps de son
`GameWindowEmbedder` interne dans les tests (sinon ce test attendrait les
5s réelles) : ajouter `setEmbedPollTimeoutForTesting(int ms)` à
`EmulatorProvider.h`/`.cpp` en même temps que `setHostWindowId`/
`handleHostWindowResized` (voir Step 3 de cette tâche) — les quatre membres
sont ajoutés ensemble.

```cpp
// À ajouter dans tests/emulators/EmulatorProviderTest.cpp, private slots,
// juste après launchGameCleansUpTempDirWhenTheGameExits() :
void launchGameFailsWhenWindowEmbeddingFails() {
    // whoami.exe démarre et se termine quasi immédiatement sans jamais
    // créer de fenêtre -- exactement le scénario "embed() échoue" (même
    // stand-in que GameWindowEmbedderTest::embedFailsWhenNoWindowEverAppears,
    // Task 1).
    const QString systemRoot = qEnvironmentVariable("SystemRoot", "C:/Windows");
    const QString whoami = systemRoot + "/System32/whoami.exe";
    QVERIFY(QFile::exists(whoami));

    QTemporaryDir dir;
    NetworkManager networkManager;
    EmulatorProvider provider(dir.path(), &networkManager);
    provider.setEmbedPollTimeoutForTesting(500); // court, pour ne pas ralentir la suite

    QDir().mkpath(provider.coresDir());
    QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
    QDir().mkpath(provider.retroArchDir());
    QVERIFY(QFile::copy(whoami, provider.retroArchExecutablePath()));

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

    QSignalSpy launchFailedSpy(&provider, &EmulatorProvider::launchFailed);
    QSignalSpy gameLaunchedSpy(&provider, &EmulatorProvider::gameLaunched);

    provider.launchGame("dummy.nes", "nes");

    QVERIFY(launchFailedSpy.wait(3000));
    QCOMPARE(gameLaunchedSpy.count(), 0);
}
```

Ajouter `#include <QWindow>` en haut du fichier de test s'il n'y est pas déjà.

### Step 2: Lancer le test, vérifier l'échec attendu

Run: `cmake --build build\windows-portable --target EmulatorProviderTest && ctest --test-dir build\windows-portable -R EmulatorProviderTest`
Expected: FAIL — `setHostWindowId`/`launchFailed` pour ce cas n'existent pas
encore / `gameLaunched` est toujours émis directement sur `started`.

### Step 3: Ajouter les membres et méthodes à `EmulatorProvider.h`

Ajouter en haut du fichier : `#include "GameWindowEmbedder.h"`.

Ajouter dans la section publique (à la suite de `launchGame`) :
```cpp
    // Fenêtre hôte (celle de Bili) dans laquelle intégrer la fenêtre du jeu
    // au lancement -- fournie une fois par app/main.cpp (voir Task 3), qui
    // est le seul endroit du code ayant accès à la fois à QtQuick et à
    // EmulatorProvider. Un id de 0 (valeur par défaut) fait échouer tout
    // embed() -- launchGame() ne peut pas fonctionner tant que ce setter
    // n'a pas été appelé.
    void setHostWindowId(WId id) { m_hostWindowId = id; }

    // Appelé depuis app/main.cpp (jamais depuis QML) quand la fenêtre hôte
    // change de taille, pour que la fenêtre du jeu actuellement intégrée
    // (s'il y en a une) suive. No-op si aucun jeu n'est en cours.
    void handleHostWindowResized() { m_windowEmbedder.resizeToHost(m_hostWindowId); }

    // Testing-only: réduit le budget de temps de GameWindowEmbedder::embed()
    // pour ne pas attendre les 5s réelles dans la suite automatisée.
    void setEmbedPollTimeoutForTesting(int ms) { m_windowEmbedder.setPollTimeoutForTesting(ms); }
```

Ajouter dans la section privée (à la suite de `m_gameTempDir`) :
```cpp
    WId m_hostWindowId = 0;
    GameWindowEmbedder m_windowEmbedder;
```

### Step 4: Modifier `launchGame()` dans `EmulatorProvider.cpp`

Remplacer la connexion existante :
```cpp
    connect(m_gameProcess, &QProcess::started, this, [this]() { emit gameLaunched(); });
```
par :
```cpp
    connect(m_gameProcess, &QProcess::started, this, [this]() {
        // L'intégration de la fenêtre (Win32 SetParent) est requise avant de
        // considérer le lancement comme réussi -- un jeu qui tourne dans sa
        // propre fenêtre non intégrée n'est pas le comportement attendu
        // (voir docs/superpowers/specs/2026-08-22-fenetre-jeu-integree-design.md).
        // embed() est bloquant (jusqu'à ~5s dans le pire cas d'échec) ; le
        // process vient tout juste de démarrer, donc dans le cas normal la
        // fenêtre apparaît en quelques centaines de ms.
        if (!m_windowEmbedder.embed(m_gameProcess->processId(), m_hostWindowId)) {
            m_gameProcess->kill();
            m_gameProcess->waitForFinished(3000);
            emit launchFailed("Impossible d'intégrer la fenêtre du jeu.");
            return;
        }
        emit gameLaunched();
    });
```

### Step 5: Lancer le test, vérifier qu'il passe

Run: `cmake --build build\windows-portable --target EmulatorProviderTest && ctest --test-dir build\windows-portable -R EmulatorProviderTest --output-on-failure`
Expected: PASS, y compris `launchGameFailsWhenWindowEmbeddingFails`.

### Step 6: Adapter le test existant `launchGameCleansUpTempDirWhenTheGameExits`

Ce test existant (autour de la ligne 512) utilisait `whoami.exe` comme
substitut de `retroarch.exe` : ça fonctionnait tant que `gameLaunched()`
était émis directement sur `QProcess::started`, mais `whoami.exe` ne crée
aucune fenêtre, donc avec le changement du Step 4 son `embed()` échouerait
systématiquement (`launchFailed` au lieu de `gameLaunched`/`gameExited`).
Remplacer `whoami.exe` par le stand-in GUI du Task 1
(`TestGuiWindowStandIn.exe`, qui crée une vraie fenêtre puis se ferme tout
seul après ~2s — voir Task 1 Step 1), et ajouter le `setHostWindowId`
requis :

```cpp
// Remplacer, dans launchGameCleansUpTempDirWhenTheGameExits() :
//     const QString systemRoot = qEnvironmentVariable("SystemRoot", "C:/Windows");
//     const QString whoami = systemRoot + "/System32/whoami.exe";
//     QVERIFY(QFile::exists(whoami));
// par :
    const QString standIn = QCoreApplication::applicationDirPath() + "/TestGuiWindowStandIn.exe";
    QVERIFY(QFile::exists(standIn));

// Remplacer :
//     QVERIFY(QFile::copy(whoami, provider.retroArchExecutablePath()));
// par :
    QVERIFY(QFile::copy(standIn, provider.retroArchExecutablePath()));

// Ajouter juste avant `provider.launchGame(zipPath + "::game.nes", "nes");` :
    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    provider.setHostWindowId(host.winId());
```

Mettre aussi à jour le commentaire du test (lignes 500-511) : il ne décrit
plus `whoami.exe` mais le stand-in GUI, et le "genuine QProcess::NormalExit"
vient maintenant de l'auto-fermeture du stand-in après ~2s plutôt que d'une
erreur immédiate sur l'argument `-L`.

Ajouter `#include <QWindow>` et `#include <QCoreApplication>` en haut du
fichier de test s'ils n'y sont pas déjà (ce dernier probablement déjà
présent via un include transitif, vérifier avant d'ajouter un doublon).

Run: `ctest --test-dir build\windows-portable -R EmulatorProviderTest --output-on-failure`
Expected: PASS, y compris ce test adapté (l'attente `exitedSpy.wait(5000)`
laisse largement le temps aux ~2s d'auto-fermeture du stand-in).

### Step 7: Lancer la suite complète, vérifier l'absence de régression

Run: `ctest --test-dir build\windows-portable --output-on-failure`
Expected: tous les tests au vert.

### Step 8: Commit

```bash
git add core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp \
        tests/emulators/EmulatorProviderTest.cpp
git commit -m "feat: wire GameWindowEmbedder into EmulatorProvider::launchGame()"
```

---

## Task 3: Câbler la fenêtre hôte réelle + suivi du redimensionnement dans `app/main.cpp`

**Files:**
- Modify: `app/main.cpp`
- Modify: `app/CMakeLists.txt` (si un module Qt supplémentaire est requis pour le filtre d'évènement natif — `Qt6::Gui` est déjà lié, aucun ajout normalement nécessaire)

**Interfaces:**
- Consumes: `EmulatorProvider::setHostWindowId(WId)` / `::handleHostWindowResized()` (Task 2).

### Step 1: Récupérer le handle natif de la fenêtre racine après `loadFromModule`

Dans `app/main.cpp`, après la ligne `engine.loadFromModule("Bili", "Main");`
et avant `return app.exec();`, ajouter :

```cpp
    // engine.rootObjects() n'est peuplé qu'après loadFromModule() -- il n'y
    // a donc pas d'ordre alternatif possible pour cette étape. root est la
    // fenêtre ApplicationWindow de ui/Main.qml elle-même (un QQuickWindow,
    // sous-classe de QWindow) : winId() donne son HWND natif Windows.
    QWindow *rootWindow = qobject_cast<QWindow *>(engine.rootObjects().value(0));
    if (rootWindow) {
        emulatorProvider.setHostWindowId(rootWindow->winId());
    }
```

Ajouter `#include <QWindow>` en haut du fichier avec les autres includes Qt.

### Step 2: Ajouter un filtre d'évènement natif pour suivre le redimensionnement

Toujours dans `app/main.cpp`, avant `int main(...)`, ajouter :

```cpp
// Relaie WM_SIZE de la fenêtre racine vers EmulatorProvider, pour que la
// fenêtre du jeu actuellement intégrée (s'il y en a une) suive un
// redimensionnement de la fenêtre de Bili. handleHostWindowResized() est
// un no-op si aucun jeu n'est en cours -- ce filtre peut donc tourner en
// permanence sans se soucier de l'état courant.
class HostResizeEventFilter : public QAbstractNativeEventFilter {
public:
    explicit HostResizeEventFilter(EmulatorProvider *provider) : m_provider(provider) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override {
        if (eventType != "windows_generic_MSG") return false;
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_SIZE) {
            m_provider->handleHostWindowResized();
        }
        return false; // ne consomme jamais l'évènement -- Qt doit le traiter aussi
    }

private:
    EmulatorProvider *m_provider;
};
```

Ajouter `#include <QAbstractNativeEventFilter>` et `#include <windows.h>`
(pour `MSG`/`WM_SIZE`) en haut du fichier -- guarder cet include Windows-only
avec `#ifdef Q_OS_WIN` autour de la classe entière et de son installation
(Step 3), pour ne pas casser la compilation des presets non-Windows.

### Step 3: Installer le filtre

Dans `main()`, après avoir construit `emulatorProvider` (et sous le même
`#ifdef Q_OS_WIN` que la classe ci-dessus) :

```cpp
#ifdef Q_OS_WIN
    HostResizeEventFilter resizeFilter(&emulatorProvider);
    app.installNativeEventFilter(&resizeFilter);
#endif
```

`resizeFilter` doit rester en vie jusqu'à la fin de `main()` (variable
locale de `main`, comme `emulatorProvider`/`networkManager` déjà présents)
-- ne pas l'allouer dynamiquement sans l'assigner à une variable qui survit
jusqu'à `app.exec()`.

### Step 4: Compiler et vérifier que le build passe

Run: `cmake --build build\windows-portable`
Expected: `Bili.exe` compile et link sans erreur.

### Step 5: Lancer la suite complète, vérifier l'absence de régression

Run: `ctest --test-dir build\windows-portable --output-on-failure`
Expected: tous les tests au vert (cette tâche ne touche aucun code testé
automatiquement, seulement le câblage de `main.cpp`).

### Step 6: Vérification manuelle réelle (pas simulée)

Avec un vrai RetroArch + un vrai core installés (via `EmulatorManagerScreen`)
et une vraie ROM :
1. Lancer `Bili.exe`, naviguer jusqu'à un jeu, le lancer.
2. Confirmer que la fenêtre de RetroArch remplit exactement la fenêtre de
   Bili, sans bordure séparée ni entrée distincte dans la barre des tâches.
3. Confirmer que le clavier contrôle bien le jeu (pas l'UI de Bili).
4. Redimensionner la fenêtre de Bili (glisser un bord) pendant que le jeu
   tourne : confirmer que la fenêtre du jeu suit exactement.
5. Quitter le jeu (menu RetroArch → Quitter) : confirmer que l'UI de Bili
   réapparaît normalement, sans fenêtre orpheline restante (vérifier via le
   Gestionnaire des tâches qu'aucun `retroarch.exe` ne subsiste).
6. Cas d'échec (optionnel, si reproductible) : renommer temporairement
   `retroarch.exe` en un nom qui ne matche pas la détection (ou tout autre
   moyen de faire échouer `embed()`), confirmer qu'un message d'erreur clair
   s'affiche plutôt qu'une fenêtre RetroArch flottante non intégrée.

### Step 7: Commit

```bash
git add app/main.cpp
git commit -m "feat: wire real host window + resize tracking for embedded game window"
```
