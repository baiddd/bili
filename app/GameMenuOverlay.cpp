// app/GameMenuOverlay.cpp
#include "GameMenuOverlay.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QQuickWindow>

bool GameMenuOverlay::show(QQuickWindow *menuWindow, WId hostWindowId, WId gameWindowId) {
    if (!menuWindow || hostWindowId == 0) return false;

    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);
    const HWND gameHwnd = reinterpret_cast<HWND>(gameWindowId);
    if (!IsWindow(hostHwnd)) return false;
    // Pas de jeu intégré = rien à recouvrir : le menu en jeu n'a aucun sens
    // (spec : le bouton Home n'a d'effet que si un jeu est en cours).
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    // winId() force la création de la surface native si elle ne l'est pas
    // encore (QWindow::create()) -- l'appelant n'a donc pas besoin d'avoir
    // fait show() avant pour qu'on obtienne un HWND valide. Vérifié
    // empiriquement (Task 3) : le HWND reste le MÊME avant et après
    // réattachement, Qt ne recrée pas la fenêtre native derrière notre dos,
    // et un appel ultérieur à menuWindow->setVisible(true) ne défait ni le
    // reparentage, ni le restylage, ni l'ordre d'empilement posés ici.
    const HWND menuHwnd = reinterpret_cast<HWND>(menuWindow->winId());
    if (!menuHwnd) return false;

    if (SetParent(menuHwnd, hostHwnd) == nullptr) return false;

    // Même restylage que GameWindowEmbedder : une fenêtre Qt top-level naît
    // WS_POPUP (+ WS_CAPTION|WS_THICKFRAME si elle n'est pas frameless), ce
    // qui laisserait une bordure et un comportement de fenêtre flottante
    // une fois réattachée.
    LONG_PTR style = GetWindowLongPtr(menuHwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP);
    style |= WS_CHILD;
    SetWindowLongPtr(menuHwnd, GWL_STYLE, style);

    // Filet de sécurité, motivé par une observation empirique (Task 3) : une
    // fenêtre WS_EX_LAYERED réattachée en WS_CHILD ne dessine strictement
    // RIEN (capture d'écran : 100% de l'image du jeu, menu totalement
    // invisible, y compris ses parties opaques ; SetLayeredWindowAttributes()
    // réussit sans rien y changer). Qt pose ce bit tout seul dès qu'on donne
    // une couleur de fond translucide à un QQuickWindow -- donc si quelqu'un
    // rend un jour le QML du menu translucide, sans ce retrait le menu
    // disparaîtrait silencieusement au lieu de mal s'afficher. Refait à
    // chaque show() plutôt qu'une fois, au cas où Qt le remette entre deux
    // ouvertures.
    LONG_PTR exStyle = GetWindowLongPtr(menuHwnd, GWL_EXSTYLE);
    SetWindowLongPtr(menuHwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);

    // Taille du panneau = taille que l'appelant a donnée à sa fenêtre de
    // menu (zone cliente, déjà en pixels physiques). Mémorisée ici pour que
    // resizeToHost() puisse la recentrer sans la déformer.
    RECT menuClient;
    if (!GetClientRect(menuHwnd, &menuClient)) return false;
    m_overlayWidth = menuClient.right - menuClient.left;
    m_overlayHeight = menuClient.bottom - menuClient.top;
    if (m_overlayWidth <= 0 || m_overlayHeight <= 0) return false;

    m_overlayWindowId = reinterpret_cast<WId>(menuHwnd);
    resizeToHost(hostWindowId);

    // HWND_TOP place bien la fenêtre en tête de l'ordre d'empilement des
    // fenêtres SOEURS sous le même parent -- vérifié empiriquement (Task 3),
    // avec le stand-in Win32 comme avec le vrai RetroArch : après cet appel,
    // GetWindow(host, GW_CHILD) retourne menuHwnd et la fenêtre du jeu vient
    // après. Aucun positionnement relatif à gameHwnd n'est donc nécessaire.
    // (Piège à éviter : SetWindowPos(menuHwnd, gameHwnd, ...) fait
    // l'INVERSE de ce qu'on veut -- hWndInsertAfter désigne la fenêtre qui
    // doit PRÉCÉDER menuHwnd, ça met donc le menu SOUS le jeu. Constaté
    // pendant la mise au point de cette tâche.)
    SetWindowPos(menuHwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

    ShowWindow(menuHwnd, SW_SHOW);
    SetFocus(menuHwnd);
    return true;
}

void GameMenuOverlay::hide(QQuickWindow *menuWindow) {
    if (!menuWindow) return;
    ShowWindow(reinterpret_cast<HWND>(menuWindow->winId()), SW_HIDE);
}

void GameMenuOverlay::resizeToHost(WId hostWindowId) {
    if (!m_overlayWindowId) return;
    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);
    const HWND menuHwnd = reinterpret_cast<HWND>(m_overlayWindowId);

    RECT hostClient;
    if (!GetClientRect(hostHwnd, &hostClient)) return;
    const int hostWidth = hostClient.right - hostClient.left;
    const int hostHeight = hostClient.bottom - hostClient.top;

    // Le panneau garde sa taille propre et reste centré ; il ne se réduit
    // que si la fenêtre de Bili devient plus petite que lui.
    const int width = qMin(m_overlayWidth, hostWidth);
    const int height = qMin(m_overlayHeight, hostHeight);
    MoveWindow(menuHwnd, (hostWidth - width) / 2, (hostHeight - height) / 2,
               width, height, TRUE);
}

#else // !Q_OS_WIN

bool GameMenuOverlay::show(QQuickWindow *, WId, WId) { return false; }
void GameMenuOverlay::hide(QQuickWindow *) {}
void GameMenuOverlay::resizeToHost(WId) {}

#endif
