// core/emulators/GameWindowEmbedder.cpp
#include "GameWindowEmbedder.h"

#ifdef Q_OS_WIN
#include <windows.h>

namespace {

struct FindWindowData {
    DWORD processId = 0;
    HWND result = nullptr;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto *data = reinterpret_cast<FindWindowData *>(lParam);
    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId == data->processId && IsWindowVisible(hwnd)) {
        data->result = hwnd;
        return FALSE; // trouvé, arrêter l'énumération
    }
    return TRUE;
}

HWND findTopLevelWindowForProcess(DWORD processId) {
    FindWindowData data{processId, nullptr};
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.result;
}

} // namespace

bool GameWindowEmbedder::embed(qint64 processId, WId hostWindowId) {
    const DWORD pid = static_cast<DWORD>(processId);
    const HWND hostHwnd = reinterpret_cast<HWND>(hostWindowId);

    HWND targetHwnd = nullptr;
    const int stepMs = 100;
    int elapsed = 0;
    while (elapsed <= m_pollTimeoutMs) {
        targetHwnd = findTopLevelWindowForProcess(pid);
        if (targetHwnd) break;
        Sleep(stepMs);
        elapsed += stepMs;
    }
    if (!targetHwnd) return false;

    // Retire bordure/barre de titre/boutons système, ajoute WS_CHILD.
    LONG_PTR style = GetWindowLongPtr(targetHwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP);
    style |= WS_CHILD;
    SetWindowLongPtr(targetHwnd, GWL_STYLE, style);

    if (SetParent(targetHwnd, hostHwnd) == nullptr) return false;

    m_embeddedWindowId = reinterpret_cast<WId>(targetHwnd);
    resizeToHost(hostWindowId);
    SetWindowPos(targetHwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    SetFocus(targetHwnd);
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
