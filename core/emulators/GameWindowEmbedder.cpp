// core/emulators/GameWindowEmbedder.cpp
#include "GameWindowEmbedder.h"

#ifdef Q_OS_WIN
#include <windows.h>

namespace {

struct FindWindowData {
    DWORD processId = 0;
    HWND result = nullptr;
    DWORD resultThreadId = 0;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto *data = reinterpret_cast<FindWindowData *>(lParam);
    DWORD windowProcessId = 0;
    DWORD windowThreadId = GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId == data->processId && IsWindowVisible(hwnd)) {
        data->result = hwnd;
        data->resultThreadId = windowThreadId;
        return FALSE; // trouvé, arrêter l'énumération
    }
    return TRUE;
}

FindWindowData findTopLevelWindowForProcess(DWORD processId) {
    FindWindowData data{processId, nullptr, 0};
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data;
}

} // namespace

bool GameWindowEmbedder::embed(qint64 processId, WId hostWindowId) {
    const DWORD pid = static_cast<DWORD>(processId);
    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);

    HWND targetHwnd = nullptr;
    DWORD targetThreadId = 0;
    const int stepMs = 100;
    int elapsed = 0;
    while (elapsed <= m_pollTimeoutMs) {
        FindWindowData found = findTopLevelWindowForProcess(pid);
        targetHwnd = found.result;
        targetThreadId = found.resultThreadId;
        if (targetHwnd) break;
        Sleep(stepMs);
        elapsed += stepMs;
    }
    if (!targetHwnd) return false;

    // Reparente d'abord : si SetParent échoue, la fenêtre cible n'a subi
    // aucune mutation (pas de style modifié) -- pas d'état intermédiaire
    // incohérent.
    if (SetParent(targetHwnd, hostHwnd) == nullptr) return false;

    // Retire bordure/barre de titre/boutons système, ajoute WS_CHILD.
    LONG_PTR style = GetWindowLongPtr(targetHwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP);
    style |= WS_CHILD;
    SetWindowLongPtr(targetHwnd, GWL_STYLE, style);

    m_embeddedWindowId = reinterpret_cast<WId>(targetHwnd);
    resizeToHost(hostWindowId);
    SetWindowPos(targetHwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // targetHwnd appartient au thread d'entrée de RetroArch (autre
    // process) -- SetFocus() seul serait un no-op silencieux sans
    // attacher temporairement les files d'entrée des deux threads.
    if (targetThreadId != 0) {
        const DWORD currentThreadId = GetCurrentThreadId();
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);
        SetFocus(targetHwnd);
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
    } else {
        SetFocus(targetHwnd);
    }
    return true;
}

void GameWindowEmbedder::resizeToHost(WId hostWindowId) {
    if (!m_embeddedWindowId) return;
    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);
    const HWND childHwnd = reinterpret_cast<HWND>(m_embeddedWindowId);

    RECT clientRect;
    if (!GetClientRect(hostHwnd, &clientRect)) return;
    MoveWindow(childHwnd, 0, 0,
               clientRect.right - clientRect.left,
               clientRect.bottom - clientRect.top,
               TRUE);
}

#else // !Q_OS_WIN

bool GameWindowEmbedder::embed(qint64, WId) { return false; }
void GameWindowEmbedder::resizeToHost(WId) {}

#endif
