# Fenêtre de jeu intégrée dans Bili (réattachement Win32 de RetroArch)

## Contexte

Le sous-projet 3 (gestion des émulateurs) est terminé et poussé sur
`origin/master` (commit `faf4dc2`). Lancer un jeu y ouvre RetroArch dans sa
propre fenêtre séparée, à côté de celle de Bili — comportement fonctionnel
mais visuellement peu intégré (deux fenêtres distinctes, deux entrées
possibles dans la barre des tâches).

Remarque utilisateur après la clôture du sous-projet 3 : le jeu doit
s'afficher **dans la fenêtre de Bili elle-même**, pas dans une fenêtre à
part — via un réattachement de fenêtre natif Windows (Win32 `SetParent`),
pas une simple option de plein écran RetroArch (`-f`), qui a été
explicitement écartée au profit de cette intégration réelle.

Comportement attendu, clarifié pendant le brainstorming : la fenêtre du jeu
doit simplement remplir la zone cliente actuelle de la fenêtre de Bili, quel
que soit son état — fenêtrée (le jeu reste dans cette fenêtre) ou plein
écran (le jeu remplit alors tout l'écran). Cette fonctionnalité ne pilote
pas elle-même l'état fenêtré/plein écran de Bili ; elle suit simplement cet
état.

### Hors périmètre

- **Piloter l'état fenêtré/plein écran de Bili lui-même** (pas de bascule
  automatique en plein écran au lancement d'un jeu) — décision explicite du
  brainstorming, la fenêtre de jeu suit l'état courant, elle ne le change
  pas.
- **Support non-Windows** — le lancement de RetroArch est déjà spécifique à
  Windows dans ce projet (`retroArchExecutablePath()` retourne un `.exe` en
  dur) ; l'intégration de fenêtre l'est donc aussi. Le code Win32 réel est
  isolé sous `#ifdef Q_OS_WIN` pour ne pas casser la compilation des presets
  Linux/RPi/Android déjà présents dans `CMakePresets.json`.
- **Conflits d'accès simultané à une même manette physique** entre Bili
  (`GamepadBridge`) et RetroArch — problème préexistant au lancement de jeu
  actuel (RetroArch lit déjà les manettes directement, indépendamment de
  Bili), ni introduit ni aggravé par cette fonctionnalité.
- **Capture d'écran en jeu** — déjà explicitement hors périmètre du
  sous-projet 3, non réouvert ici.
- **Changement de l'écran QML affiché pendant le jeu** — la fenêtre native
  intégrée recouvre entièrement la zone cliente quel que soit l'écran QML en
  dessous ; aucun nouvel état d'écran (« Playing », etc.) n'est nécessaire.
  `GameDetailsScreen.qml`'s `onGameLaunched`/`onGameExited` (texte de statut)
  restent inchangés.

## Décisions validées

- **Réattachement natif réel** (Win32 `SetParent` + changement de style de
  fenêtre), pas une fenêtre RetroArch simplement repositionnée par-dessus en
  imitation visuelle.
- **Repli en cas d'échec du réattachement : le lancement échoue** avec un
  message clair via le signal `launchFailed` existant, plutôt que de lancer
  le jeu dans une fenêtre séparée non intégrée. Le process RetroArch déjà
  démarré est tué dans ce cas.
- **Nouvelle classe dédiée `GameWindowEmbedder`**, isolée d'`EmulatorProvider`
  plutôt qu'ajoutée directement dedans — logique Win32 auto-contenue, testable
  séparément, gardée sous `#ifdef Q_OS_WIN` (no-op explicite ailleurs, jamais
  une erreur de compilation).
- **La fenêtre hôte (celle de Bili) est injectée depuis `app/main.cpp`**, le
  seul endroit du code qui a accès à la fois à QtQuick (pour obtenir le
  `WId` natif de la fenêtre racine) et à `EmulatorProvider` — pas de nouvelle
  dépendance QtQuick ajoutée à `core/` (qui ne lie aujourd'hui que Qt6::Core/
  Sql/Network/Gui/Concurrent + SDL2).

## Architecture

### 1. `GameWindowEmbedder` (`core/emulators/GameWindowEmbedder.h`/`.cpp`)

API :
- `bool embed(qint64 processId, WId hostWindowId)` — bloquant avec un budget
  de temps interne (sondage toutes les ~100 ms, ~5 s au total par défaut) ;
  retourne `true` seulement une fois que la fenêtre du process cible a été
  trouvée, restylée, réattachée, redimensionnée pour remplir la zone cliente
  de l'hôte, et mise au focus clavier avec succès. Retourne `false` sur
  n'importe quel échec (fenêtre jamais trouvée dans le budget de temps, ou
  un appel Win32 échoue), sans lever d'exception.
- `void resizeToHost(WId hostWindowId)` — repositionne/redimensionne la
  fenêtre déjà intégrée pour suivre un changement de taille de l'hôte ;
  no-op silencieux si aucune fenêtre n'est actuellement intégrée.
- `void setPollTimeoutForTesting(int ms)` — permet de réduire le budget de
  temps dans la suite automatisée, même convention que les autres
  surcharges de test déjà présentes dans ce projet (ex.
  `setSevenZipExecutablePathForTesting`).

Implémentation Windows (`<windows.h>`, guardé `#ifdef Q_OS_WIN`) :
`EnumWindows` + `GetWindowThreadProcessId` pour trouver la première fenêtre
top-level visible appartenant au PID donné ; `SetWindowLongPtr(GWL_STYLE, …)`
pour retirer bordure/barre de titre/boutons système et ajouter `WS_CHILD` ;
`SetParent` pour la réattacher ; `GetClientRect` (sur l'hôte) + `MoveWindow`
pour la redimensionner exactement à la zone cliente hôte ; `SetFocus` pour
rediriger le clavier vers le jeu (sans quoi les touches iraient à Bili). Sur
toute autre plateforme, `embed()`/`resizeToHost()` sont des no-op qui
retournent `false`/ne font rien, jamais une erreur de compilation.

### 2. Intégration dans `EmulatorProvider::launchGame()`

- `EmulatorProvider` possède une instance de `GameWindowEmbedder` et un
  nouveau setter `setHostWindowId(WId id)`, appelé une seule fois depuis
  `app/main.cpp` après la création de la fenêtre QML.
- Après `QProcess::started`, `launchGame()` appelle
  `m_windowEmbedder.embed(m_gameProcess->processId(), m_hostWindowId)`.
  - Si `false` : `m_gameProcess->kill()`, émet
    `launchFailed("Impossible d'intégrer la fenêtre du jeu.")`, ne
    déclenche pas `gameLaunched`.
  - Si `true` : comportement actuel inchangé, `gameLaunched()` émis
    normalement (déjà connecté à `QProcess::started` aujourd'hui — ce
    branchement doit être revu pour n'émettre qu'après un `embed()` réussi,
    pas directement sur `started`).

### 3. Suivi du redimensionnement de la fenêtre hôte

`app/main.cpp` installe un filtre d'évènement natif
(`QAbstractNativeEventFilter`) sur la fenêtre racine QML pour intercepter
`WM_SIZE` et appeler `EmulatorProvider`'s equivalent de
`GameWindowEmbedder::resizeToHost(...)` — no-op si aucun jeu n'est
actuellement intégré (le cas normal, la plupart du temps).

### 4. Cycle de vie / nettoyage

Aucun démontage explicite n'est nécessaire côté Bili : à la sortie du jeu,
Windows détruit la fenêtre de RetroArch avec son process, la logique
`gameExited` existante (nettoyage de `m_gameTempDir`, etc.) reste
inchangée. Le destructeur d'`EmulatorProvider` (déjà corrigé au sous-projet
3 pour tuer `m_gameProcess` avant `m_gameTempDir`) n'a pas besoin de
changement supplémentaire.

## Vérification

- **Test Qt Test sur `GameWindowEmbedder::embed()`** : lance un vrai petit
  exécutable de test factice (nouveau stand-in créant une fenêtre Win32
  simple avec un titre connu — même convention que les stand-ins
  `7za.exe`/`whoami.exe` déjà utilisés dans le sous-projet 3) dans un
  process réel, vérifie que sa fenêtre est retrouvée par PID dans le budget
  de temps réduit (`setPollTimeoutForTesting`), et que son style/parent ont
  bien changé comme attendu après l'appel.
- **Test du cas d'échec** : pointer vers un exécutable qui ne crée jamais de
  fenêtre visible, vérifier que `embed()` retourne `false` sans exception et
  sans laisser de process orphelin (le process de test doit être tué par
  l'appelant, comme `launchGame()` le fera pour un vrai RetroArch).
- **Vérification manuelle réelle** (même méthode que le reste du
  sous-projet 3 : vrais artefacts, pas simulés) : lancer un vrai jeu avec un
  vrai RetroArch installé, confirmer visuellement que sa fenêtre remplit
  exactement la fenêtre de Bili sans bordure séparée ni entrée distincte
  dans la barre des tâches, que le clavier contrôle bien le jeu et non
  l'UI de Bili, que redimensionner/basculer en plein écran la fenêtre de
  Bili pendant une partie fait suivre correctement la fenêtre intégrée, et
  que la sortie du jeu restaure proprement l'UI de Bili. Vérifier également
  empiriquement (pas supposé) que Bili ne reçoit plus les évènements
  clavier une fois le focus donné à la fenêtre intégrée.
