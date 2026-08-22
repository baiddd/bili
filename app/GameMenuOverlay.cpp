// app/GameMenuOverlay.cpp
#include "GameMenuOverlay.h"
#include "GameFrameImageProvider.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QQuickWindow>
#include <QImage>

// Windows 8.1+. Absent de certains en-têtes MinGW, d'où la définition de
// repli. C'EST le drapeau qui compte : sans lui, PrintWindow() retourne
// pourtant TRUE mais rend une image entièrement noire sur une fenêtre
// accélérée matériellement comme celle de RetroArch (constaté en Task 3 :
// PrintWindow(0) -> image 100% #000000, PW_RENDERFULLCONTENT -> vraie image
// du jeu, identique au pixel près à une capture d'écran).
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace {

// Capture le contenu réel d'une fenêtre (y compris le rendu GPU) dans une
// QImage, sans passer par l'écran : marche donc même si la fenêtre est
// partiellement recouverte. Retourne une image nulle sur échec.
QImage captureWindowContent(HWND hwnd) {
    RECT windowRect;
    if (!IsWindow(hwnd) || !GetWindowRect(hwnd, &windowRect)) return {};
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    if (width <= 0 || height <= 0) return {};

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return {};
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    QImage frame;
    if (memoryDc && bitmap) {
        HGDIOBJ previous = SelectObject(memoryDc, bitmap);
        if (PrintWindow(hwnd, memoryDc, PW_RENDERFULLCONTENT)) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height; // négatif = lignes de haut en bas
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            QImage buffer(width, height, QImage::Format_RGB32);
            if (GetDIBits(memoryDc, bitmap, 0, height, buffer.bits(), &info, DIB_RGB_COLORS)) {
                frame = buffer;
            }
        }
        SelectObject(memoryDc, previous);
    }
    if (bitmap) DeleteObject(bitmap);
    if (memoryDc) DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return frame;
}

} // namespace

bool GameMenuOverlay::show(QQuickWindow *menuWindow, WId hostWindowId, WId gameWindowId) {
    if (!menuWindow || hostWindowId == 0) return false;

    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);
    const HWND gameHwnd = reinterpret_cast<HWND>(gameWindowId);
    if (!IsWindow(hostHwnd)) return false;
    // Pas de jeu intégré = rien à recouvrir : le menu en jeu n'a aucun sens
    // (spec : le bouton Home n'a d'effet que si un jeu est en cours).
    if (!gameHwnd || !IsWindow(gameHwnd)) return false;

    // Capture AVANT tout affichage du menu. PrintWindow lit la surface de la
    // fenêtre elle-même et non l'écran, donc l'ordre n'est pas critique pour
    // l'occlusion -- mais le faire d'abord garde le code lisible et évite de
    // dépendre de ce détail.
    m_fit = Fit::CenteredPanel;
    if (m_frameProvider) {
        const QImage frame = captureWindowContent(gameHwnd);
        m_frameProvider->setFrame(frame);
        if (!frame.isNull()) m_fit = Fit::FullClient;
        ++m_frameRevision;
        emit frameRevisionChanged();
    }

    // winId() force la création de la surface native si elle ne l'est pas
    // encore (QWindow::create()) -- l'appelant n'a donc pas besoin d'avoir
    // fait show() avant pour qu'on obtienne un HWND valide. Vérifié
    // empiriquement (Task 3) : le HWND reste le MÊME avant et après
    // réattachement, Qt ne recrée pas la fenêtre native derrière notre dos,
    // et un appel ultérieur à menuWindow->setVisible(true) ne défait ni le
    // reparentage, ni le restylage, ni l'ordre d'empilement posés ici.
    const HWND menuHwnd = reinterpret_cast<HWND>(menuWindow->winId());
    if (!menuHwnd) return false;

    // Taille du panneau de repli : relevée une seule fois, au premier show(),
    // pendant que la fenêtre a encore la taille que l'appelant lui a donnée.
    // Aux appels suivants elle vaudrait la taille pleine posée par
    // resizeToHost().
    if (m_panelWidth == 0 || m_panelHeight == 0) {
        RECT menuClient;
        if (!GetClientRect(menuHwnd, &menuClient)) return false;
        m_panelWidth = menuClient.right - menuClient.left;
        m_panelHeight = menuClient.bottom - menuClient.top;
        if (m_panelWidth <= 0 || m_panelHeight <= 0) return false;
    }

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

    if (m_fit == Fit::FullClient) {
        // Le QML affiche lui-même l'image capturée du jeu : la fenêtre du
        // menu peut couvrir toute la zone cliente sans rien cacher.
        MoveWindow(menuHwnd, 0, 0, hostWidth, hostHeight, TRUE);
        return;
    }

    // Repli : le panneau garde sa taille propre et reste centré, laissant le
    // jeu visible autour ; il ne se réduit que si la fenêtre de Bili devient
    // plus petite que lui.
    const int width = qMin(m_panelWidth, hostWidth);
    const int height = qMin(m_panelHeight, hostHeight);
    MoveWindow(menuHwnd, (hostWidth - width) / 2, (hostHeight - height) / 2,
               width, height, TRUE);
}

#else // !Q_OS_WIN

bool GameMenuOverlay::show(QQuickWindow *, WId, WId) { return false; }
void GameMenuOverlay::hide(QQuickWindow *) {}
void GameMenuOverlay::resizeToHost(WId) {}

#endif
