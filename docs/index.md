# Index des manuels de référence

Ce fichier recense les manuels téléchargés localement dans `docs/`, pour
être lus à la demande plutôt que devinés. Ne pas télécharger de manuel par
anticipation — seulement quand un point précis de l'implémentation en a
besoin.

| Fichier | Sujet | Ajouté pour |
|---|---|---|
| `core/third_party/miniz/miniz.c` / `miniz.h` (vendored, release 3.1.2, MIT, https://github.com/richgel999/miniz) | Lecture d'archives `.zip` (lister les entrées, extraire leur contenu en mémoire) — bibliothèque C amalgamée en une paire de fichiers unique, sans dépendance système ni DLL à distribuer (compilée en lib statique). | Ajouté pour: sous-projet 2, scan d'archives ROMs |

**Lacune documentée — `.7z` non supporté** : `RomScanner::scanDirectory` ne scanne que les archives `.zip`. Le format `.7z` repose sur LZMA et aucune bibliothèque C/C++ légère et facilement vendorable (à la manière de miniz) n'a été trouvée pour Windows/MinGW lors de la recherche du sous-projet 2 — les options disponibles (SDK LZMA complet, ou wrapper autour des DLL de 7-Zip comme `bit7z`) ajoutent une dépendance de build/runtime nettement plus lourde que ce que ce sous-projet justifie. À reconsidérer si le besoin devient prioritaire.
