#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <algorithm>
#include "imux_3d_engine.h"

namespace {
constexpr wchar_t kWindowClass[] = L"ImuxGameWindow";
constexpr wchar_t kWindowTitle[] = L"Imux";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (imux_world_wndproc(hwnd, msg, wp, lp)) return 0;

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            imux_world_shutdown();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&wc)) return 10;

    RECT desired{0, 0, 1280, 720};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        desired.right - desired.left,
        desired.bottom - desired.top,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
    if (!hwnd) return 11;

    ShowWindow(hwnd, showCommand == 0 ? SW_SHOWDEFAULT : showCommand);
    UpdateWindow(hwnd);

    if (!imux_world_run(hwnd)) {
        DestroyWindow(hwnd);
        return 12;
    }

    const auto start = std::chrono::steady_clock::now();
    auto previous = start;
    MSG msg{};
    bool running = true;

    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!running) break;

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::clamp(
            std::chrono::duration<float>(now - previous).count(),
            0.0f,
            0.05f
        );
        previous = now;

        imux_world_update(dt);
        imux_world_render();
    }

    if (imux_world_is_running()) {
        imux_world_shutdown();
    }

    return static_cast<int>(msg.wParam);
}
