#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>

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
ComPtr<IDWriteTextFormat> g_title;
ComPtr<IDWriteTextFormat> g_body;
ComPtr<IDWriteTextFormat> g_small;
ComPtr<IDWriteTextFormat> g_button;
ComPtr<ID3D11Device> g_d3dDevice;
ComPtr<ID3D11DeviceContext> g_d3dContext;
ComPtr<ID3DBlob> g_pixelShader;

bool g_hoverPlay = false;
bool g_launching = false;

void Color(float r, float g, float b, float a = 1.0f) {
    g_brush->SetColor(D2D1::ColorF(r, g, b, a));
}

void Fill(float l, float t, float r, float b, float radius = 0.0f) {
    g_target->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(l, t, r, b), radius, radius),
        g_brush.Get());
}

void Text(const wchar_t* value, float x, float y, float w, float h, IDWriteTextFormat* format) {
    g_target->DrawTextW(value, static_cast<UINT32>(wcslen(value)),
        format, D2D1::RectF(x, y, x + w, y + h), g_brush.Get());
}

void Render(HWND hwnd) {
    if (!g_target) return;
    g_target->BeginDraw();
    g_target->Clear(D2D1::ColorF(0.035f, 0.043f, 0.055f));

    RECT client{};
    GetClientRect(hwnd, &client);
    const float width = static_cast<float>(client.right);
    const float height = static_cast<float>(client.bottom);
    const float sidebar = std::clamp(width * 0.20f, 210.0f, 280.0f);

    Color(0.045f, 0.055f, 0.070f);
    g_target->FillRectangle(D2D1::RectF(0, 0, sidebar, height), g_brush.Get());

    Color(0.30f, 0.82f, 0.62f);
    Text(L"IMUX", 34, 32, sidebar - 60, 48, g_title);
    Color(0.46f, 0.50f, 0.57f);
    Text(L"CLIENT PLATFORM", 36, 80, sidebar - 60, 24, g_small);

    Color(0.16f, 0.19f, 0.24f);
    Fill(22, 132, sidebar - 22, 180, 12);
    Color(0.88f, 0.91f, 0.95f);
    Text(L"Home", 48, 145, sidebar - 70, 28, g_body);
    Color(0.52f, 0.56f, 0.63f);
    Text(L"Instances", 48, 196, sidebar - 70, 28, g_body);
    Text(L"Engine", 48, 247, sidebar - 70, 28, g_body);
    Text(L"Settings", 48, 298, sidebar - 70, 28, g_body);

    Color(0.22f, 0.25f, 0.31f);
    g_target->FillRectangle(D2D1::RectF(sidebar + 1, 0, sidebar + 2, height), g_brush.Get());

    Color(0.88f, 0.91f, 0.95f);
    Text(L"Your game environment", sidebar + 54, 42, width - sidebar - 100, 42, g_title);
    Color(0.48f, 0.52f, 0.60f);
    Text(L"One native launcher. One controlled runtime. Your client.",
        sidebar + 56, 88, width - sidebar - 110, 28, g_body);

    const float cardLeft = sidebar + 54;
    const float cardRight = width - 54;
    Color(0.075f, 0.090f, 0.115f);
    Fill(cardLeft, 142, cardRight, 350, 18);

    Color(0.30f, 0.82f, 0.62f);
    Text(L"IMUX DEVELOPMENT", cardLeft + 28, 170, 360, 28, g_small);
    Color(0.93f, 0.95f, 0.98f);
    Text(L"Default Client", cardLeft + 28, 205, 420, 46, g_title);
    Color(0.48f, 0.52f, 0.60f);
    Text(L"Development profile  |  Windows x64",
        cardLeft + 30, 258, 480, 28, g_body);

    const float buttonLeft = cardRight - 190;
    const float buttonTop = 218;
    Color(g_hoverPlay ? 0.36f : 0.30f,
          g_hoverPlay ? 0.92f : 0.82f,
          g_hoverPlay ? 0.70f : 0.62f);
    Fill(buttonLeft, buttonTop, cardRight - 28, buttonTop + 62, 14);
    Color(0.025f, 0.035f, 0.043f);
    Text(g_launching ? L"STARTING..." : L"PLAY",
        buttonLeft + 45, buttonTop + 16, 120, 32, g_button);

    Color(0.060f, 0.075f, 0.095f);
    Fill(cardLeft, 378, cardRight, 474, 16);
    Color(0.88f, 0.91f, 0.95f);
    Text(L"Runtime status", cardLeft + 24, 400, 240, 28, g_body);
    Color(0.30f, 0.82f, 0.62f);
    Text(g_launching ? L"Launching development runtime" : L"Ready",
        cardLeft + 24, 432, 360, 24, g_small);

    Color(0.48f, 0.52f, 0.60f);
    Text(L"Native stack", cardLeft, 510, 240, 30, g_body);

    const wchar_t* chips[] = {L"C++", L"C", L"Rust", L"CMake", L"HLSL", L"C#", L"XAML"};
    float x = cardLeft;
    for (const auto* chip : chips) {
        const float chipWidth = 92.0f;
        Color(0.085f, 0.105f, 0.135f);
        Fill(x, 548, x + chipWidth, 590, 10);
        Color(0.72f, 0.76f, 0.83f);
        Text(chip, x + 14, 559, chipWidth - 20, 22, g_small);
        x += chipWidth + 10;
        if (x + chipWidth > cardRight) break;
    }

    Color(0.38f, 0.42f, 0.50f);
    Text(L"Imux UI Engine 0.1  |  Direct2D / DirectWrite  |  D3D11 + HLSL pipeline",
        cardLeft, std::max(620.0f, height - 42.0f), cardRight - cardLeft, 24, g_small);

    g_target->EndDraw();
}

void InitializeGraphics(HWND hwnd) {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_writeFactory.GetAddressOf()));

    RECT rc{};
    GetClientRect(hwnd, &rc);
    const auto size = D2D1::SizeU(
        std::max<LONG>(1, rc.right), std::max<LONG>(1, rc.bottom));

    g_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &g_target);
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
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &g_d3dDevice, &featureLevel, &g_d3dContext);

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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        InitializeGraphics(hwnd);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMinTrackSize.x = 960;
        info->ptMinTrackSize.y = 640;
        return 0;
    }
    case WM_SIZE:
        if (g_target) {
            g_target->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE: {
        const float x = static_cast<float>(GET_X_LPARAM(lp));
        const float y = static_cast<float>(GET_Y_LPARAM(lp));
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const bool next = x >= rc.right - 244 && x <= rc.right - 82 &&
            y >= 218 && y <= 280;
        if (next != g_hoverPlay) {
            g_hoverPlay = next;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const float x = static_cast<float>(GET_X_LPARAM(lp));
        const float y = static_cast<float>(GET_Y_LPARAM(lp));
        if (x >= rc.right - 244 && x <= rc.right - 82 &&
            y >= 218 && y <= 280) {
            g_launching = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            SetTimer(hwnd, 1, 900, nullptr);
        }
        return 0;
    }
    case WM_TIMER:
        KillTimer(hwnd, 1);
        g_launching = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        Render(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 760,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
