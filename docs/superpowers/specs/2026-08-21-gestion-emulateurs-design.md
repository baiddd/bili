# Sous-projet 3 — Gestion des émulateurs (RetroArch + cores libretro)

## Contexte

Le socle applicatif (sous-projet 1) a posé `IEmulatorProvider` + `StubEmulatorProvider`
("non implémenté"), un écran `EmulatorManagerScreen` avec 3 boutons statiques
(RetroArch/core "nes" codé en dur/émulateur "Dolphin" codé en dur), et un
écran `DownloadManagerScreen` toujours vide. Le sous-projet 2 a livré la
bibliothèque locale : `LibraryDatabase`/`LibraryModel` exposent chaque jeu
avec son `system` détecté (nes/snes/gba/gb/n64/genesis), et
`GameListScreen`/`GameDetailsScreen` sont navigables mais ne permettent pas
encore de lancer un jeu.

Ce sous-projet rend `IEmulatorProvider` réel : téléchargement/installation de
RetroArch et des cores libretro correspondants, et lancement effectif d'un
jeu depuis l'app.

Hors périmètre (différé) :
- **Émulateurs autonomes** (Dolphin, PCSX2, etc.) — seul RetroArch + cores
  libretro est couvert ici. Un émulateur autonome par système ne sera ajouté
  que si un core libretro s'avère insuffisant pour un système donné, en tant
  que sous-projet/extension future séparée.
- **Capture d'écran en jeu** — nécessite une interception de fenêtre externe
  au niveau OS ; différée à un sous-projet/extension future une fois le flux
  de lancement de base éprouvé.
- **Netplay** — hors périmètre total ici (sous-projet 6), RetroArch a son
  propre netplay intégré pour les cores libretro, à réévaluer à ce moment-là.

## Décisions validées

- **RetroArch + cores libretro uniquement** pour ce sous-projet.
- **Catalogue dynamique via un manifeste hébergé par le projet Bili
  lui-même** (JSON dans le dépôt `baiddd/bili`, servi via
  `raw.githubusercontent.com`) plutôt qu'un catalogue figé dans le code ou
  une dépendance directe à la structure du buildbot libretro. Permet de
  corriger/mettre à jour les URLs sans nouvelle version de Bili.
- **Progression de téléchargement/installation affichée en ligne** sur
  `EmulatorManagerScreen` (barre de progression inline) — pas d'écran dédié
  dans ce sous-projet. `DownloadManagerScreen` reste un placeholder,
  potentiellement réutilisé plus tard (scraping, sous-projet 4).
- **Installation ET désinstallation** possibles depuis Bili pour RetroArch
  et chaque core.
- **Lancement d'un jeu suit le cycle de vie du processus** (pas de
  fire-and-forget) — `QProcess` non détaché, signaux `gameLaunched`/
  `gameExited` exposés à QML.
- **Lancement possible directement depuis `GameListScreen`** (pas
  uniquement via `GameDetailsScreen`) si un core est déjà installé pour le
  système du jeu sélectionné ; sinon, comportement actuel (ouvre
  `GameDetailsScreen` pour proposer l'installation).
- **Bili génère une config manette de base pour RetroArch** (autoconfig) à
  partir du même mapping SDL2 que `GamepadBridge` utilise déjà ; l'utilisateur
  reste libre de remapper certaines touches ensuite via le menu Input natif
  de RetroArch — Bili ne construit pas sa propre UI de remapping.
- **RetroArch tourne en mode portable** : config, saves, save-states restent
  sous `data/emulators/retroarch/`, jamais dans le profil utilisateur
  Windows — cohérent avec la contrainte "app portable" du socle.
- **`.7z` requis pour RetroArch, `.zip` suffit pour les cores** — vérifié
  directement sur `buildbot.libretro.com` (pas deviné) : les cores libretro
  sont distribués en `.zip` (compatible avec `miniz`, déjà vendu au
  sous-projet 2) ; RetroArch lui-même n'existe qu'en `RetroArch-Win64-setup.exe`
  (installeur, exclu par la contrainte "pas d'installeur") ou `RetroArch.7z`
  (portable, mais `.7z`). Décision : bundler un petit exécutable `7za.exe`
  (7-Zip en ligne de commande, domaine public) et l'appeler via `QProcess`
  pour extraire uniquement l'archive RetroArch — pas de nouvelle bibliothèque
  C++ à intégrer pour ce besoin ponctuel. Ne réouvre pas la question `.7z`
  pour le scan de ROMs (sous-projet 2), chemin de code totalement séparé.

## Architecture

### 1. Manifeste du catalogue

Fichier JSON hébergé dans le dépôt `baiddd/bili` (ex: `catalog/emulators.json`
sur `master`), servi via
`https://raw.githubusercontent.com/baiddd/bili/master/catalog/emulators.json` :

```json
{
  "retroarch": {
    "version": "<à vérifier à l'implémentation>",
    "windows_x64_url": "<URL .7z du buildbot stable, vérifiée à l'implémentation>"
  },
  "cores": {
    "nes":     { "core": "fceumm",           "url": "<URL .zip vérifiée>" },
    "snes":    { "core": "snes9x",           "url": "<URL .zip vérifiée>" },
    "gba":     { "core": "mgba",             "url": "<URL .zip vérifiée>" },
    "gb":      { "core": "mgba",             "url": "<URL .zip vérifiée>" },
    "n64":     { "core": "mupen64plus_next", "url": "<URL .zip vérifiée>" },
    "genesis": { "core": "genesis_plus_gx",  "url": "<URL .zip vérifiée>" }
  }
}
```

Les noms de cores ci-dessus (fceumm/snes9x/mgba/mupen64plus_next/
genesis_plus_gx) ont été confirmés existants sur
`buildbot.libretro.com/nightly/windows/x86_64/latest/` pendant le
brainstorming — ce sont des choix communautaires standards par système.
Les URLs exactes/versions précises restent à figer au moment de
l'implémentation (le buildbot nightly change en continu ; utiliser une
version stable numérotée plutôt que "latest" pour la reproductibilité,
sauf si une raison technique impose autrement — à trancher à
l'implémentation).

`EmulatorProvider` télécharge ce manifeste à chaque ouverture de
`EmulatorManagerScreen` (pas de cache long terme). Échec de téléchargement
→ message clair + bouton Réessayer, sans bloquer le reste de l'app (le
réseau ne doit jamais bloquer le fonctionnement offline de la bibliothèque
locale, contrainte déjà établie au socle).

### 2. `EmulatorProvider` (remplace `StubEmulatorProvider`)

Responsabilités :
- **Récupérer et parser le manifeste** (`NetworkManager` existant pour le
  téléchargement).
- **Détecter l'état installé** : vérifie la présence des fichiers attendus
  sur disque (`data/emulators/retroarch/retroarch.exe`,
  `data/emulators/retroarch/cores/<core>_libretro.dll`) — pas de base de
  données séparée, l'app étant portable et les fichiers étant la seule
  source de vérité.
- **Installer** : télécharge (`NetworkManager`) dans `data/emulators/.tmp/`,
  extrait (`miniz` pour les cores `.zip`, `7za.exe` via `QProcess` pour
  RetroArch `.7z`) vers la destination finale, nettoie le fichier temporaire,
  ré-vérifie l'état installé.
- **Désinstaller** : supprime les fichiers d'un core/RetroArch installé via
  l'API Qt standard (jamais de commande shell arbitraire) ; en cas d'échec
  partiel, re-vérifie l'état réel sur disque plutôt que de faire confiance à
  un booléen mémorisé.
- **Lancer un jeu** : résout le core pour le système du jeu, construit la
  commande RetroArch, lance via `QProcess` (non détaché), expose
  `gameLaunched()`/`gameExited(int exitCode)`.
- **Générer la config manette** : au premier lancement de RetroArch, si
  aucun profil autoconfig n'existe encore dans
  `data/emulators/retroarch/autoconfig/`, en génère un à partir du mapping
  SDL2 déjà utilisé par `GamepadBridge`.

### 3. Téléchargement + extraction

1. `NetworkManager.startDownload(url, destPath)` → fichier dans
   `data/emulators/.tmp/`.
2. `NetworkManager::progress` → barre de progression inline sur
   `EmulatorManagerScreen`.
3. `NetworkManager::finished` → extraction :
   - Core (`.zip`) : `miniz`, même mécanisme que le scan d'archives ROM
     (lecture directe, mais ici on **écrit réellement** les fichiers extraits
     sur disque — contrairement au scan de bibliothèque qui reste virtuel,
     ceci n'est pas un ROM utilisateur mais un binaire d'émulateur que l'app
     gère elle-même).
   - RetroArch (`.7z`) : `QProcess` exécutant `7za.exe x <archive> -o<dest>`.
4. Nettoyage du fichier temporaire, mise à jour de l'état installé.
5. Échec à n'importe quelle étape → message d'erreur clair, aucun état
   "installé" n'est marqué, fichiers temporaires nettoyés.

### 4. Lancement d'un jeu

1. `GameDetailsScreen` (ou directement `GameListScreen` si un core est déjà
   disponible) appelle `EmulatorProvider.launchGame(romPath, system)`.
2. Résolution du core installé pour `system` → commande :
   `retroarch.exe -L <core_path> "<romPath>"`.
3. **ROMs dans une archive** (`rom_path = "<archive>::<entry>"`) : à
   l'implémentation, vérifier dans la documentation officielle de RetroArch
   comment il gère le chargement direct depuis une archive (support intégré
   probable, format d'argument à confirmer) — si aucune option propre
   n'existe, extraire l'entrée dans un dossier temporaire avant lancement.
   Point explicitement laissé à vérifier, pas deviné ici.
4. `QProcess` (non détaché) → `gameLaunched()` à l'appel réussi,
   `gameExited(int exitCode)` quand RetroArch se ferme. L'UI peut afficher
   "En cours de jeu..." et suspendre sa propre navigation pendant ce temps.
5. Échec de lancement (fichiers manquants/corrompus malgré un état
   "installé" mémorisé) → message clair, ré-vérification de l'état installé
   plutôt qu'un crash silencieux.

### 5. UI

- **`EmulatorManagerScreen.qml`** : liste dynamique — RetroArch en premier
  (bouton Installer/Désinstaller selon l'état), puis un core par système
  détecté, chacun avec bouton Installer/Désinstaller + barre de progression
  inline. Navigation clavier/manette via `KeyNavigation`, même pattern que
  l'existant.
- **`GameDetailsScreen.qml`** : affiche titre/système du jeu sélectionné
  (déjà dans `LibraryModel`), puis bouton "Lancer" (core disponible) ou
  "Installer un core pour ce système" (poussant vers
  `EmulatorManagerScreen`).
- **`GameListScreen.qml` / `Main.qml`** : le `onAccept` du sous-projet 2
  (qui pousse vers `GameDetails`) devient conditionnel — lance directement
  si un core existe pour le système du jeu focus, sinon comportement actuel
  (ouvre `GameDetails`).

## Vérification

- Test manuel réel : installer RetroArch + au moins un core depuis Bili,
  lancer un vrai jeu (ROM brute et ROM dans une archive), confirmer que ça
  fonctionne, puis désinstaller et confirmer la disparition propre des
  fichiers.
- Couper l'accès réseau pendant le chargement du manifeste : l'app affiche
  un message clair sans planter ni bloquer la bibliothèque locale.
- Annuler un téléchargement en cours : aucun fichier partiel marqué comme
  installé.
- Supprimer manuellement un fichier de core après installation, relancer
  Bili : l'état "installé" reflète la réalité du disque (pas un état
  mémorisé obsolète).
- Tests Qt Test sur `EmulatorProvider` : parsing du manifeste (JSON valide/
  invalide/incomplet), détection d'état installé via `QTemporaryDir`,
  construction de la commande de lancement (testable statiquement, sans
  lancer RetroArch réellement dans la suite automatisée — même pattern que
  `SystemController` au socle) ; téléchargement simulé via un serveur HTTP
  local factice, même pattern que `NetworkManagerTest` existant.
