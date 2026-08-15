# Sous-projet 2 — Bibliothèque locale (rangement ROMs)

## Contexte

Le socle applicatif (sous-projet 1) est terminé : navigation, stockage
portable, manette/clavier, dossiers ROMs configurables via
`RomSourcesStore`. Mais `GameListScreen` reste un placeholder et
`library.db` (schéma SQLite déjà posé au sous-projet 1) n'est jamais
peuplé — le socle crée le fichier mais rien ne le remplit.

Ce sous-projet livre le scan réel des dossiers ROMs configurés, la
détection du système par fichier, l'indexation dans `library.db`, et un
`GameListScreen` fonctionnel (vraie liste de jeux, navigable au
clavier/manette via le système de focus déjà en place).

Hors périmètre (sous-projets suivants) : téléchargement/lancement des
émulateurs (sous-projet 3), scraping de box art/métadonnées réelles
(sous-projet 4), déduplication avancée multi-disque, tri/filtrage
poussé.

## Décisions validées

- **Indexation virtuelle uniquement** — jamais de déplacement/renommage
  physique des fichiers ROMs. L'appli lit et indexe où les fichiers sont,
  compatible avec des collections déjà organisées par d'autres outils.
- **Support des archives (.zip/.7z) dès le début** — scan à l'intérieur
  des archives pour y détecter des ROMs, pas seulement les fichiers bruts.
- **Nettoyage des titres** — les tags de convention No-Intro/TOSEC
  (`(USA)`, `(Europe)`, `[!]`, `(Rev 1)`, etc.) sont retirés du nom de
  fichier pour produire un titre provisoire lisible, en attendant le
  vrai nom via le scraping (sous-projet 4).
- **Scan automatique incrémental au démarrage** (nouveaux fichiers
  ajoutés/disparus depuis le dernier scan) **+ bouton "Rescanner"** dans
  Réglages pour forcer un scan complet.
- **Pas de déduplication multi-disque/multi-région pour l'instant** —
  chaque fichier détecté devient une entrée distincte dans `library.db`
  (la seule contrainte d'unicité est `rom_path`, déjà en place). Un jeu
  multi-disque (ex: PS1) apparaîtra comme plusieurs entrées séparées —
  accepté comme limitation connue, à raffiner plus tard si besoin.

## Architecture

### 1. `RomScanner` (`core/library/RomScanner.h/.cpp`)

Classe C++ qui, pour chaque `RomSource` activé (`RomSourcesStore::sources()`),
parcourt récursivement le dossier et pour chaque fichier :

- Détecte le système via une table statique extension → système
  (`.nes`→nes, `.sfc`/`.smc`→snes, `.gba`→gba, `.gb`/`.gbc`→gb,
  `.n64`/`.z64`→n64, `.md`/`.gen`→genesis, etc. — liste à établir/étendre
  pendant l'implémentation, pas devinée ici).
- Si le fichier est une archive (`.zip`/`.7z`), l'ouvre et énumère son
  contenu ; chaque fichier interne dont l'extension correspond à un
  système connu devient une entrée virtuelle.
- Nettoie le nom de fichier en titre provisoire (regex retirant les tags
  entre parenthèses/crochets).
- Insère/mets à jour l'entrée correspondante dans `LibraryDatabase`.

**Point à rechercher pendant l'implémentation, pas deviné maintenant** :
Qt n'a pas de support natif pour lire les archives `.7z` (LZMA). Il
faudra identifier la meilleure option (bibliothèque légère type
`libarchive`/`miniz`, ou appel à un outil externe) en consultant la
documentation à jour au moment d'implémenter — `/docs/index.md` du
projet doit être mis à jour avec ce qui est retenu et pourquoi.

### 2. Scan incrémental

Compare la liste actuelle des fichiers d'un `RomSource` avec les entrées
déjà indexées pour ce dossier dans `library.db` : ajoute les nouveaux,
retire les entrées dont le fichier a disparu, laisse les autres
inchangées. Le bouton "Rescanner" déclenche le même algorithme (pas un
mode différent — juste retriggé manuellement plutôt qu'au boot).

### 3. Exécution en arrière-plan

Le scan tourne sur un thread dédié (même pattern que `GamepadBridge` du
socle : `QThread` + signaux), pour ne jamais bloquer l'UI — une
bibliothèque de plusieurs milliers de ROMs avec archives à ouvrir peut
prendre du temps. Émet des signaux de progression consommés par
`GameListScreen` pour afficher un indicateur "Scan en cours (X/Y)"
pendant que la liste se peuple progressivement.

### 4. `LibraryModel` — pont QML

Un modèle Qt exposé à QML (`QAbstractListModel`, pas juste des
`Q_INVOKABLE` renvoyant des `QVariantList` comme `RomSourcesStore` —
une vraie bibliothèque peut contenir des milliers d'entrées, un modèle
Qt natif évite de tout recharger à chaque changement) interrogeant
`LibraryDatabase`. Exposé en tant que `model:` d'une `GridView`/`ListView`
dans `GameListScreen.qml`.

### 5. `GameListScreen.qml`

Remplace le placeholder par une vraie liste/grille de jeux (titre pour
l'instant, pas de box art — ça arrive au sous-projet 4). Navigation
clavier/manette câblée avec le système de focus déjà posé (`KeyNavigation`
+ jetons `Theme.focus*`). `accept` sur un jeu sélectionné ouvre
`GameDetailsScreen` (toujours un placeholder pour l'instant — l'affichage
détaillé réel et le lancement du jeu sont hors périmètre ici).

## Vérification

- Configurer un dossier ROMs de test avec quelques fichiers de systèmes
  différents (bruts + dans une archive), lancer l'appli, confirmer
  qu'ils apparaissent dans `GameListScreen` avec un titre nettoyé.
- Supprimer un fichier du dossier test, relancer l'appli, confirmer que
  l'entrée disparaît de la liste (scan incrémental fonctionne dans les
  deux sens).
- Cliquer "Rescanner" dans Réglages sans rien changer, confirmer que la
  liste reste stable (pas de doublons, pas de perte).
- Naviguer la liste de jeux au clavier et à la manette, confirmer que le
  focus se voit et se déplace correctement.
- Tests unitaires réels sur `RomScanner` (détection système par
  extension, nettoyage de titre, logique incrémentale) sans dépendre
  d'un vrai dossier ROMs pour chaque cas — utiliser `QTemporaryDir` avec
  des fichiers factices, comme fait pour `RomSourcesStore`/`ConfigStore`.
