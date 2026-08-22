// tests/emulators/TestGuiWindowStandIn.cpp
//
// Stand-in Win32 minimal pour GameWindowEmbedderTest (Task 1) et pour le
// test de nettoyage-à-la-sortie de EmulatorProviderTest (Task 2) : simule
// une application graphique tierce (RetroArch) en créant une vraie fenêtre
// top-level avec un titre connu. Se ferme tout seul après ~2s (WM_TIMER ->
// DestroyWindow), pour que le test de nettoyage à la sortie (qui a besoin
// d'un process qui se termine de lui-même une fois lancé via le vrai
// EmulatorProvider::launchArgs(), donc sans pouvoir lui passer un argument
// personnalisé) observe une vraie sortie de process sans attendre
// longtemps. 2s laisse largement le temps aux tests Task 1
// (embed()/resizeToHost(), qui font leurs assertions en quelques ms puis
// tuent le process explicitement) de terminer avant l'auto-fermeture --
// un kill() explicite sur un process déjà en train de se fermer tout seul
// est un no-op sans danger. Volontairement sans dépendance Qt -- ce n'est
// pas Bili, c'est le "process externe" que GameWindowEmbedder doit savoir
// retrouver et réattacher.
#include <windows.h>

constexpr UINT_PTR kAutoCloseTimerId = 1;
constexpr UINT kAutoCloseDelayMs = 2000;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_TIMER && wParam == kAutoCloseTimerId) {
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"BiliTestStandInWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"BiliTestStandInWindow",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    SetTimer(hwnd, kAutoCloseTimerId, kAutoCloseDelayMs, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
