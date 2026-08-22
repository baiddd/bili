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
