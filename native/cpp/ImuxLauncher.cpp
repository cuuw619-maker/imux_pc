#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdarg>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cwchar>
#include <cstdio>
#include "imux_3d_engine.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kWindowClass[] = L"ImuxLauncherWindow";
constexpr wchar_t kWindowTitle[] = L"Imux";
constexpr wchar_t kVersion[] = L"0.0.1";
constexpr float kDesignWidth = 1920.0f;
constexpr float kDesignHeight = 1080.0f;
constexpr float kDesignScale = 0.82f;

ComPtr<ID2D1Factory> g_d2dFactory;
ComPtr<IDWriteFactory> g_writeFactory;
ComPtr<ID2D1HwndRenderTarget> g_target;
ComPtr<ID2D1SolidColorBrush> g_brush;
ComPtr<IDWriteTextFormat> g_display;
ComPtr<IDWriteTextFormat> g_title;
ComPtr<IDWriteTextFormat> g_body;
ComPtr<IDWriteTextFormat> g_label;
ComPtr<IDWriteTextFormat> g_button;
ComPtr<ID3D11Device> g_d3dDevice;
ComPtr<ID3D11DeviceContext> g_d3dContext;
ComPtr<ID3DBlob> g_pixelShader;

std::ofstream g_log;
std::string g_logPath;

int g_page = 0;
bool g_inWorld = false;
bool g_hoverPlay = false;
bool g_fullscreen = false;
D2D1_MATRIX_3X2_F g_uiTransform = D2D1::Matrix3x2F::Identity();
WINDOWPLACEMENT g_windowedPlacement{sizeof(WINDOWPLACEMENT)};
LONG_PTR g_windowedStyle = 0;
LONG_PTR g_windowedExStyle = 0;
float g_motion = 0.0f;

struct Rect {
    float l, t, r, b;
    Rect(float left, float top, float right, float bottom) : l(left), t(top), r(right), b(bottom) {}
};

struct UiViewport {
    float scale;
    float offsetX;
    float offsetY;
};

UiViewport CalculateUiViewport(float width, float height) {
    const float fit = std::min(width / kDesignWidth, height / kDesignHeight);
    const float scale = fit * kDesignScale;
    return {
        scale,
        (width - kDesignWidth * scale) * 0.5f,
        (height - kDesignHeight * scale) * 0.5f
    };
}

D2D1_POINT_2F ToDesignPoint(const UiViewport& viewport, float x, float y) {
    return D2D1::Point2F(
        (x - viewport.offsetX) / viewport.scale,
        (y - viewport.offsetY) / viewport.scale
    );
}

bool ClientToUiPoint(float x, float y, D2D1_POINT_2F& result) {
    if (!g_target) return false;

    D2D1_MATRIX_3X2_F transform{};
    g_target->GetTransform(&transform);

    if (!D2D1InvertMatrix(&transform)) {
        Log("Hit-test transform inversion failed");
        return false;
    }

    result.x = x * transform._11 + y * transform._21 + transform._31;
    result.y = x * transform._12 + y * transform._22 + transform._32;
    return true;
}

struct LauncherLayout {
    float left;
    float right;
    float top;
    float bottom;
    bool compact;
    Rect playRect;
};

LauncherLayout CalculateLauncherLayout() {
    constexpr float edge = 44.0f;
    const float contentW = std::min(1240.0f, std::max(320.0f, kDesignWidth - edge * 2.0f));
    const float left = (kDesignWidth - contentW) * 0.5f;
    const float right = left + contentW;
    const float top = 110.0f;
    const float bottom = kDesignHeight - 24.0f;
    const bool compact = contentW < 920.0f;
    const float heroBottom = std::min(bottom - 170.0f, top + 290.0f);
    const float cardBottom = compact ? heroBottom : bottom;
    const float buttonW = std::min(270.0f, contentW - 56.0f);
    const Rect playRect{
        left + 28.0f,
        cardBottom - 82.0f,
        left + 28.0f + buttonW,
        cardBottom - 24.0f
    };
    return {left, right, top, bottom, compact, playRect};
}

bool Hit(const Rect& r, float x, float y) {
    return x >= r.l && x <= r.r && y >= r.t && y <= r.b;
}

float Clamp(float value, float lo, float hi) {
    return std::max(lo, std::min(value, hi));
}

void Log(const char* format, ...) {
    if (!g_log.is_open()) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char message[2048]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);
    g_log << '[' << st.wHour << ':' << st.wMinute << ':' << st.wSecond << '.' << st.wMilliseconds << "] " << message << '\n';
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
    const DWORD length = GetCurrentDirectoryA(MAX_PATH, cwd);
    g_logPath = length > 0 && length < MAX_PATH ? std::string(cwd) + "\\imux.log" : "imux.log";
    g_log.open(g_logPath, std::ios::out | std::ios::app);
    if (g_log.is_open()) {
        Log("========== Imux launcher start ==========");
        Log("Working directory: %s", length > 0 ? cwd : ".");
        Log("Log file: %s", g_logPath.c_str());
    }
}

void Color(float r, float g, float b, float a = 1.0f) {
    g_brush->SetColor(D2D1::ColorF(r, g, b, a));
}

void Fill(const Rect& r, float radius = 0.0f) {
    if (radius <= 0.0f) {
        g_target->FillRectangle(D2D1::RectF(r.l, r.t, r.r, r.b), g_brush.Get());
    } else {
        g_target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.l, r.t, r.r, r.b), radius, radius),
            g_brush.Get()
        );
    }
}

void Stroke(const Rect& r, float radius = 0.0f, float width = 1.0f) {
    if (radius <= 0.0f) {
        g_target->DrawRectangle(D2D1::RectF(r.l, r.t, r.r, r.b), g_brush.Get(), width);
    } else {
        g_target->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.l, r.t, r.r, r.b), radius, radius),
            g_brush.Get(),
            width
        );
    }
}

void Text(const wchar_t* value, const Rect& r, const ComPtr<IDWriteTextFormat>& format) {
    g_target->DrawTextW(
        value,
        static_cast<UINT32>(wcslen(value)),
        format.Get(),
        D2D1::RectF(r.l, r.t, r.r, r.b),
        g_brush.Get()
    );
}

void Circle(float x, float y, float radius) {
    g_target->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius),
        g_brush.Get()
    );
}

void Line(float x1, float y1, float x2, float y2, float width = 1.0f) {
    g_target->DrawLine(
        D2D1::Point2F(x1, y1),
        D2D1::Point2F(x2, y2),
        g_brush.Get(),
        width
    );
}

void PlayGlyph(float x, float y, float size) {
    Color(0.04f, 0.07f, 0.06f);
    Line(x - size * 0.22f, y - size * 0.33f, x + size * 0.30f, y, 3.0f);
    Line(x + size * 0.30f, y, x - size * 0.22f, y + size * 0.33f, 3.0f);
    Line(x - size * 0.22f, y + size * 0.33f, x - size * 0.22f, y - size * 0.33f, 3.0f);
}

std::filesystem::path AppDataRoot() {
    wchar_t buffer[32768]{};
    const DWORD capacity = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, capacity);
    if (length == 0 || length >= capacity) return {};
    return std::filesystem::path(buffer) / L"Imux";
}

std::filesystem::path FindGameExecutable() {
    const auto appData = AppDataRoot();
    const std::filesystem::path current = std::filesystem::current_path();

    const std::vector<std::filesystem::path> candidates = {
        current / L"ImuxGame.exe",
        appData / L"game" / L"ImuxGame.exe",
        appData / L"instances" / L"default" / L"ImuxGame.exe",
        appData / L"instances" / L"default" / L"game" / L"ImuxGame.exe"
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
    }
    return {};
}

int TryLaunchInstalledGame() {
    const auto executable = FindGameExecutable();
    if (executable.empty()) return 0;

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION processInfo{};
    std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    const std::wstring workingDirectory = executable.parent_path().wstring();
    const BOOL started = CreateProcessW(
        executable.c_str(),
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        nullptr,
        workingDirectory.c_str(),
        &startup,
        &processInfo
    );

    if (!started) {
        Log("External game launch failed: error=%lu path=%ls", GetLastError(), executable.c_str());
        return -1;
    }

    Log("External game started: pid=%lu path=%ls",
        static_cast<unsigned long>(processInfo.dwProcessId),
        executable.c_str());

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 1;
}

void RenderHeader(float width) {
    Color(0.035f, 0.043f, 0.055f);
    Fill({0.0f, 0.0f, width, 84.0f});

    Color(0.60f, 0.98f, 0.79f);
    Fill({24.0f, 23.0f, 60.0f, 59.0f}, 12.0f);
    Color(0.035f, 0.055f, 0.045f);
    Circle(42.0f, 34.0f, 7.0f);
    Line(31.0f, 45.0f, 42.0f, 53.0f, 3.0f);
    Line(53.0f, 45.0f, 42.0f, 53.0f, 3.0f);

    Color(0.90f, 0.94f, 0.96f);
    Text(L"IMUX", {74.0f, 23.0f, 155.0f, 55.0f}, g_title);
    Color(0.47f, 0.52f, 0.59f);
    Text(kVersion, {75.0f, 51.0f, 155.0f, 68.0f}, g_label);

    const float navStart = 176.0f;
    const float navW = 106.0f;
    const wchar_t* labels[] = {L"Start", L"Library", L"Changelog", L"Settings"};
    for (int i = 0; i < 4; ++i) {
        const float x = navStart + i * navW;
        const bool selected = g_page == i;
        if (selected) {
            Color(0.08f, 0.17f, 0.14f);
            Fill({x, 21.0f, x + navW - 7.0f, 63.0f}, 13.0f);
        }
        Color(selected ? 0.67f : 0.49f, selected ? 0.97f : 0.55f, selected ? 0.80f : 0.63f);
        Text(labels[i], {x + 13.0f, 31.0f, x + navW - 14.0f, 55.0f}, g_body);
    }

    Color(0.10f, 0.13f, 0.17f);
    Fill({width - 144.0f, 22.0f, width - 22.0f, 62.0f}, 20.0f);
    Color(0.54f, 0.59f, 0.65f);
    Text(L"Guest", {width - 113.0f, 31.0f, width - 37.0f, 54.0f}, g_body);
}

void RenderLaunchCard(const Rect& box) {
    Color(0.066f, 0.082f, 0.102f);
    Fill(box, 28.0f);

    const float pulse = 1.0f + 0.035f * std::sin(g_motion * 1.6f);
    Color(0.25f, 0.78f, 0.57f, 0.06f);
    Circle(box.r - 34.0f, box.t + 64.0f, 82.0f * pulse);
    Color(0.25f, 0.78f, 0.57f, 0.035f);
    Circle(box.r - 92.0f, box.b - 42.0f, 116.0f * pulse);

    Color(0.43f, 0.98f, 0.75f);
    Text(L"READY TO LAUNCH", {box.l + 30.0f, box.t + 28.0f, box.r - 30.0f, box.t + 51.0f}, g_label);

    Color(0.94f, 0.97f, 0.98f);
    Text(L"Default Client", {box.l + 30.0f, box.t + 59.0f, box.r - 30.0f, box.t + 105.0f}, g_display);

    Color(0.48f, 0.54f, 0.62f);
    Text(L"Guest profile", {box.l + 31.0f, box.t + 110.0f, box.r - 30.0f, box.t + 135.0f}, g_body);

    const auto executable = FindGameExecutable();
    const bool external = !executable.empty();

    Color(0.10f, 0.13f, 0.16f);
    Fill({box.l + 28.0f, box.t + 158.0f, box.r - 28.0f, box.t + 210.0f}, 16.0f);
    Color(0.43f, 0.98f, 0.75f);
    Text(external ? L"GAME" : L"BASE WORLD",
        {box.l + 46.0f, box.t + 172.0f, box.l + 164.0f, box.t + 194.0f},
        g_label);
    Color(0.49f, 0.55f, 0.63f);
    Text(external ? L"ImuxGame.exe" : L"Built-in first-person world",
        {box.l + 174.0f, box.t + 172.0f, box.r - 42.0f, box.t + 195.0f},
        g_label);

    Color(0.37f, 0.42f, 0.49f);
    Text(L"0.0.1",
        {box.l + 31.0f, box.t + 228.0f, box.l + 100.0f, box.t + 250.0f},
        g_label);

    const float buttonW = std::min(270.0f, box.r - box.l - 56.0f);
    const float bx = box.l + 28.0f;
    const float by = box.b - 82.0f;
    Color(
        g_hoverPlay ? 0.62f : 0.52f,
        g_hoverPlay ? 1.00f : 0.96f,
        g_hoverPlay ? 0.82f : 0.74f
    );
    Fill({bx, by, bx + buttonW, by + 58.0f}, 20.0f);
    PlayGlyph(bx + 31.0f, by + 29.0f, 20.0f);
    Color(0.035f, 0.055f, 0.05f);
    Text(L"PLAY", {bx + 55.0f, by + 16.0f, bx + buttonW - 18.0f, by + 43.0f}, g_button);
}

void RenderReleaseCard(const Rect& box) {
    Color(0.082f, 0.100f, 0.125f);
    Fill(box, 28.0f);

    Color(0.43f, 0.98f, 0.75f);
    Text(L"WHAT'S NEW", {box.l + 26.0f, box.t + 28.0f, box.r - 24.0f, box.t + 51.0f}, g_label);

    Color(0.94f, 0.97f, 0.98f);
    Text(L"0.0.1", {box.l + 26.0f, box.t + 58.0f, box.r - 24.0f, box.t + 96.0f}, g_title);

    Color(0.46f, 0.52f, 0.60f);
    Text(L"2026-09-19", {box.l + 27.0f, box.t + 99.0f, box.r - 24.0f, box.t + 122.0f}, g_body);
    Color(0.16f, 0.19f, 0.23f);
    Fill({box.l + 24.0f, box.t + 136.0f, box.r - 24.0f, box.t + 137.0f});

    const wchar_t* items[] = {
        L"Launcher rebuilt from scratch.",
        L"New responsive launch workspace.",
        L"Real executable launch boundary.",
        L"Built-in base world remains available.",
        L"In-launcher changelog and version."
    };
    const int maxEntries = std::max(1, std::min(5, static_cast<int>((box.b - box.t - 145.0f) / 45.0f)));
    for (int i = 0; i < maxEntries; ++i) {
        const float y = box.t + 151.0f + i * 45.0f;
        Color(0.60f, 0.96f, 0.78f);
        Circle(box.l + 31.0f, y + 7.0f, 3.0f);
        Color(0.66f, 0.71f, 0.77f);
        Text(items[i], {box.l + 45.0f, y - 2.0f, box.r - 22.0f, y + 23.0f}, g_body);
    }
}

void RenderStart(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Start", {left, top, right, top + 45.0f}, g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"Launch the base game from one focused workspace.", {left, top + 48.0f, right, top + 74.0f}, g_body);

    const float contentTop = top + 94.0f;
    const float gap = 16.0f;
    const float available = right - left;
    const bool compact = available < 920.0f;

    if (compact) {
        const float heroBottom = std::min(bottom - 170.0f, contentTop + 290.0f);
        RenderLaunchCard({left, contentTop, right, heroBottom});
        RenderReleaseCard({left, heroBottom + gap, right, bottom});
    } else {
        const float heroRight = left + available * 0.66f;
        RenderLaunchCard({left, contentTop, heroRight - gap * 0.5f, bottom});
        RenderReleaseCard({heroRight + gap * 0.5f, contentTop, right, bottom});
    }
}

void RenderChangelog(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Changelog", {left, top, right, top + 45.0f}, g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"Release history is kept here until the next version is approved.", {left, top + 48.0f, right, top + 74.0f}, g_body);

    const float y0 = top + 96.0f;
    const float h = Clamp((bottom - y0 - 50.0f) / 5.0f, 54.0f, 74.0f);
    const wchar_t* entries[] = {
        L"Launcher workspace and navigation rebuilt from scratch.",
        L"Responsive layout for compact and wide Windows windows.",
        L"Real executable launch boundary replaces the mock process.",
        L"Native base first-person world remains the fallback target.",
        L"Version and changelog are now part of the launcher UI."
    };

    Color(0.43f, 0.98f, 0.75f);
    Text(L"0.0.1", {left, y0, left + 120.0f, y0 + 26.0f}, g_title);
    Color(0.45f, 0.50f, 0.58f);
    Text(L"2026-09-19", {right - 130.0f, y0 + 4.0f, right, y0 + 26.0f}, g_label);

    for (int i = 0; i < 5; ++i) {
        const float y = y0 + 38.0f + i * h;
        Color(0.066f, 0.082f, 0.102f);
        Fill({left, y, right, y + h - 8.0f}, 17.0f);
        Color(0.46f, 0.98f, 0.76f);
        Circle(left + 22.0f, y + 22.0f, 4.0f);
        Color(0.78f, 0.82f, 0.87f);
        Text(entries[i], {left + 39.0f, y + 10.0f, right - 20.0f, y + h - 12.0f}, g_body);
    }
}

void RenderLibrary(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Library", {left, top, right, top + 45.0f}, g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"The current installation is the only launch target implemented.", {left, top + 48.0f, right, top + 74.0f}, g_body);

    const float y0 = top + 96.0f;
    Color(0.066f, 0.082f, 0.102f);
    Fill({left, y0, right, y0 + 188.0f}, 26.0f);

    Color(0.43f, 0.98f, 0.75f);
    Text(L"ACTIVE", {left + 26.0f, y0 + 25.0f, right - 20.0f, y0 + 48.0f}, g_label);
    Color(0.94f, 0.97f, 0.98f);
    Text(L"Default Client", {left + 26.0f, y0 + 54.0f, right - 20.0f, y0 + 92.0f}, g_title);

    Color(0.50f, 0.55f, 0.63f);
    Text(L"Version", {left + 26.0f, y0 + 108.0f, left + 190.0f, y0 + 132.0f}, g_label);
    Text(L"Directory", {left + 26.0f, y0 + 141.0f, left + 190.0f, y0 + 165.0f}, g_label);

    Color(0.78f, 0.82f, 0.87f);
    Text(L"0.0.1", {left + 190.0f, y0 + 108.0f, right - 20.0f, y0 + 132.0f}, g_body);
    Text(L"%APPDATA%\\Imux\\instances\\default", {left + 190.0f, y0 + 141.0f, right - 20.0f, y0 + 165.0f}, g_body);

    Color(0.15f, 0.18f, 0.22f);
    Fill({left, y0 + 208.0f, right, y0 + 274.0f}, 19.0f);
    Color(0.43f, 0.98f, 0.75f);
    Text(L"SOON", {left + 25.0f, y0 + 230.0f, left + 90.0f, y0 + 252.0f}, g_label);
}

void RenderSettings(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Settings", {left, top, right, top + 45.0f}, g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"Concrete diagnostics are shown here. Unimplemented controls stay empty.", {left, top + 48.0f, right, top + 74.0f}, g_body);

    const float y0 = top + 96.0f;
    const float rowH = 66.0f;
    const wchar_t* labels[] = {L"Version", L"Launcher log", L"Game data"};
    const std::wstring appData = AppDataRoot().wstring();
    const std::wstring gameData = appData.empty() ? L"%APPDATA%\\Imux" : appData;
    const std::wstring logPath = g_logPath.empty() ? L"imux.log" : std::wstring(g_logPath.begin(), g_logPath.end());
    const std::wstring values[] = {kVersion, logPath, gameData};

    for (int i = 0; i < 3; ++i) {
        const float y = y0 + i * rowH;
        Color(0.066f, 0.082f, 0.102f);
        Fill({left, y, right, y + rowH - 10.0f}, 17.0f);
        Color(0.48f, 0.54f, 0.62f);
        Text(labels[i], {left + 20.0f, y + 11.0f, left + 170.0f, y + 33.0f}, g_label);
        Color(0.79f, 0.83f, 0.88f);
        Text(values[i].c_str(), {left + 170.0f, y + 10.0f, right - 18.0f, y + 38.0f}, g_body);
    }

    const float soonY = y0 + rowH * 3.0f + 8.0f;
    Color(0.15f, 0.18f, 0.22f);
    Fill({left, soonY, right, std::min(bottom, soonY + 74.0f)}, 19.0f);
    Color(0.43f, 0.98f, 0.75f);
    Text(L"SOON", {left + 25.0f, soonY + 25.0f, left + 95.0f, soonY + 49.0f}, g_label);
}

void InitializeGraphics(HWND hwnd);

void Render(HWND hwnd) {
    if (!g_target) return;

    g_target->BeginDraw();

    RECT client{};
    GetClientRect(hwnd, &client);
    const float width = static_cast<float>(client.right);
    const float height = static_cast<float>(client.bottom);
    const UiViewport viewport = CalculateUiViewport(width, height);

    Color(0.028f, 0.034f, 0.044f);
    g_target->Clear(D2D1::ColorF(0.028f, 0.034f, 0.044f));

    g_uiTransform = D2D1::Matrix3x2F(
        viewport.scale, 0.0f,
        0.0f, viewport.scale,
        viewport.offsetX, viewport.offsetY
    );
    g_target->SetTransform(g_uiTransform);

    RenderHeader(kDesignWidth);

    const LauncherLayout layout = CalculateLauncherLayout();
    const float left = layout.left;
    const float right = layout.right;
    const float top = layout.top;
    const float bottom = layout.bottom;

    switch (g_page) {
        case 0: RenderStart(left, right, top, bottom); break;
        case 1: RenderLibrary(left, right, top, bottom); break;
        case 2: RenderChangelog(left, right, top, bottom); break;
        case 3: RenderSettings(left, right, top, bottom); break;
        default: g_page = 0; RenderStart(left, right, top, bottom); break;
    }

    g_target->SetTransform(D2D1::Matrix3x2F::Identity());
    const HRESULT hr = g_target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        g_target.Reset();
        g_brush.Reset();
        Log("Direct2D requested render target recreation");
        InitializeGraphics(hwnd);
    }
}

void InitializeGraphics(HWND hwnd) {
    Log("InitializeGraphics: begin");

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        g_d2dFactory.GetAddressOf()
    );
    Log("D2D1CreateFactory: hr=0x%08lX", static_cast<unsigned long>(hr));

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_writeFactory.GetAddressOf())
    );
    Log("DWriteCreateFactory: hr=0x%08lX", static_cast<unsigned long>(hr));

    RECT rc{};
    GetClientRect(hwnd, &rc);
    hr = g_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT),
        D2D1::HwndRenderTargetProperties(
            hwnd,
            D2D1::SizeU(
                static_cast<UINT>(std::max(1L, rc.right)),
                static_cast<UINT>(std::max(1L, rc.bottom))
            )
        ),
        &g_target
    );
    Log("CreateHwndRenderTarget: hr=0x%08lX", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return;

    hr = g_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_brush);
    Log("CreateSolidColorBrush: hr=0x%08lX", static_cast<unsigned long>(hr));

    g_writeFactory->CreateTextFormat(
        L"Segoe UI Variable", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 34.0f, L"en-us", &g_display
    );
    g_writeFactory->CreateTextFormat(
        L"Segoe UI Variable", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.0f, L"en-us", &g_title
    );
    g_writeFactory->CreateTextFormat(
        L"Segoe UI Variable", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &g_body
    );
    g_writeFactory->CreateTextFormat(
        L"Segoe UI Variable", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &g_label
    );
    g_writeFactory->CreateTextFormat(
        L"Segoe UI Variable", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &g_button
    );

    D3D_FEATURE_LEVEL featureLevel{};
    const HRESULT d3dHr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &g_d3dDevice,
        &featureLevel,
        &g_d3dContext
    );
    Log(
        "D3D11CreateDevice: hr=0x%08lX featureLevel=0x%08lX",
        static_cast<unsigned long>(d3dHr),
        static_cast<unsigned long>(featureLevel)
    );

    const char* shader = R"(float4 main() : SV_TARGET { return float4(0.16,0.42,0.31,1.0); })";
    const HRESULT shaderHr = D3DCompile(
        shader,
        strlen(shader),
        "imux_ui.hlsl",
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        0,
        0,
        &g_pixelShader,
        nullptr
    );
    Log("D3DCompile: hr=0x%08lX", static_cast<unsigned long>(shaderHr));
    Log("InitializeGraphics: complete");
}

int HeaderPageAt(float x, float y) {
    if (y < 18.0f || y > 68.0f) return -1;
    const float navStart = 176.0f;
    const float navW = 106.0f;
    if (x < navStart || x > navStart + navW * 4.0f) return -1;
    const int index = static_cast<int>((x - navStart) / navW);
    return index >= 0 && index < 4 ? index : -1;
}

void ToggleFullscreen(HWND hwnd) {
    g_fullscreen = !g_fullscreen;

    if (g_fullscreen) {
        g_windowedStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
        g_windowedExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        g_windowedPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd, &g_windowedPlacement);

        MONITORINFO monitorInfo{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo);

        SetWindowLongPtrW(hwnd, GWL_STYLE, g_windowedStyle & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, g_windowedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        SetWindowPos(
            hwnd,
            HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );
        Log("F11 fullscreen: enabled");
    } else {
        SetWindowLongPtrW(hwnd, GWL_STYLE, g_windowedStyle);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, g_windowedExStyle);
        SetWindowPlacement(hwnd, &g_windowedPlacement);
        SetWindowPos(
            hwnd,
            nullptr,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );
        Log("F11 fullscreen: disabled");
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_F11) {
        ToggleFullscreen(hwnd);
        return 0;
    }

    if (g_inWorld) {
        if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
            Log("Escape: leaving base world");
            imux_world_shutdown();
            g_inWorld = false;
            InitializeGraphics(hwnd);
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
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_CREATE:
            Log("WM_CREATE received");
            g_windowedStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
            g_windowedExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            g_windowedPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(hwnd, &g_windowedPlacement);
            InitializeGraphics(hwnd);
            SetTimer(hwnd, 3, 16, nullptr);
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lp);
            info->ptMinTrackSize.x = 880;
            info->ptMinTrackSize.y = 600;
            return 0;
        }

        case WM_DPICHANGED:
            Log("WM_DPICHANGED: dpi=%u", LOWORD(wp));
            if (g_target) InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

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
            RECT rc{};
            GetClientRect(hwnd, &rc);
            D2D1_POINT_2F point{};
            if (!ClientToUiPoint(x, y, point)) return 0;
            const LauncherLayout layout = CalculateLauncherLayout();
            const bool hover = g_page == 0 && Hit(layout.playRect, point.x, point.y);
            if (hover != g_hoverPlay) {
                g_hoverPlay = hover;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            const float x = static_cast<float>(GET_X_LPARAM(lp));
            const float y = static_cast<float>(GET_Y_LPARAM(lp));
            RECT rc{};
            GetClientRect(hwnd, &rc);
            D2D1_POINT_2F point{};
            if (!ClientToUiPoint(x, y, point)) return 0;

            const int headerPage = HeaderPageAt(point.x, point.y);
            if (headerPage >= 0) {
                g_page = headerPage;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (g_page == 0) {
                const LauncherLayout layout = CalculateLauncherLayout();

                if (Hit(layout.playRect, point.x, point.y)) {
                    Log("Play clicked");
                    const int externalResult = TryLaunchInstalledGame();
                    if (externalResult == 1) {
                        Log("Play target: installed executable");
                        return 0;
                    }
                    if (externalResult < 0) {
                        MessageBoxW(hwnd, L"ImuxGame.exe was found but could not be started. See imux.log for details.", L"Imux", MB_ICONERROR | MB_OK);
                        return 0;
                    }

                    Log("Play target: built-in base world");
                    g_target.Reset();
                    g_brush.Reset();
                    g_inWorld = imux_world_run(hwnd) != 0;
                    Log("Base world start result: %d", g_inWorld ? 1 : 0);
                    if (!g_inWorld) {
                        InitializeGraphics(hwnd);
                        MessageBoxW(hwnd, L"The base world could not be started. See imux.log for details.", L"Imux", MB_ICONERROR | MB_OK);
                    }
                    return 0;
                }
            }
            return 0;
        }

        case WM_TIMER:
            if (wp == 3) {
                g_motion += 0.016f;
                if (g_inWorld) {
                    imux_world_update(0.016f);
                    imux_world_render();
                } else {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
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
            Log("WM_DESTROY received; inWorld=%d", g_inWorld ? 1 : 0);
            if (g_inWorld) imux_world_shutdown();
            KillTimer(hwnd, 3);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

} 

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    InitializeLogging();
    SetUnhandledExceptionFilter(ImuxUnhandledException);
    std::set_terminate(ImuxTerminate);

    Log("wWinMain entered");
    const BOOL dpiResult = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Log("SetProcessDpiAwarenessContext: result=0x%08lX", static_cast<unsigned long>(dpiResult));

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = kWindowClass;
    wc.lpszMenuName = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(101));

    Log("Registering window class");
    if (!RegisterClassExW(&wc)) {
        Log("RegisterClassExW failed: error=%lu", GetLastError());
        return 1;
    }
    Log("RegisterClassExW succeeded");

    Log("Creating main window");
    HWND hwnd = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1600,
        900,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        Log("CreateWindowExW failed: error=%lu", GetLastError());
        return 1;
    }

    Log("CreateWindowExW succeeded hwnd=%p", hwnd);
    ShowWindow(hwnd, SW_MAXIMIZE);
    Log("ShowWindow complete");
    UpdateWindow(hwnd);
    Log("UpdateWindow complete; entering message loop");

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Log("Message loop ended: exitCode=%lld", static_cast<long long>(msg.wParam));
    Log("========== Imux launcher end ==========");
    g_log.close();
    return static_cast<int>(msg.wParam);
}
