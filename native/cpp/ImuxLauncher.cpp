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
#include <fstream>
#include <string>
#include <exception>
#include <cstdarg>
#include "imux_3d_engine.h"

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

std::ofstream g_log;
std::string g_logPath;

void Log(const char* format, ...) {
    if (!g_log.is_open()) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char message[2048]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);
    g_log << '[' << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '.' << st.wMilliseconds << "] " << message << '\\n';
    g_log.flush();
}

LONG WINAPI ImuxUnhandledException(EXCEPTION_POINTERS* info) {
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    const void* address = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
    Log("FATAL unhandled exception: code=0x%08lX address=%p", static_cast<unsigned long>(code), address);
    return EXCEPTION_EXECUTE_HANDLER;
}

void ImuxTerminate() {
    Log("FATAL std::terminate called");
    std::abort();
}

void InitializeLogging() {
    char cwd[MAX_PATH]{};
    DWORD length = GetCurrentDirectoryA(MAX_PATH, cwd);
    g_logPath = length > 0 && length < MAX_PATH ? std::string(cwd) + "\\imux.log" : "imux.log";
    g_log.open(g_logPath, std::ios::out | std::ios::app);
    if (g_log.is_open()) {
        Log("========== Imux launcher start ==========");
        Log("Working directory: %s", length > 0 ? cwd : ".");
        Log("Log file: %s", g_logPath.c_str());
    }
}

int g_page = 0;
bool g_drawerOpen = false;
bool g_hoverPlay = false;
bool g_launching = false;
bool g_inWorld = false;
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
    const float topBar = 68.0f;
    const float bottomBar = 72.0f;

    Color(0.045f, 0.055f, 0.070f);
    g_target->FillRectangle(D2D1::RectF(0, 0, width, topBar), g_brush.Get());

    const Rect menu{16, 14, 58, 54};
    Color(g_drawerOpen ? 0.13f : 0.075f, g_drawerOpen ? 0.18f : 0.09f, g_drawerOpen ? 0.16f : 0.115f);
    Fill(menu, 12);
    Color(0.88f, 0.91f, 0.95f);
    for (int i = 0; i < 3; ++i)
        g_target->FillRectangle(D2D1::RectF(menu.l + 12, menu.t + 10 + i * 7, menu.r - 12, menu.t + 12 + i * 7), g_brush.Get());

    Color(0.30f, 0.82f, 0.62f);
    Text(L"IMUX", {76, 18, 170, 52}, g_title);
    Color(0.52f, 0.56f, 0.63f);
    const wchar_t* pageName = g_page == 0 ? L"Home" : g_page == 1 ? L"Instances" : g_page == 2 ? L"Engine" : L"Settings";
    Text(pageName, {184, 23, 420, 48}, g_body);

    // Content is centered against the real client width. It never reserves an invisible sidebar.
    const float edge = width < 1000.0f ? 20.0f : 40.0f;
    const float maxWidth = 1180.0f;
    const float usable = width - edge * 2.0f;
    const float contentWidth = usable < maxWidth ? usable : maxWidth;
    const float left = (width - contentWidth) * 0.5f;
    const float right = left + contentWidth;
    const float top = 94.0f;
    const float bottom = height - bottomBar - 20.0f;

    Color(0.93f, 0.95f, 0.98f);
    const wchar_t* titles[] = {L"Your game environment", L"Instances", L"Engine", L"Settings"};
    Text(titles[g_page], {left, top, right, top + 38}, g_title);
    Color(0.48f, 0.52f, 0.60f);
    const wchar_t* subtitles[] = {
        L"One clear launch action. Everything else stays out of the way.",
        L"Isolated environments for the Imux client.",
        L"Native rendering and systems foundation.",
        L"Appearance, runtime and advanced configuration."
    };
    Text(subtitles[g_page], {left + 2, top + 42, right, top + 70}, g_body);

    const float bodyTop = top + 88.0f;
    const float gap = width < 1050.0f ? 14.0f : 20.0f;

    if (g_page == 0) {
        const bool compact = contentWidth < 900.0f;
        const float heroH = (bottom - bodyTop) < 340.0f ? (bottom - bodyTop) : 340.0f;
        const float heroBottom = bodyTop + heroH;
        const float mainRight = compact ? right : left + contentWidth * 0.64f - gap * 0.5f;
        const float sideLeft = compact ? left : left + contentWidth * 0.64f + gap * 0.5f;

        Color(0.075f, 0.090f, 0.115f);
        Fill({left, bodyTop, mainRight, heroBottom}, 24);
        Color(0.30f, 0.82f, 0.62f);
        Text(L"ACTIVE PROFILE", {left + 28, bodyTop + 26, mainRight - 24, bodyTop + 50}, g_small);
        Color(0.94f, 0.96f, 0.99f);
        Text(L"Default Client", {left + 28, bodyTop + 58, mainRight - 24, bodyTop + 104}, g_title);
        Color(0.52f, 0.56f, 0.63f);
        Text(L"Windows x64  •  Development environment",
             {left + 30, bodyTop + 112, mainRight - 24, bodyTop + 140}, g_body);

        Color(0.060f, 0.075f, 0.095f);
        Fill({left + 22, heroBottom - 82, mainRight - 22, heroBottom - 22}, 14);
        Color(0.30f, 0.82f, 0.62f);
        Text(L"READY", {left + 42, heroBottom - 69, left + 122, heroBottom - 45}, g_small);
        Color(0.50f, 0.54f, 0.61f);
        Text(L"Services ready", {left + 132, heroBottom - 69, mainRight - 38, heroBottom - 45}, g_small);

        if (!compact) {
            Color(0.075f, 0.090f, 0.115f);
            Fill({sideLeft, bodyTop, right, heroBottom}, 24);
            Color(0.88f, 0.91f, 0.95f);
            Text(L"Quick status", {sideLeft + 24, bodyTop + 26, right - 24, bodyTop + 54}, g_body);
            const wchar_t* labels[] = {L"Runtime", L"Memory", L"Mods"};
            const wchar_t* values[] = {L"Java 21", L"1–4 GB", L"None"};
            for (int i = 0; i < 3; ++i) {
                const float y = bodyTop + 78 + i * 72;
                Color(0.42f, 0.46f, 0.53f);
                Text(labels[i], {sideLeft + 24, y, right - 24, y + 24}, g_small);
                Color(0.86f, 0.89f, 0.93f);
                Text(values[i], {sideLeft + 24, y + 25, right - 24, y + 50}, g_body);
            }
        }
    } else if (g_page == 1) {
        Color(0.075f, 0.090f, 0.115f);
        Fill({left, bodyTop, right, bodyTop + 104}, 20);
        Color(0.30f, 0.82f, 0.62f);
        Text(L"ACTIVE", {left + 24, bodyTop + 20, left + 150, bodyTop + 44}, g_small);
        Color(0.93f, 0.95f, 0.98f);
        Text(L"Default Client", {left + 24, bodyTop + 46, right - 24, bodyTop + 78}, g_body);

        const float y = bodyTop + 120;
        Color(0.075f, 0.090f, 0.115f);
        Fill({left, y, right, bottom}, 20);
        Color(0.88f, 0.91f, 0.95f);
        Text(L"Environment details", {left + 24, y + 24, right - 24, y + 52}, g_body);
        Color(0.48f, 0.52f, 0.60f);
        Text(L"Version and runtime selection are managed by launcher services.",
             {left + 24, y + 62, right - 24, y + 90}, g_body);
        Text(L"Ready   •   Java 21   •   1024–4096 MB   •   No mods",
             {left + 24, y + 100, right - 24, y + 126}, g_small);
    } else if (g_page == 2) {
        const wchar_t* names[] = {L"Renderer", L"GPU", L"Native systems", L"Interop", L"Build"};
        const wchar_t* details[] = {
            L"Direct2D / DirectWrite UI renderer",
            L"D3D11 test world + HLSL pipeline",
            L"C++ / C runtime and ABI",
            L"Rust engine services",
            L"CMake + Gradle + CI validation"
        };
        const float row = (bottom - bodyTop - 32.0f) / 5.0f;
        const float rowH = row > 50.0f ? row : 50.0f;
        for (int i = 0; i < 5; ++i) {
            const float y = bodyTop + i * (rowH + 8.0f);
            Color(0.075f, 0.090f, 0.115f);
            Fill({left, y, right, y + rowH}, 14);
            Color(0.30f, 0.82f, 0.62f);
            Text(names[i], {left + 20, y + 13, left + 205, y + 38}, g_small);
            Color(0.62f, 0.66f, 0.73f);
            Text(details[i], {left + 205, y + 12, right - 20, y + 40}, g_small);
        }
    } else {
        const wchar_t* labels[] = {L"Appearance", L"Runtime", L"Storage", L"Advanced"};
        const wchar_t* values[] = {
            L"Material You inspired • adaptive density • motion",
            L"Java 21 • automatic runtime validation",
            L"%APPDATA%\\Imux • JSON configuration • SQLite metadata",
            L"Diagnostics and renderer configuration"
        };
        const float row = (bottom - bodyTop - 24.0f) / 4.0f;
        const float rowH = row > 62.0f ? row : 62.0f;
        for (int i = 0; i < 4; ++i) {
            const float y = bodyTop + i * (rowH + 8.0f);
            Color(0.075f, 0.090f, 0.115f);
            Fill({left, y, right, y + rowH}, 16);
            Color(0.90f, 0.93f, 0.96f);
            Text(labels[i], {left + 22, y + 14, left + 230, y + 40}, g_body);
            Color(0.50f, 0.54f, 0.61f);
            Text(values[i], {left + 230, y + 15, right - 22, y + 42}, g_small);
        }
    }

    Color(0.045f, 0.055f, 0.070f);
    g_target->FillRectangle(D2D1::RectF(0, height - bottomBar, width, height), g_brush.Get());
    Color(0.38f, 0.42f, 0.50f);
    Text(L"Imux", {left, height - 49, left + 100, height - 24}, g_small);

    if (g_page == 0) {
        const Rect play{right - 196, height - 60, right, height - 16};
        Color(g_hoverPlay ? 0.36f : 0.30f, g_hoverPlay ? 0.92f : 0.82f, g_hoverPlay ? 0.70f : 0.62f);
        Fill(play, 14);
        Color(0.025f, 0.035f, 0.043f);
        Text(L"PLAY", {play.l + 26, play.t + 10, play.r - 18, play.b - 7}, g_button);
        Color(0.38f, 0.42f, 0.50f);
        Text(L"Test 3D world", {right - 320, height - 48, right - 212, height - 25}, g_small);
    }

    if (g_drawerProgress > 0.001f) {
        Color(0.0f, 0.0f, 0.0f, 0.30f * g_drawerProgress);
        g_target->FillRectangle(D2D1::RectF(0, topBar, width, height - bottomBar), g_brush.Get());

        const float drawerWidth = width * 0.78f < 330.0f ? width * 0.78f : 330.0f;
        const float x = Lerp(-drawerWidth, 0.0f, g_drawerProgress);
        Color(0.055f, 0.067f, 0.085f);
        g_target->FillRectangle(D2D1::RectF(x, 0, x + drawerWidth, height), g_brush.Get());
        Color(0.30f, 0.82f, 0.62f);
        Text(L"IMUX", {x + 28, 30, x + drawerWidth - 24, 66}, g_title);
        Color(0.45f, 0.49f, 0.56f);
        Text(L"NAVIGATION", {x + 30, 76, x + drawerWidth - 24, 100}, g_small);

        const wchar_t* nav[] = {L"Home", L"Instances", L"Engine", L"Settings"};
        for (int i = 0; i < 4; ++i) {
            const Rect item{x + 16, 118.0f + i * 54.0f, x + drawerWidth - 16, 164.0f + i * 54.0f};
            Color(i == g_page ? 0.15f : 0.075f, i == g_page ? 0.20f : 0.090f, i == g_page ? 0.17f : 0.115f);
            Fill(item, 12);
            Color(i == g_page ? 0.90f : 0.72f, i == g_page ? 0.94f : 0.76f, i == g_page ? 0.92f : 0.82f);
            Text(nav[i], {item.l + 18, item.t + 11, item.r - 16, item.b - 8}, g_body);
        }
        Color(0.38f, 0.42f, 0.50f);
        Text(L"Settings > Advanced", {x + 28, height - 56, x + drawerWidth - 20, height - 30}, g_small);
    }

    g_target->EndDraw();
}
void InitializeGraphics(HWND hwnd) {
    Log("InitializeGraphics: begin");
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
    HRESULT d3dHr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, &g_d3dDevice, &featureLevel, &g_d3dContext);
    Log("D3D11CreateDevice: hr=0x%08lX featureLevel=0x%08lX", static_cast<unsigned long>(d3dHr), static_cast<unsigned long>(featureLevel));

    const char* shader = R"(
        struct PSInput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
        float4 main(PSInput input) : SV_TARGET {
            float glow = 0.04 + 0.04 * sin(input.uv.x * 6.28318);
            return float4(0.12 + glow, 0.28 + glow, 0.21 + glow, 1.0);
        }
    )";
    HRESULT shaderHr = D3DCompile(shader, strlen(shader), "imux_ui.hlsl", nullptr, nullptr, "main", "ps_5_0",
        0, 0, &g_pixelShader, nullptr);
    Log("D3DCompile: hr=0x%08lX", static_cast<unsigned long>(shaderHr));
    Log("InitializeGraphics: complete");
}

void SetDrawer(HWND hwnd, bool open) {
    g_drawerOpen = open;
    SetTimer(hwnd, 2, 10, nullptr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_inWorld) {
        if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
            imux_world_shutdown();
            g_inWorld = false;
            InitializeGraphics(hwnd);
            SetTimer(hwnd, 3, 16, nullptr);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (msg == WM_SIZE) {
            imux_world_wndproc(hwnd, msg, wp, lp);
            return 0;
        }
        if (msg == WM_TIMER && wp == 3) {
            imux_world_update(0.016f);
            imux_world_render();
            return 0;
        }
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps{}; BeginPaint(hwnd, &ps); EndPaint(hwnd, &ps); return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
    case WM_CREATE:
        InitializeGraphics(hwnd);
        SetTimer(hwnd, 3, 16, nullptr);
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
        const float edge = width < 1000.0f ? 20.0f : 40.0f;
        const float contentWidth = (width - edge * 2.0f) < 1180.0f ? width - edge * 2.0f : 1180.0f;
        const float right = (width - contentWidth) * 0.5f + contentWidth;
        const Rect play{right - 196, height - 60, right, height - 16};
        const bool next = g_page == 0 && !g_drawerOpen && Hit(play, x, y);
        if (next != g_hoverPlay) { g_hoverPlay = next; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    }
    case WM_LBUTTONUP: {
        const float x = static_cast<float>(GET_X_LPARAM(lp));
        const float y = static_cast<float>(GET_Y_LPARAM(lp));
        RECT rc{}; GetClientRect(hwnd, &rc);
        const float width = static_cast<float>(rc.right);
        const float height = static_cast<float>(rc.bottom);
        const float edge = width < 1000.0f ? 20.0f : 40.0f;
        const float contentWidth = (width - edge * 2.0f) < 1180.0f ? width - edge * 2.0f : 1180.0f;
        const float left = (width - contentWidth) * 0.5f;
        const float right = left + contentWidth;
        const Rect menu{16, 14, 58, 54};

        if (Hit(menu, x, y)) { SetDrawer(hwnd, !g_drawerOpen); return 0; }
        if (g_drawerOpen) {
            const float drawerWidth = width * 0.78f < 330.0f ? width * 0.78f : 330.0f;
            if (x <= drawerWidth) {
                for (int i = 0; i < 4; ++i) {
                    const Rect item{16, 118.0f + i * 54.0f, drawerWidth - 16, 164.0f + i * 54.0f};
                    if (Hit(item, x, y)) {
                        g_page = i; SetDrawer(hwnd, false); InvalidateRect(hwnd, nullptr, FALSE); return 0;
                    }
                }
            }
            if (x > drawerWidth) SetDrawer(hwnd, false);
            return 0;
        }
        const Rect play{right - 196, height - 60, right, height - 16};
        if (g_page == 0 && Hit(play, x, y)) {
            g_target.Reset(); g_brush.Reset();
            Log("Play clicked: starting 3D world");
            g_inWorld = imux_world_run(hwnd) != 0;
            Log("3D world start result: %d", g_inWorld ? 1 : 0);
            if (!g_inWorld) InitializeGraphics(hwnd);
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == 2) {
            const float target = g_drawerOpen ? 1.0f : 0.0f;
            g_drawerProgress += (target - g_drawerProgress) * 0.24f;
            if (std::fabs(target - g_drawerProgress) < 0.01f) {
                g_drawerProgress = target; KillTimer(hwnd, 2);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == 3) {
            if (g_inWorld) {
                imux_world_update(0.016f);
                imux_world_render();
            } else {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; BeginPaint(hwnd, &ps); Render(hwnd); EndPaint(hwnd, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        Log("WM_DESTROY received; inWorld=%d", g_inWorld ? 1 : 0);
        if (g_inWorld) imux_world_shutdown();
        KillTimer(hwnd, 2); KillTimer(hwnd, 3);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    InitializeLogging();
    SetUnhandledExceptionFilter(ImuxUnhandledException);
    std::set_terminate(ImuxTerminate);
    Log("wWinMain entered");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.hInstance = instance; wc.lpfnWndProc = WndProc; wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) Log("RegisterClassExW failed: error=%lu", GetLastError()); else Log("RegisterClassExW succeeded");
    Log("Creating main window");
    HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 760, nullptr, nullptr, instance, nullptr);
    if (!hwnd) { Log("CreateWindowExW failed: error=%lu", GetLastError()); return 1; }
    Log("CreateWindowExW succeeded hwnd=%p", hwnd);
    // Start maximized so the launcher uses the full available work area on Windows.\n    // The UI itself remains bounded by the live client rectangle and responsive layout.\n    Log("Calling ShowWindow");
    ShowWindow(hwnd, SW_MAXIMIZE);
    Log("Calling UpdateWindow");
    UpdateWindow(hwnd);
    Log("Entering message loop");
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    Log("Message loop ended: exitCode=%lld", static_cast<long long>(msg.wParam));
    Log("========== Imux launcher end ==========");
    g_log.close();
    return static_cast<int>(msg.wParam);
}
