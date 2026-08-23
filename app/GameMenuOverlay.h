// app/GameMenuOverlay.h
#pragma once
#include <QObject>
#include <qwindowdefs.h> // WId

class QQuickWindow;
class QImage;
class GameFrameImageProvider;

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
// COMMENT LE VOILE TRANSLUCIDE EST OBTENU (vérifié empiriquement, Task 3 --
// voir task-3-report.md). Le spec demande que l'image figée du jeu reste
// visible À TRAVERS le menu. On NE peut PAS l'obtenir avec la transparence
// de fenêtre native : Qt crée une fenêtre translucide en WS_EX_LAYERED, et
// une fenêtre WS_EX_LAYERED réattachée en WS_CHILD ne dessine plus rien du
// tout ; et même en retirant ce bit, l'alpha d'une fenêtre soeur ne se
// compose pas avec RetroArch, qui dessine dans sa propre surface de
// redirection DWM. L'API DWM Thumbnail ne s'applique pas non plus
// (DwmRegisterThumbnail exige une source top-level et retourne
// E_INVALIDARG sur la fenêtre du jeu une fois réattachée en enfant).
//
// La solution retenue : show() CAPTURE l'image du jeu
// (PrintWindow + PW_RENDERFULLCONTENT, seul mode qui capture réellement le
// rendu GPU de RetroArch) et la publie dans le QML du menu via
// GameFrameImageProvider. L'image du jeu devient donc du contenu QML de
// Bili, que Qt compose lui-même avec le voile semi-transparent et le
// panneau dans UNE seule surface : plus aucune transparence entre fenêtres
// natives n'est nécessaire.
class GameMenuOverlay : public QObject {
    Q_OBJECT
    // Numéro de révision de l'image capturée, à concaténer dans l'URL
    // "image://gameframe/..." côté QML pour forcer un rechargement à chaque
    // ouverture du menu (sans ça, QML réutiliserait l'image précédente).
    Q_PROPERTY(int frameRevision READ frameRevision NOTIFY frameRevisionChanged)
    // false si la capture a échoué : le QML doit alors masquer l'image de
    // fond et le voile, car la fenêtre de menu est repliée sur un panneau
    // centré (voir Fit::CenteredPanel).
    Q_PROPERTY(bool hasGameFrame READ hasGameFrame NOTIFY frameRevisionChanged)

public:
    explicit GameMenuOverlay(QObject *parent = nullptr) : QObject(parent) {}

    // Comment la fenêtre de menu est dimensionnée dans la fenêtre hôte.
    enum class Fit {
        FullClient,    // couvre toute la zone cliente : le QML affiche l'image
                       // capturée du jeu + le voile + le panneau
        CenteredPanel, // repli si la capture échoue : panneau opaque centré,
                       // le jeu reste visible AUTOUR (comportement d'origine,
                       // conservé comme filet de sécurité éprouvé)
    };

    // Le fournisseur d'image auquel show() publie l'image capturée. Non
    // possédé : c'est le QQmlEngine du menu qui en prend la propriété via
    // addImageProvider(). Sans fournisseur, show() fonctionne toujours mais
    // se replie systématiquement sur Fit::CenteredPanel.
    void setFrameProvider(GameFrameImageProvider *provider) { m_frameProvider = provider; }

    // Capture l'image figée du jeu, la publie vers le QML, puis réattache
    // menuWindow comme enfant de hostWindowId et le place en tête de
    // l'ordre d'empilement des fenêtres soeurs -- donc au-dessus de
    // gameWindowId, la fenêtre du jeu intégrée par GameWindowEmbedder.
    //
    // L'appelant doit avoir dimensionné menuWindow à la taille du PANNEAU
    // avant le premier appel (cette taille est mémorisée une fois pour le
    // repli Fit::CenteredPanel), et doit le laisser OPAQUE : une couleur de
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
    //
    // gameWindowId : redonne le focus clavier Win32 au jeu une fois le menu
    // masqué. Nécessaire (revue finale du projet) parce que
    // ShowWindow(SW_HIDE) sur une fenêtre enfant qui avait le focus le
    // transfère à sa fenêtre PARENTE (l'hôte de Bili), pas à une fenêtre
    // soeur -- sans ça, le clavier pilotait l'UI (masquée) de Bili plutôt
    // que le jeu après une reprise, ce qui pouvait relancer le jeu en cours
    // (Entrée -> onAccept de Main.qml -> launchGame() une deuxième fois).
    // Même manœuvre AttachThreadInput()/SetFocus() que
    // GameWindowEmbedder::embed() (core/emulators/GameWindowEmbedder.cpp),
    // pour la même raison : gameWindowId appartient au thread d'entrée d'un
    // autre process (RetroArch), donc un SetFocus() nu serait un no-op
    // silencieux. No-op silencieux aussi si gameWindowId n'est plus une
    // fenêtre valide (jeu déjà terminé, par exemple).
    void hide(QQuickWindow *menuWindow, WId gameWindowId);

    // Réajuste la fenêtre de menu déjà affichée à la zone cliente actuelle
    // de hostWindowId (pleine zone, ou panneau recentré selon le Fit
    // courant). No-op silencieux si show() n'a jamais réussi. Pendant que le
    // menu est ouvert, le jeu est en pause mais la fenêtre de Bili reste
    // redimensionnable.
    void resizeToHost(WId hostWindowId);

    int frameRevision() const { return m_frameRevision; }
    bool hasGameFrame() const { return m_fit == Fit::FullClient; }

    // Détecte une capture silencieusement ratée. PrintWindow() retourne TRUE
    // et remplit une image ENTIÈREMENT NOIRE quand il ne sait pas lire le
    // rendu d'une fenêtre accélérée matériellement -- c'est exactement ce
    // que fait PrintWindow(..., 0) sur RetroArch (mesuré en Task 3), et
    // c'est aussi ce qui arriverait sur Windows 7/8.0, où
    // PW_RENDERFULLCONTENT (Windows 8.1+) n'est qu'un bit ignoré. Sans ce
    // test, une telle capture serait publiée telle quelle et le menu
    // afficherait un grand rectangle noir sous le voile au lieu de se
    // replier sur le panneau centré.
    //
    // Échantillonne une grille de points plutôt que toute l'image (appelé
    // une fois par ouverture du menu, pas par image). Retourne false pour
    // une image nulle : il n'y a alors rien à échantillonner, et ce cas est
    // déjà traité en amont par l'appelant.
    //
    // Faux positif possible et assumé : une scène de jeu réellement toute
    // noire (fondu au noir, écran de chargement) est indiscernable d'une
    // capture ratée. La conséquence est bénigne -- le menu se replie sur le
    // panneau centré opaque, qui est un affichage correct.
    static bool isUniformlyBlack(const QImage &frame);

signals:
    void frameRevisionChanged();

private:
    GameFrameImageProvider *m_frameProvider = nullptr; // non possédé (voir setFrameProvider)
    WId m_overlayWindowId = 0; // 0 = aucune fenêtre de menu actuellement réattachée
    int m_panelWidth = 0;      // taille du panneau de repli, relevée au premier show()
    int m_panelHeight = 0;
    int m_frameRevision = 0;
    Fit m_fit = Fit::CenteredPanel;
};
