# Menu en jeu Bili (bouton Home/Guide) — V1 : pause + "Quitter le jeu"

## Contexte

Le sous-projet précédent (fenêtre de jeu intégrée) a fait en sorte que
RetroArch s'affiche à l'intérieur de la fenêtre de Bili (réattachement
Win32 `SetParent`) plutôt que dans une fenêtre séparée. Pendant les tests
manuels de cette fonctionnalité, deux besoins réels sont apparus :

1. **Quitter un jeu à la manette** : RetroArch n'a par défaut aucun bouton
   manette assigné à son raccourci "quitter" (seulement la touche Échap au
   clavier) — déjà corrigé séparément en configurant
   `input_quit_gamepad_combo` (Start+Select) dans le `retroarch.cfg` généré
   par Bili.
2. **Un vrai menu Bili en jeu** : l'utilisateur veut pouvoir appuyer sur le
   bouton Home/Guide de la manette pour mettre le jeu en pause et afficher
   un menu **de Bili** (pas le Quick Menu natif de RetroArch) par-dessus
   l'image figée du jeu, avec au minimum un bouton "Quitter le jeu". Vision
   à plus long terme (hors périmètre ici, voir ci-dessous) : gérer depuis ce
   menu les save states, les shaders, et l'upscaling/la résolution des
   jeux.

Contrainte technique découverte pendant le brainstorming : la fenêtre de
RetroArch est intégrée comme **fenêtre enfant** de la fenêtre de Bili
(`GameWindowEmbedder`, sous-projet précédent). Une fenêtre enfant s'affiche
toujours par-dessus ce que sa fenêtre parente dessine dans cette même
zone — il n'existe donc aucun moyen de faire apparaître le QML rendu
directement par la fenêtre principale de Bili "par-dessus" cette fenêtre
enfant. Afficher un vrai menu Bili par-dessus l'image figée du jeu (choix
retenu après discussion, plutôt que cacher la fenêtre du jeu) demande donc
une **deuxième fenêtre native, elle aussi réattachée comme enfant de la
fenêtre de Bili**, positionnée au-dessus de la fenêtre de RetroArch dans
l'ordre d'empilement (Z-order) des fenêtres sœurs — un mécanisme apparenté
à `GameWindowEmbedder`, mais pour un contenu QML propre à Bili plutôt que
pour une fenêtre de processus tiers.

### Hors périmètre (V1)

- **Save states, shaders, upscaling/résolution** — vision à plus long terme
  explicitement mise de côté ; sous-projet séparé une fois cette base (menu
  + pause + fenêtre superposée) posée et éprouvée. Nécessitera de piloter
  RetroArch au-delà d'une simple pause/quit (probablement via son
  interface de commandes réseau — voir Décisions validées — mais l'étendue
  exacte de ce qui est pilotable ainsi reste à vérifier le moment venu, pas
  devinée ici).
- **Remapper les touches de navigation de Bili lui-même** (haut/bas/gauche/
  droite/valider/annuler dans l'UI de Bili) — question distincte du menu en
  jeu, non traitée ici.
- **Menu en jeu accessible autrement qu'à la manette** (raccourci clavier
  dédié, etc.) — V1 se concentre sur le bouton Home/Guide manette, seul
  déclencheur demandé.
- **Reprendre le jeu depuis le menu** ("Retour au jeu") — la V1 n'a qu'un
  bouton "Quitter le jeu" ; un moyen de fermer le menu sans quitter (Cancel/
  B, ou Home à nouveau) est nécessaire pour ne pas bloquer l'utilisateur,
  voir Architecture ci-dessous, mais aucune autre action de menu n'est
  prévue dans cette V1.

## Décisions validées

- **Déclencheur** : bouton Home/Guide de la manette
  (`SDL_CONTROLLER_BUTTON_GUIDE`, non géré du tout aujourd'hui par
  `GamepadBridge`) — nouveau signal dédié sur `InputManager` (pas de
  réutilisation du signal `menu()` existant, déjà associé à Start dans le
  mapping actuel, pour éviter toute ambiguïté). N'a d'effet que si un jeu
  est actuellement lancé/intégré ; sans jeu en cours, ce bouton ne fait
  rien en V1 (pas de comportement de repli défini).
- **Mise en pause réelle du jeu** (pas juste visuelle) via l'interface de
  **commandes réseau de RetroArch** (`network_cmd_enable`/
  `network_cmd_port` dans `retroarch.cfg`, commandes texte UDP comme
  `PAUSE_TOGGLE`/`QUIT` d'après une recherche initiale non vérifiée en
  profondeur) plutôt qu'en tuant/gelant le process depuis Bili — à
  confirmer par une vraie vérification empirique à l'implémentation
  (documenter la source exacte, tester contre un vrai RetroArch, ne pas
  supposer que les noms de commandes ci-dessus sont corrects tels quels).
  Ce même canal est le candidat naturel pour piloter save states/shaders
  dans un futur sous-projet, ce qui justifie de l'établir maintenant plutôt
  qu'un mécanisme ad hoc limité à pause/quit.
- **Superposition visuelle par une deuxième fenêtre native réattachée**,
  pas en cachant la fenêtre de RetroArch — l'image figée du jeu doit rester
  visible sous le menu (choix explicite, plus proche de l'expérience
  console Switch/PS4 qu'un simple retour à un écran Bili classique).
- **Contenu du menu V1 minimal** : un seul bouton, "Quitter le jeu".
- **Quitter privilégie la commande réseau `QUIT`** (arrêt propre de
  RetroArch) plutôt que `EmulatorProvider` qui tue le process directement ;
  le kill existant (destructeur, chemin d'échec d'`embed()`) reste en
  repli si la commande réseau échoue ou n'est pas confirmée fiable à
  l'implémentation.

## Architecture

### 1. `GamepadBridge` / `InputManager`

Ajoute un cas `SDL_CONTROLLER_BUTTON_GUIDE` dans le switch existant,
émettant un nouveau signal `InputManager::homeMenuRequested()` (nom
provisoire). Même pattern que les autres boutons déjà gérés.

### 2. Commandes réseau RetroArch

Nouvelle classe `core/emulators/RetroArchNetworkCommand` (nom provisoire),
responsabilité unique : envoyer une commande texte à RetroArch via UDP sur
le port configuré. `EmulatorProvider::writePortableRetroArchConfig()` doit
activer `network_cmd_enable`/fixer `network_cmd_port` dans le
`retroarch.cfg` généré. Recherche requise à l'implémentation (pas
supposée ici) : noms de commandes exacts, format du protocole (texte brut
sur UDP à confirmer), comportement si RetroArch n'écoute pas encore au
moment de l'envoi (fenêtre de course possible juste après le lancement).

### 3. Fenêtre de superposition du menu

Nouvelle classe (nom provisoire `GameMenuOverlay`), distincte de
`GameWindowEmbedder` (responsabilité différente : héberger un `QQuickWindow`
propre à Bili plutôt que réattacher la fenêtre d'un process tiers) :
- Crée/possède un second `QQuickWindow` chargeant un petit fichier QML
  dédié au menu en jeu (pas le même contenu que `ui/Main.qml`).
- Réattache ce `QQuickWindow` comme enfant de la même fenêtre hôte que
  `GameWindowEmbedder` (Win32 `SetParent`), le redimensionne pour couvrir
  la zone voulue (probablement toute la zone cliente, ou une zone centrée
  selon maquette à définir), et le place au-dessus de la fenêtre de
  RetroArch dans l'ordre d'empilement des fenêtres sœurs
  (`SetWindowPos(..., HWND_TOP, ...)`).
- Point technique à vérifier à l'implémentation (pas supposé ici) :
  comportement réel du clavier/de la manette une fois deux `QQuickWindow`
  distincts coexistent sous la même fenêtre top-level — lequel reçoit le
  focus, comment le router explicitement vers la fenêtre de menu pendant
  qu'elle est affichée puis le rendre à RetroArch à la fermeture.

### 4. Flux

1. Jeu en cours (déjà intégré via `GameWindowEmbedder`), utilisateur
   appuie sur Home/Guide → `InputManager::homeMenuRequested()`.
2. Envoie `PAUSE_TOGGLE` à RetroArch via `RetroArchNetworkCommand`.
3. Affiche la fenêtre de superposition du menu (`GameMenuOverlay`),
   contenu : bouton "Quitter le jeu" (+ un moyen de fermer le menu sans
   quitter, ex. bouton Cancel/B — comportement exact à définir : revient au
   jeu et renvoie `PAUSE_TOGGLE` pour reprendre l'exécution).
4. "Quitter le jeu" → envoie `QUIT` via `RetroArchNetworkCommand` ; si pas
   de confirmation de succès dans un délai raisonnable, repli sur le kill
   de process déjà existant dans `EmulatorProvider`. Cache/détruit la
   fenêtre de superposition, laisse le flux `gameExited` existant gérer le
   nettoyage normal.

## Vérification

- Recherche réelle requise avant implémentation (pas devinée) : format
  exact et noms de commandes de l'interface réseau de RetroArch
  (documentation officielle et/ou code source RetroArch, comme pour
  chaque intégration RetroArch précédente de ce projet).
- Vérification manuelle réelle : lancer un vrai jeu intégré, appuyer sur
  Home à la manette, confirmer visuellement que le jeu est bien figé/en
  pause (pas juste visuellement recouvert) et que le menu Bili s'affiche
  par-dessus avec l'image du jeu toujours visible dessous, tester
  "Quitter le jeu" (arrêt propre confirmé, pas de process orphelin), et
  tester la fermeture du menu sans quitter si ce chemin est implémenté.
- Tests automatisés : la logique de commande réseau
  (`RetroArchNetworkCommand`) est testable isolément (serveur UDP factice
  en test, même esprit que les tests HTTP factices déjà utilisés dans ce
  projet). Le comportement de fenêtre de superposition réelle (deuxième
  `QQuickWindow` réattaché) suit la même limite que
  `GameWindowEmbedder` : logique de recherche/réattachement testable avec
  un stand-in, comportement visuel réel vérifié à la main.
