# Index des manuels de référence

Ce fichier recense les manuels téléchargés localement dans `docs/`, pour
être lus à la demande plutôt que devinés. Ne pas télécharger de manuel par
anticipation — seulement quand un point précis de l'implémentation en a
besoin.

| Fichier | Sujet | Ajouté pour |
|---|---|---|
| `core/third_party/miniz/miniz.c` / `miniz.h` (vendored, release 3.1.2, MIT, https://github.com/richgel999/miniz) | Lecture d'archives `.zip` (lister les entrées, extraire leur contenu en mémoire) — bibliothèque C amalgamée en une paire de fichiers unique, sans dépendance système ni DLL à distribuer (compilée en lib statique). | Ajouté pour: sous-projet 2, scan d'archives ROMs |

**Lacune documentée — `.7z` non supporté** : `RomScanner::scanDirectory` ne scanne que les archives `.zip`. Le format `.7z` repose sur LZMA et aucune bibliothèque C/C++ légère et facilement vendorable (à la manière de miniz) n'a été trouvée pour Windows/MinGW lors de la recherche du sous-projet 2 — les options disponibles (SDK LZMA complet, ou wrapper autour des DLL de 7-Zip comme `bit7z`) ajoutent une dépendance de build/runtime nettement plus lourde que ce que ce sous-projet justifie. À reconsidérer si le besoin devient prioritaire.

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
