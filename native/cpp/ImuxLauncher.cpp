#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <utility>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kWindowClass[] = L"ImuxLauncherWindow";
constexpr wchar_t kWindowTitle[] = L"Imux";

ComPtr<ID2D1Factory> g_d2dFactory;
ComPtr<IDWriteFactory> g_writeFactory;
ComPtr<ID2D1HwndRenderTarget> g_target;
ComPtr<ID2D1SolidColorBrush> g_brush;
ComPtr<IDWriteTextFormat> g_title, g_body, g_small, g_button;
ComPtr<ID3D11Device> g_d3dDevice;
ComPtr<ID3D11DeviceContext> g_d3dContext;
ComPtr<ID3DBlob> g_pixelShader;

int g_page = 0;
bool g_drawerOpen = false;
bool g_hoverPlay = false;
bool g_launching = false;
float g_drawerProgress = 0.0f;

struct Rect { float l, t, r, b; };
bool Hit(const Rect& r, float x, float y) { return x >= r.l && x <= r.r && y >= r.t && y <= r.b; }

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

void Color(float r, float g, float b, float a = 1.0f) {
    g_brush->SetColor(D2D1::ColorF(r, g, b, a));
}

void Fill(const Rect& r, float radius = 0.0f) {
    g_target->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(r.l, r.t, r.r, r.b), radius, radius), g_brush.Get());
}

void Text(const wchar_t* value, const Rect& r, const ComPtr<IDWriteTextFormat>& format) {
    g_target->DrawTextW(value, static_cast<UINT32>(wcslen(value)), format.Get(),
        D2D1::RectF(r.l, r.t, r.r, r.b), g_brush.Get());
}

void Render(HWND hwnd) {
    if (!g_target) return;
    g_target->BeginDraw();
    g_target->Clear(D2D1::ColorF(0.035f, 0.043f, 0.055f));

    RECT rc{}; GetClientRect(hwnd, &rc);
    const float width = static_cast<float>(rc.right);
    const float height = static_cast<float>(rc.bottom);

    const float topBar = 72.0f;
    const float margin = width < 1100.0f ? 28.0f : 48.0f;
    const float contentLeft = margin;
    const float contentRight = width - margin;
    const float contentWidth = contentRight - contentLeft;

    // Top bar: the layout is always anchored to the current client rectangle.
    Color(0.045f, 0.055f, 0.070f);
    g_target->FillRectangle(D2D1::RectF(0, 0, width, topBar), g_brush.Get());

    Color(0.30f, 0.82f, 0.62f);
    Text(L"IMUX", {68, 18, 180, 54}, g_title);
    Color(0.52f, 0.56f, 0.63f);
    Text(g_page == 0 ? L"Home" : g_page == 1 ? L"Instances" : g_page == 2 ? L"Engine" : L"Settings",
         {190, 25, 420, 50}, g_body);

    // Hamburger button. Its visual and hit rectangles are the same.
    const Rect menu = {18, 17, 58, 55};
    Color(g_drawerOpen ? 0.12f : 0.075f, g_drawerOpen ? 0.16f : 0.090f, g_drawerOpen ? 0.15f : 0.115f);
    Fill(menu, 12);
    Color(0.88f, 0.91f, 0.95f);
    for (int i = 0; i < 3; ++i)
        g_target->FillRectangle(D2D1::RectF(menu.l + 12, menu.t + 11 + i * 7,
            menu.r - 12, menu.t + 13 + i * 7), g_brush.Get());

    const float availableTop = topBar + 1.0f;
    const float bottomBar = 78.0f;
    const float contentTop = availableTop + 32.0f;
    const float contentBottom = height - bottomBar - 18.0f;
    const float cardGap = width < 1050.0f ? 16.0f : 24.0f;

    Color(0.88f, 0.91f, 0.95f);
    const wchar_t* title =
        g_page == 0 ? L"Your game environment" :
        g_page == 1 ? L"Instances" :
        g_page == 2 ? L"Engine" : L"Settings";
    Text(title, {contentLeft, contentTop, contentRight, contentTop + 44}, g_title);

    Color(0.48f, 0.52f, 0.60f);
    const wchar_t* subtitle =
        g_page == 0 ? L"One controlled profile, one clear launch action." :
        g_page == 1 ? L"Manage isolated game environments." :
        g_page == 2 ? L"Rendering and systems information." :
                      L"Appearance, runtime and advanced developer options.";
    Text(subtitle, {contentLeft + 2, contentTop + 48, contentRight, contentTop + 78}, g_body);

    const float bodyTop = contentTop + 104.0f;

    if (g_page == 0) {
        const float cardBottom = bodyTop + (contentBottom - bodyTop) * 0.68f;
        Color(0.075f, 0.090f, 0.115f);
        Fill({contentLeft, bodyTop, contentRight, cardBottom}, 20);

        Color(0.30f, 0.82f, 0.62f);
        Text(L"DEFAULT PROFILE", {contentLeft + 28, bodyTop + 26, contentRight - 28, bodyTop + 50}, g_small);
        Color(0.93f, 0.95f, 0.98f);
        Text(L"Default Client", {contentLeft + 28, bodyTop + 60, contentRight - 28, bodyTop + 108}, g_title);
        Color(0.48f, 0.52f, 0.60f);
        Text(L"Windows x64  •  Development profile",
             {contentLeft + 30, bodyTop + 116, contentRight - 30, bodyTop + 146}, g_body);

        const float statusY = cardBottom - 62.0f;
        Color(0.060f, 0.075f, 0.095f);
        Fill({contentLeft + 20, statusY, contentRight - 20, cardBottom - 20}, 12);
        Color(0.30f, 0.82f, 0.62f);
        Text(g_launching ? L"Starting development runtime..." : L"Ready",
             {contentLeft + 40, statusY + 12, contentRight - 40, statusY + 40}, g_small);

        const Rect play = {contentRight - 176, height - 64, contentRight, height - 18};
        Color(g_hoverPlay ? 0.36f : 0.30f, g_hoverPlay ? 0.92f : 0.82f,
              g_hoverPlay ? 0.70f : 0.62f);
        Fill(play, 14);
        Color(0.025f, 0.035f, 0.043f);
        Text(g_launching ? L"STARTING" : L"PLAY",
             {play.l + 26, play.t + 11, play.r - 20, play.b - 8}, g_button);
    } else if (g_page == 1) {
        Color(0.075f, 0.090f, 0.115f);
        Fill({contentLeft, bodyTop, contentRight, bodyTop + 92}, 16);
        Color(0.30f, 0.82f, 0.62f);
        Text(L"ACTIVE", {contentLeft + 24, bodyTop + 18, contentLeft + 180, bodyTop + 42}, g_small);
        Color(0.93f, 0.95f, 0.98f);
        Text(L"Default Client", {contentLeft + 24, bodyTop + 46, contentRight - 24, bodyTop + 78}, g_body);

        const float y = bodyTop + 112;
        Color(0.075f, 0.090f, 0.115f);
        Fill({contentLeft, y, contentRight, contentBottom}, 16);
        Color(0.88f, 0.91f, 0.95f);
        Text(L"Runtime", {contentLeft + 24, y + 24, contentRight - 24, y + 52}, g_body);
        Color(0.48f, 0.52f, 0.60f);
        Text(L"Java and process configuration are owned by launcher services.",
             {contentLeft + 24, y + 60, contentRight - 24, y + 88}, g_body);
        Text(L"Mods: none   •   Memory: 1024–4096 MB   •   Status: ready",
             {contentLeft + 24, y + 94, contentRight - 24, y + 122}, g_small);
    } else if (g_page == 2) {
        const wchar_t* names[] = {L"Renderer", L"GPU", L"Native systems", L"Interop", L"Build"};
        const wchar_t* details[] = {
            L"Direct2D / DirectWrite + future D3D12 backend",
            L"D3D11 foundation + HLSL shader pipeline",
            L"C++ / C for Windows and ABI boundaries",
            L"Rust backend connected through stable native contracts",
            L"CMake + Gradle + CI validation"
        };
        const float calculatedRowH = (contentBottom - bodyTop) / 5.0f - 8.0f;\n        const float rowH = calculatedRowH > 44.0f ? calculatedRowH : 44.0f;
        for (int i = 0; i < 5; ++i) {
            const float y = bodyTop + i * (rowH + 8.0f);
            Color(0.075f, 0.090f, 0.115f);
            Fill({contentLeft, y, contentRight, y + rowH}, 12);
            Color(0.30f, 0.82f, 0.62f);
            Text(names[i], {contentLeft + 20, y + 10, contentLeft + 190, y + 34}, g_small);
            Color(0.62f, 0.66f, 0.73f);
            Text(details[i], {contentLeft + 210, y + 10, contentRight - 20, y + 34}, g_small);
        }
    } else {
        const wchar_t* labels[] = {L"Appearance", L"Runtime", L"Storage", L"Advanced"};
        const wchar_t* values[] = {
            L"Material You inspired • adaptive density • motion",
            L"Java 21 • automatic runtime validation",
            L"%APPDATA%\\Imux • JSON configuration • SQLite metadata",
            L"Technology information, diagnostics and native engine options"
        };
        for (int i = 0; i < 4; ++i) {
            const float y = bodyTop + i * 78.0f;
            Color(0.075f, 0.090f, 0.115f);
            Fill({contentLeft, y, contentRight, y + 62}, 14);
            Color(0.88f, 0.91f, 0.95f);
            Text(labels[i], {contentLeft + 20, y + 10, contentLeft + 210, y + 34}, g_body);
            Color(0.48f, 0.52f, 0.60f);
            Text(values[i], {contentLeft + 230, y + 11, contentRight - 20, y + 36}, g_small);
        }
    }

    // Bottom action bar is always full-width; no hard-coded content/sidebar offset.
    Color(0.045f, 0.055f, 0.070f);
    g_target->FillRectangle(D2D1::RectF(0, height - bottomBar, width, height), g_brush.Get());

    Color(0.38f, 0.42f, 0.50f);
    Text(L"Imux", {margin, height - 54, margin + 180, height - 30}, g_small);
    if (g_page == 0) {
        Color(0.30f, 0.82f, 0.62f);
        Text(L"Default Client", {margin + 50, height - 54, margin + 220, height - 30}, g_small);
    }

    // Animated drawer is rendered last so it overlays content consistently.
    if (g_drawerProgress > 0.001f) {
        Color(0.0f, 0.0f, 0.0f, 0.30f * g_drawerProgress);
        g_target->FillRectangle(D2D1::RectF(0, topBar, width, height - bottomBar), g_brush.Get());

        const float drawerWidth = (width * 0.82f < 310.0f ? width * 0.82f : 310.0f);
        const float x = Lerp(-drawerWidth, 0.0f, g_drawerProgress);
        Color(0.055f, 0.067f, 0.085f);
        g_target->FillRectangle(D2D1::RectF(x, 0, x + drawerWidth, height), g_brush.Get());

        Color(0.30f, 0.82f, 0.62f);
        Text(L"IMUX", {x + 28, 30, x + drawerWidth - 24, 66}, g_title);
        Color(0.45f, 0.49f, 0.56f);
        Text(L"NAVIGATION", {x + 30, 76, x + drawerWidth - 24, 100}, g_small);

        const wchar_t* nav[] = {L"Home", L"Instances", L"Engine", L"Settings"};
        for (int i = 0; i < 4; ++i) {
            const Rect item = {x + 16, 118.0f + i * 54.0f, x + drawerWidth - 16, 164.0f + i * 54.0f};
            Color(i == g_page ? 0.15f : 0.075f, i == g_page ? 0.20f : 0.090f, i == g_page ? 0.17f : 0.115f);
            Fill(item, 12);
            Color(i == g_page ? 0.90f : 0.72f, i == g_page ? 0.94f : 0.76f, i == g_page ? 0.92f : 0.82f);
            Text(nav[i], {item.l + 18, item.t + 11, item.r - 16, item.b - 8}, g_body);
        }
        Color(0.38f, 0.42f, 0.50f);
        Text(L"Settings > Advanced > Technology", {x + 28, height - 56, x + drawerWidth - 20, height - 30}, g_small);
    }

    g_target->EndDraw();
}

void InitializeGraphics(HWND hwnd) {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, g_d2dFactory.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_writeFactory.GetAddressOf()));

    RECT rc{}; GetClientRect(hwnd, &rc);
    g_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(
            (rc.right > 1 ? rc.right : 1), (rc.bottom > 1 ? rc.bottom : 1))), &g_target);
    g_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_brush);

    g_writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 30.0f, L"en-us", &g_title);
    g_writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &g_body);
    g_writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &g_small);
    g_writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &g_button);

    D3D_FEATURE_LEVEL featureLevel{};
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, &g_d3dDevice, &featureLevel, &g_d3dContext);

    const char* shader = R"(
        struct PSInput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
        float4 main(PSInput input) : SV_TARGET {
            float glow = 0.04 + 0.04 * sin(input.uv.x * 6.28318);
            return float4(0.12 + glow, 0.28 + glow, 0.21 + glow, 1.0);
        }
    )";
    D3DCompile(shader, strlen(shader), "imux_ui.hlsl", nullptr, nullptr, "main", "ps_5_0",
        0, 0, &g_pixelShader, nullptr);
}

void SetDrawer(HWND hwnd, bool open) {
    g_drawerOpen = open;
    SetTimer(hwnd, 2, 10, nullptr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        InitializeGraphics(hwnd);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMinTrackSize.x = 880;
        info->ptMinTrackSize.y = 600;
        return 0;
    }
    case WM_SIZE:
        if (g_target) {
            const UINT w = static_cast<UINT>(LOWORD(lp));
            const UINT h = static_cast<UINT>(HIWORD(lp));
            if (w > 0 && h > 0) g_target->Resize(D2D1::SizeU(w, h));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE: {
        const float x = static_cast<float>(GET_X_LPARAM(lp));
        const float y = static_cast<float>(GET_Y_LPARAM(lp));
        RECT rc{}; GetClientRect(hwnd, &rc);
        const float width = static_cast<float>(rc.right);
        const float height = static_cast<float>(rc.bottom);
        const float margin = width < 1100.0f ? 28.0f : 48.0f;
        const Rect play = {width - margin - 176, height - 64, width - margin, height - 18};
        const bool next = g_page == 0 && Hit(play, x, y) && !g_drawerOpen;
        if (next != g_hoverPlay) { g_hoverPlay = next; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    }
    case WM_LBUTTONUP: {
        const float x = static_cast<float>(GET_X_LPARAM(lp));
        const float y = static_cast<float>(GET_Y_LPARAM(lp));
        RECT rc{}; GetClientRect(hwnd, &rc);
        const float width = static_cast<float>(rc.right);
        const float height = static_cast<float>(rc.bottom);
        const float margin = width < 1100.0f ? 28.0f : 48.0f;
        const Rect menu = {18, 17, 58, 55};

        if (Hit(menu, x, y)) { SetDrawer(hwnd, !g_drawerOpen); return 0; }

        if (g_drawerOpen) {
            const float drawerWidth = (width * 0.82f < 310.0f ? width * 0.82f : 310.0f);
            if (x <= drawerWidth) {
                for (int i = 0; i < 4; ++i) {
                    const Rect item = {16, 118.0f + i * 54.0f, drawerWidth - 16, 164.0f + i * 54.0f};
                    if (Hit(item, x, y)) {
                        g_page = i; SetDrawer(hwnd, false); InvalidateRect(hwnd, nullptr, FALSE); return 0;
                    }
                }
            }
            if (x > drawerWidth) { SetDrawer(hwnd, false); return 0; }
            return 0;
        }

        const Rect play = {width - margin - 176, height - 64, width - margin, height - 18};
        if (g_page == 0 && Hit(play, x, y)) {
            g_launching = true; InvalidateRect(hwnd, nullptr, FALSE);
            SetTimer(hwnd, 1, 900, nullptr);
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == 1) {
            KillTimer(hwnd, 1); g_launching = false; InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == 2) {
            const float target = g_drawerOpen ? 1.0f : 0.0f;
            const float delta = (target - g_drawerProgress) * 0.24f;
            g_drawerProgress += delta;
            if (std::fabs(target - g_drawerProgress) < 0.01f) {
                g_drawerProgress = target; KillTimer(hwnd, 2);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; BeginPaint(hwnd, &ps); Render(hwnd); EndPaint(hwnd, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.hInstance = instance; wc.lpfnWndProc = WndProc; wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 760, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show); UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return static_cast<int>(msg.wParam);
}
