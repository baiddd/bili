# Index des manuels de référence

Ce fichier recense les manuels téléchargés localement dans `docs/`, pour
être lus à la demande plutôt que devinés. Ne pas télécharger de manuel par
anticipation — seulement quand un point précis de l'implémentation en a
besoin.

| Fichier | Sujet | Ajouté pour |
|---|---|---|
| `core/third_party/miniz/miniz.c` / `miniz.h` (vendored, release 3.1.2, MIT, https://github.com/richgel999/miniz) | Lecture d'archives `.zip` (lister les entrées, extraire leur contenu en mémoire) — bibliothèque C amalgamée en une paire de fichiers unique, sans dépendance système ni DLL à distribuer (compilée en lib statique). | Ajouté pour: sous-projet 2, scan d'archives ROMs |
| `platform/windows/tools/7za.exe` (vendored, 7-Zip **26.02** (2026-06-25), source : `https://github.com/ip7z/7zip/releases/download/26.02/7z2602-extra.7z` — le lien "7-Zip Extra: standalone console version" de la page officielle `https://www.7-zip.org/download.html`. Licence : voir note ci-dessous, texte complet vendored dans `platform/windows/tools/7za-LICENSE.txt`. | Extraction en ligne de commande (via `QProcess`) de `RetroArch.7z`, le seul format sous lequel RetroArch lui-même est distribué en dehors de l'installeur `.exe` (interdit — Bili est portable, sans installeur). | Ajouté pour: sous-projet 3, installation de RetroArch (tâche 4) |

**Lacune documentée — `.7z` non supporté (scan de ROMs uniquement)** : `RomScanner::scanDirectory` ne scanne que les archives `.zip`. Le format `.7z` repose sur LZMA et aucune bibliothèque C/C++ légère et facilement vendorable (à la manière de miniz) n'a été trouvée pour Windows/MinGW lors de la recherche du sous-projet 2 — les options disponibles (SDK LZMA complet, ou wrapper autour des DLL de 7-Zip comme `bit7z`) ajoutent une dépendance de build/runtime nettement plus lourde que ce que ce sous-projet justifie. À reconsidérer si le besoin devient prioritaire. **Cette lacune ne concerne que le scan d'archives ROMs** : l'installation de RetroArch lui-même (sous-projet 3, tâche 4) utilise un mécanisme entièrement séparé — `EmulatorProvider::extract7zArchive()` shell-out vers le `7za.exe` vendored ci-dessus via `QProcess`, sans passer par `RomScanner` ni par une bibliothèque `.7z` en C++ — donc ce n'est pas une réouverture de la lacune ci-dessus, simplement un besoin différent résolu différemment.

**Correction sur la licence de `7za.exe`** : le plan de la tâche 4 supposait "le cœur de 7-Zip, dont `7za.exe`, est dans le domaine public" — **vérification faite sur le fichier `License.txt` réellement fourni dans le package `7z-extra`, cette hypothèse est fausse pour `7za.exe`.** Le fichier `License.txt` du package indique explicitement :
- `7za.exe` : licence **GNU LGPL** (licence principale du code), plus **BSD 3-clause** (code de décodage ZSTD) et **BSD 2-clause** (code XXH64) pour certaines portions.
- Tous les autres fichiers du package : **GNU LGPL**.
- Seul `7zr.exe` (un exécutable *différent*, fourni par le "LZMA SDK", un téléchargement distinct de "7-Zip Extra") est en domaine public — `7za.exe` n'en fait pas partie.

Conséquence pratique (FAQ de licence du `readme.txt` du package) : redistribuer `7za.exe` en binaire est autorisé, y compris commercialement, à condition de documenter (1) l'utilisation de composants de 7-Zip, (2) que 7-Zip est sous licence GNU LGPL, et (3) un lien vers `www.7-zip.org`. Cette page en tient lieu ; le texte de licence complet est aussi vendored tel quel dans `platform/windows/tools/7za-LICENSE.txt`.

## Installation de RetroArch — mode portable (sous-projet 3, tâche 4)

Recherche effectuée en téléchargeant réellement `RetroArch.7z` (URL du
manifeste `catalog/emulators.json`, version stable 1.22.2) et en l'extrayant
avec un 7-Zip local, puis en consultant le code source de RetroArch
(`frontend/drivers/platform_win32.c` et
`libretro-common/file/file_path.c::fill_pathname_expand_special`) :

- **Le build Windows `.7z` de RetroArch est portable par défaut.** Toutes les
  clés de répertoire (`system_directory`, `savefile_directory`,
  `savestate_directory`, ...) sont commentées (non définies) dans le
  `retroarch.default.cfg` livré, ce qui laisse RetroArch utiliser ses valeurs
  compilées en dur — des chemins préfixés par `:` (ex. `:\system`,
  `:\saves`, `:\states`) qui se résolvent **relativement au dossier contenant
  retroarch.exe lui-même** (`fill_pathname_expand_special` appelle
  `fill_pathname_application_dir()` pour ce préfixe). L'archive contient
  d'ailleurs déjà des dossiers vides `system/`, `saves/`, `states/`,
  `screenshots/`, `shaders/` à côté de `retroarch.exe`, cohérent avec ce
  comportement. Au premier lancement, RetroArch cherche aussi `retroarch.cfg`
  dans le dossier de son propre exécutable *avant* `%APPDATA%`, et en crée un
  à cet endroit s'il n'en trouve nulle part.
- **Point de vigilance découvert en pratique** : l'archive `RetroArch.7z`
  place tout son contenu sous un unique dossier racine (`RetroArch-Win64/`)
  plutôt qu'à la racine de l'archive, et `7za.exe x` n'a pas d'équivalent au
  `--strip-components` de `tar`. `EmulatorProvider::extract7zArchive()`
  détecte ce cas (un seul sous-dossier contenant `retroarch.exe` après
  extraction) et remonte son contenu d'un niveau, pour que
  `retroArchExecutablePath()` (`<retroArchDir>/retroarch.exe`) trouve bien
  l'exécutable. Ce remonte-niveau est un no-op pour le stub `7za.exe` de test
  (qui écrit `retroarch.exe` directement, sans dossier imbriqué).
- **Implémentation retenue** : bien que l'extraction seule suffise déjà
  (portable par défaut), `EmulatorProvider::writePortableRetroArchConfig()`
  écrit tout de même un `retroarch.cfg` explicite fixant
  `system_directory`, `savefile_directory`, `savestate_directory`,
  `screenshot_directory`, `cache_directory` et `libretro_directory` (ce
  dernier pointé vers `coresDir()`, là où `EmulatorProvider` installe déjà
  les cores) sous `data/emulators/retroarch/`. Objectif : ne pas faire
  reposer la garantie « rien en dehors de son propre dossier » de Bili sur le
  maintien futur de ces valeurs par défaut par RetroArch, plutôt que combler
  un vrai manque de support portable (qui n'existe pas).

## Catalogue de téléchargement des émulateurs (sous-projet 3)

`EmulatorCatalog` (`core/emulators/EmulatorCatalog.h`/`.cpp`) télécharge et
parse à l'exécution un manifeste JSON qui indique où récupérer RetroArch et
les cores libretro, plutôt que de coder ces URLs en dur dans l'application.
Ce manifeste est un fichier de **ce dépôt** (`catalog/emulators.json`),
publié via l'URL brute GitHub :

```
https://raw.githubusercontent.com/baiddd/bili/master/catalog/emulators.json
```

Ce choix (manifeste versionné dans le dépôt Bili, plutôt qu'une dépendance
directe et codée en dur sur l'arborescence du buildbot libretro) découple
« quoi télécharger » d'une release de Bili : mettre à jour les URLs/versions
ne nécessite qu'un commit + push sur `catalog/emulators.json`, sans
recompiler ni republier l'application.

Recherche effectuée le 2026-08-21 sur `https://buildbot.libretro.com/` pour
renseigner le contenu réel du manifeste (voir
`.superpowers/sdd/2026-08-21-gestion-emulateurs/task-2-report.md` pour le
détail des vérifications) :

| Élément | Choix retenu | Pourquoi |
|---|---|---|
| RetroArch (Windows x86_64) | Release **stable** `1.22.2`, fichier `RetroArch.7z` sous `https://buildbot.libretro.com/stable/1.22.2/windows/x86_64/` | C'est la dernière version stable listée sous `stable/` au moment de la recherche (confirmée par listing de répertoire) ; une release stable numérotée est reproductible (l'URL ne changera pas sous les pieds de l'app), contrairement à un flux nightly. |
| Cores libretro (`fceumm`, `snes9x`, `mgba`, `mupen64plus_next`, `genesis_plus_gx`) | Builds **nightly** (`_libretro.dll.zip`) sous `https://buildbot.libretro.com/nightly/windows/x86_64/latest/` | Le buildbot libretro ne publie de builds *numérotés/datés* que pour l'application RetroArch elle-même (dossiers datés `AAAA-MM-JJ_RetroArch.7z` sous `nightly/windows/x86_64/`) ; les cores individuels n'existent que sous `nightly/.../latest/` (pas de dossier daté équivalent par core), donc "latest" nightly est la seule option disponible pour les cores — choix nightly documenté et accepté par le plan pour ce cas. |
