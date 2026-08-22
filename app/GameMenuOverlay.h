// app/GameMenuOverlay.h
#pragma once
#include <qwindowdefs.h> // WId

class QQuickWindow;

// Réattache une fenêtre de menu Bili (un QQuickWindow créé et possédé par
// l'appelant, voir app/main.cpp) comme fenêtre SOEUR de celle du jeu
// actuellement intégré (GameWindowEmbedder), au-dessus d'elle dans l'ordre
// d'empilement -- pas une fenêtre flottante séparée. Nécessaire parce
// qu'une fenêtre enfant Win32 (celle de RetroArch) recouvre toujours ce
// que sa fenêtre parente dessine dans la même zone : aucun QML rendu par
// la fenêtre principale de Bili ne peut apparaître par-dessus le jeu.
//
// Fonctionnalité strictement Windows, comme GameWindowEmbedder : ailleurs,
// show() échoue silencieusement (retourne false) et hide()/resizeToHost()
// sont des no-op -- jamais une erreur de compilation, pour ne pas casser
// les presets Linux/RPi/Android.
//
// Vit dans app/ et non dans core/emulators/ (contrairement à
// GameWindowEmbedder) parce que cette classe manipule un QQuickWindow,
// donc dépend du module Qt6::Quick que `bili-core` ne lie volontairement
// pas -- même raison pour laquelle c'est déjà app/main.cpp, et non core/,
// qui récupère le WId de la fenêtre racine pour
// EmulatorProvider::setHostWindowId().
//
// POURQUOI UN PANNEAU CENTRÉ ET NON UN VOILE PLEIN ÉCRAN (vérifié
// empiriquement, Task 3 -- voir task-3-report.md) : le spec demande que
// l'image figée du jeu reste visible pendant que le menu est ouvert. Un
// QQuickWindow translucide couvrant toute la zone cliente ne permet PAS
// d'y arriver au-dessus du vrai RetroArch : RetroArch dessine dans sa
// propre surface de redirection DWM (swapchain matérielle), et l'alpha de
// la fenêtre soeur placée au-dessus ne se compose pas avec elle -- le jeu
// disparaît quand même. Cette fenêtre de menu est donc OPAQUE et ne couvre
// que la taille que l'appelant lui a donnée, centrée : le jeu reste
// visible tout AUTOUR du panneau.
class GameMenuOverlay {
public:
    // Réattache menuWindow comme enfant de hostWindowId, le centre dans la
    // zone cliente de l'hôte en gardant sa propre taille, et le place en
    // tête de l'ordre d'empilement des fenêtres soeurs -- donc au-dessus de
    // gameWindowId, la fenêtre du jeu déjà intégrée par GameWindowEmbedder.
    //
    // L'appelant doit avoir dimensionné menuWindow (c'est cette taille qui
    // devient celle du panneau) et doit le laisser OPAQUE : une couleur de
    // fond translucide ferait poser WS_EX_LAYERED par Qt, et une fenêtre
    // WS_EX_LAYERED réattachée en enfant ne dessine plus rien du tout.
    //
    // Retourne false sans rien afficher si gameWindowId n'est pas une
    // fenêtre valide (aucun jeu en cours : le menu en jeu n'aurait rien à
    // recouvrir), et sur tout échec Win32. Ne lève jamais d'exception.
    bool show(QQuickWindow *menuWindow, WId hostWindowId, WId gameWindowId);

    // Cache la fenêtre de menu sans la détruire -- l'appelant la garde
    // vivante entre deux ouvertures (recharger le QML à chaque ouverture
    // reconstruirait toute la scène pour rien). Un show() ultérieur sur la
    // même fenêtre refonctionne tel quel (vérifié en Task 3).
    void hide(QQuickWindow *menuWindow);

    // Recentre la fenêtre de menu déjà affichée dans la zone cliente
    // actuelle de hostWindowId. No-op silencieux si show() n'a jamais
    // réussi. Pendant que le menu est ouvert, le jeu est en pause mais la
    // fenêtre de Bili reste redimensionnable.
    void resizeToHost(WId hostWindowId);

private:
    WId m_overlayWindowId = 0; // 0 = aucune fenêtre de menu actuellement réattachée
    int m_overlayWidth = 0;    // taille du panneau, en pixels physiques, relevée au show()
    int m_overlayHeight = 0;
};
