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
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cwchar>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <utility>
#include "imux_3d_engine.h"
#include "imux_launcher_functions.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kWindowClass[] = L"ImuxLauncherWindow";
constexpr wchar_t kWindowTitle[] = L"Imux";
constexpr wchar_t kVersion[] = L"0.0.1";
constexpr float kDesignWidth = 1920.0f;
constexpr float kDesignHeight = 1080.0f;
constexpr float kDesignScale = 0.92f;

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
HWND g_mainWindow = nullptr;
D2D1_MATRIX_3X2_F g_uiTransform = D2D1::Matrix3x2F::Identity();
WINDOWPLACEMENT g_windowedPlacement{sizeof(WINDOWPLACEMENT)};
LONG_PTR g_windowedStyle = 0;
LONG_PTR g_windowedExStyle = 0;
float g_motion = 0.0f;
float g_pageAnim = 1.0f;

struct Rect {
    float l, t, r, b;
    Rect() : l(0.0f), t(0.0f), r(0.0f), b(0.0f) {}
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

void Log(const char* format, ...);

enum class EditorTarget {
    Visual,
    Hitbox
};

enum class EditorHandle {
    None,
    N,
    NE,
    E,
    SE,
    S,
    SW,
    W,
    NW
};

struct EditorItem;
struct LauncherLayout;
float Clamp(float value, float lo, float hi);
void InitializeEditorItems();
void SaveEditorJson();
void RenderEditorOverlay();
void EditorResetSelected(HWND hwnd);
EditorItem* FindEditorItem(const std::string& id);
bool Hit(const Rect& r, float x, float y);
void Color(float r, float g, float b, float a = 1.0f);
void Text(const wchar_t* value, const Rect& r, const ComPtr<IDWriteTextFormat>& format);
void Fill(const Rect& r, float radius = 0.0f);
void Stroke(const Rect& r, float radius = 0.0f, float width = 1.0f);
Rect EditorVisual(const std::string& id, const Rect& fallback);
Rect EditorHitbox(const std::string& id, const Rect& fallback);

D2D1_POINT_2F ClientToUiPoint(float x, float y) {
    RECT client{};
    if (!g_mainWindow || !GetClientRect(g_mainWindow, &client)) {
        return D2D1::Point2F(-1.0f, -1.0f);
    }

    const UiViewport viewport = CalculateUiViewport(
        static_cast<float>(std::max(1L, client.right)),
        static_cast<float>(std::max(1L, client.bottom))
    );
    if (viewport.scale <= 0.0001f) {
        return D2D1::Point2F(-1.0f, -1.0f);
    }

    return D2D1::Point2F(
        (x - viewport.offsetX) / viewport.scale,
        (y - viewport.offsetY) / viewport.scale
    );
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
    constexpr float edge = 32.0f;
    const float contentW = std::min(1540.0f, std::max(520.0f, kDesignWidth - edge * 2.0f));
    const float left = (kDesignWidth - contentW) * 0.5f;
    const float right = left + contentW;
    const float top = 102.0f;
    const float bottom = kDesignHeight - 20.0f;
    const bool compact = contentW < 980.0f;
    const float heroBottom = std::min(bottom - 170.0f, top + 304.0f);
    const float cardBottom = compact ? heroBottom : bottom;
    const float buttonW = std::min(300.0f, contentW - 56.0f);
    const Rect playRect{
        left + 28.0f,
        cardBottom - 82.0f,
        left + 28.0f + buttonW,
        cardBottom - 24.0f
    };
    return {left, right, top, bottom, compact, playRect};
}

struct EditorItem {
    std::string id;
    int page;
    Rect visual;
    Rect hitbox;
    Rect defaultVisual;
    Rect defaultHitbox;
    bool hitboxLinked;

    EditorItem(std::string itemId, int itemPage, Rect visualRect, Rect hitboxRect)
        : id(std::move(itemId)),
          page(itemPage),
          visual(visualRect),
          hitbox(hitboxRect),
          defaultVisual(visualRect),
          defaultHitbox(hitboxRect),
          hitboxLinked(true) {}
};

std::vector<EditorItem> g_editorItems;
bool g_editorMode = false;
EditorTarget g_editorTarget = EditorTarget::Visual;
std::string g_editorSelected;
EditorHandle g_editorHandle = EditorHandle::None;
bool g_editorDragging = false;
D2D1_POINT_2F g_editorDragStart{};
Rect g_editorDragOrigin{};
const char* g_editorJsonFile = "imux-ui-layout.json";

Rect MakeRect(float x, float y, float width, float height) {
    return {x, y, x + width, y + height};
}

float RectWidth(const Rect& r) {
    return r.r - r.l;
}

float RectHeight(const Rect& r) {
    return r.b - r.t;
}

void ClampEditorRect(Rect& r) {
    constexpr float minSize = 6.0f;
    const float width = std::max(minSize, RectWidth(r));
    const float height = std::max(minSize, RectHeight(r));

    r.l = Clamp(r.l, 0.0f, kDesignWidth - width);
    r.t = Clamp(r.t, 0.0f, kDesignHeight - height);
    r.r = r.l + width;
    r.b = r.t + height;
}

EditorItem* FindEditorItem(const std::string& id) {
    for (auto& item : g_editorItems) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

Rect EditorVisual(const std::string& id, const Rect& fallback) {
    if (const auto* item = FindEditorItem(id)) return item->visual;
    return fallback;
}

Rect EditorHitbox(const std::string& id, const Rect& fallback) {
    if (const auto* item = FindEditorItem(id)) return item->hitbox;
    return fallback;
}

bool EditorItemVisibleOnPage(const EditorItem& item) {
    return item.page == -1 || item.page == g_page;
}

void AddEditorItem(const char* id, int page, const Rect& rect) {
    g_editorItems.emplace_back(id, page, rect, rect);
}

void InitializeEditorItems() {
    if (!g_editorItems.empty()) return;

    const LauncherLayout layout = CalculateLauncherLayout();
    const float contentTop = layout.top + 94.0f;
    const float available = layout.right - layout.left;

    AddEditorItem("header.logo.mark", -1, {24.0f, 23.0f, 60.0f, 59.0f});
    AddEditorItem("header.logo.text", -1, {74.0f, 23.0f, 155.0f, 55.0f});
    AddEditorItem("header.logo.version", -1, {75.0f, 51.0f, 155.0f, 68.0f});
    AddEditorItem("header.nav.start", -1, {176.0f, 21.0f, 275.0f, 63.0f});
    AddEditorItem("header.nav.library", -1, {282.0f, 21.0f, 381.0f, 63.0f});
    AddEditorItem("header.nav.changelog", -1, {388.0f, 21.0f, 487.0f, 63.0f});
    AddEditorItem("header.nav.settings", -1, {494.0f, 21.0f, 593.0f, 63.0f});
    AddEditorItem("header.guest", -1, {1776.0f, 22.0f, 1898.0f, 62.0f});
    AddEditorItem("header.guest.text", -1, {1807.0f, 31.0f, 1883.0f, 53.0f});

    AddEditorItem("start.title", 0, {340.0f, layout.top, 1580.0f, layout.top + 45.0f});
    AddEditorItem("start.subtitle", 0, {340.0f, layout.top + 48.0f, 1580.0f, layout.top + 74.0f});

    const bool compact = available < 980.0f;
    if (compact) {
        const float heroBottom = std::min(layout.bottom - 170.0f, contentTop + 290.0f);
        AddEditorItem("start.launch.card", 0, {layout.left, contentTop, layout.right, heroBottom});
        AddEditorItem("start.release.card", 0, {layout.left, heroBottom + 16.0f, layout.right, layout.bottom});
    } else {
        const float heroRight = layout.left + available * 0.635f;
        AddEditorItem("start.launch.card", 0, {layout.left, contentTop, heroRight - 8.0f, layout.bottom});
        AddEditorItem("start.release.card", 0, {heroRight + 8.0f, contentTop, layout.right, layout.bottom});
    }
    AddEditorItem("start.launch.glow.top", 0, {0,0,1,1});
    AddEditorItem("start.launch.glow.bottom", 0, {0,0,1,1});
    AddEditorItem("start.launch.status", 0, {0,0,1,1});
    AddEditorItem("start.launch.name", 0, {0,0,1,1});
    AddEditorItem("start.launch.profile", 0, {0,0,1,1});
    AddEditorItem("start.launch.info.box", 0, {0,0,1,1});
    AddEditorItem("start.launch.info.label", 0, {0,0,1,1});
    AddEditorItem("start.launch.info.value", 0, {0,0,1,1});
    AddEditorItem("start.launch.version", 0, {0,0,1,1});
    AddEditorItem("start.play", 0, {0,0,1,1});
    AddEditorItem("start.play.icon", 0, {0,0,1,1});
    AddEditorItem("start.play.text", 0, {0,0,1,1});
    AddEditorItem("start.release.heading", 0, {0,0,1,1});
    AddEditorItem("start.release.version", 0, {0,0,1,1});
    AddEditorItem("start.release.date", 0, {0,0,1,1});
    AddEditorItem("start.release.divider", 0, {0,0,1,1});
    for (int i = 0; i < 5; ++i) {
        char id[64]{};
        sprintf_s(id, "start.release.item.%d", i + 1);
        AddEditorItem(id, 0, {0,0,1,1});
    }

    AddEditorItem("changelog.title", 2, {layout.left, layout.top, layout.right, layout.top + 45.0f});
    AddEditorItem("changelog.subtitle", 2, {layout.left, layout.top + 48.0f, layout.right, layout.top + 74.0f});
    AddEditorItem("changelog.version", 2, {layout.left, layout.top + 96.0f, layout.left + 120.0f, layout.top + 122.0f});
    AddEditorItem("changelog.date", 2, {layout.right - 130.0f, layout.top + 100.0f, layout.right, layout.top + 122.0f});
    for (int i = 0; i < 5; ++i) {
        char id[64]{};
        sprintf_s(id, "changelog.row.%d", i + 1);
        AddEditorItem(id, 2, {layout.left, 0, layout.right, 1});
        sprintf_s(id, "changelog.row.%d.dot", i + 1);
        AddEditorItem(id, 2, {0,0,1,1});
        sprintf_s(id, "changelog.row.%d.text", i + 1);
        AddEditorItem(id, 2, {0,0,1,1});
    }

    AddEditorItem("library.title", 1, {layout.left, layout.top, layout.right, layout.top + 45.0f});
    AddEditorItem("library.subtitle", 1, {layout.left, layout.top + 48.0f, layout.right, layout.top + 74.0f});
    AddEditorItem("library.active.card", 1, {layout.left, layout.top + 96.0f, layout.right, layout.top + 284.0f});
    AddEditorItem("library.active.badge", 1, {0,0,1,1});
    AddEditorItem("library.active.name", 1, {0,0,1,1});
    AddEditorItem("library.active.version.label", 1, {0,0,1,1});
    AddEditorItem("library.active.version.value", 1, {0,0,1,1});
    AddEditorItem("library.active.directory.label", 1, {0,0,1,1});
    AddEditorItem("library.active.directory.value", 1, {0,0,1,1});
    AddEditorItem("library.soon", 1, {layout.left, layout.top + 304.0f, layout.right, layout.top + 370.0f});
    AddEditorItem("library.soon.text", 1, {0,0,1,1});

    AddEditorItem("settings.title", 3, {layout.left, layout.top, layout.right, layout.top + 45.0f});
    AddEditorItem("settings.subtitle", 3, {layout.left, layout.top + 48.0f, layout.right, layout.top + 74.0f});
    for (int i = 0; i < 3; ++i) {
        char id[64]{};
        sprintf_s(id, "settings.row.%d", i + 1);
        AddEditorItem(id, 3, {layout.left, layout.top + 96.0f + i * 66.0f, layout.right, layout.top + 152.0f + i * 66.0f});
        sprintf_s(id, "settings.row.%d.label", i + 1);
        AddEditorItem(id, 3, {0,0,1,1});
        sprintf_s(id, "settings.row.%d.value", i + 1);
        AddEditorItem(id, 3, {0,0,1,1});
    }
    AddEditorItem("settings.soon", 3, {layout.left, layout.top + 302.0f, layout.right, layout.top + 376.0f});
    AddEditorItem("settings.soon.text", 3, {0,0,1,1});

    Log("UI editor initialized: %zu atomic elements", g_editorItems.size());
}

Rect& EditorActiveRect(EditorItem& item) {
    return g_editorTarget == EditorTarget::Visual ? item.visual : item.hitbox;
}

const Rect& EditorActiveRect(const EditorItem& item) {
    return g_editorTarget == EditorTarget::Visual ? item.visual : item.hitbox;
}

std::wstring EditorTargetText() {
    return g_editorTarget == EditorTarget::Visual ? L"VISUAL" : L"HITBOX";
}
void SyncLinkedHitbox(EditorItem& item, const Rect& beforeVisual, const Rect& afterVisual) {
    if (!item.hitboxLinked) return;
    const float dx = afterVisual.l - beforeVisual.l;
    const float dy = afterVisual.t - beforeVisual.t;
    const float dw = RectWidth(afterVisual) - RectWidth(beforeVisual);
    const float dh = RectHeight(afterVisual) - RectHeight(beforeVisual);
    if (std::abs(dw) > 0.001f || std::abs(dh) > 0.001f) item.hitbox = afterVisual;
    else {
        item.hitbox.l += dx; item.hitbox.r += dx;
        item.hitbox.t += dy; item.hitbox.b += dy;
    }
    ClampEditorRect(item.hitbox);
}

void RelinkSelectedHitbox() {
    if (auto* item = FindEditorItem(g_editorSelected)) {
        item->hitboxLinked = true;
        item->hitbox = item->visual;
        ClampEditorRect(item->hitbox);
    }
}


void SetEditorItemRect(const char* id, const Rect& rect) {
    if (auto* item = FindEditorItem(id)) {
        item->visual = rect;
        item->hitbox = rect;
        item->defaultVisual = rect;
        item->defaultHitbox = rect;
    }
}

Rect EditorOrFallback(const char* id, const Rect& fallback) {
    auto* item = FindEditorItem(id);
    if (!item || (RectWidth(item->visual) <= 1.0f && RectHeight(item->visual) <= 1.0f)) {
        SetEditorItemRect(id, fallback);
        return fallback;
    }
    return item->visual;
}

void SaveEditorJson() {
    std::ofstream out(g_editorJsonFile, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        Log("UI editor: could not open %s for writing", g_editorJsonFile);
        return;
    }

    out << "{\n";
    out << "  \"version\": 1,\n";
    out << "  \"design\": {\"width\": 1920, \"height\": 1080, \"scale\": 0.92},\n";
    out << "  \"lastPage\": " << g_page << ",\n";
    out << "  \"elements\": [\n";

    out << std::fixed << std::setprecision(2);
    for (size_t i = 0; i < g_editorItems.size(); ++i) {
        const auto& item = g_editorItems[i];
        out << "    {\n";
        out << "      \"id\": \"" << item.id << "\",\n";
        out << "      \"page\": " << item.page << ",\n";
        out << "      \"visual\": {\"x\": " << item.visual.l
            << ", \"y\": " << item.visual.t
            << ", \"width\": " << RectWidth(item.visual)
            << ", \"height\": " << RectHeight(item.visual) << "},\n";
        out << "      \"hitbox\": {\"x\": " << item.hitbox.l
            << ", \"y\": " << item.hitbox.t
            << ", \"width\": " << RectWidth(item.hitbox)
            << ", \"height\": " << RectHeight(item.hitbox) << "}\n";
        out << "    }" << (i + 1 == g_editorItems.size() ? "\n" : ",\n");
    }

    out << "  ]\n";
    out << "}\n";
    out.close();
    Log("UI editor saved: %s", g_editorJsonFile);
}

EditorHandle HitResizeHandle(const Rect& r, float x, float y) {
    constexpr float handle = 11.0f;
    const float midX = (r.l + r.r) * 0.5f;
    const float midY = (r.t + r.b) * 0.5f;

    auto isNear = [handle](float a, float b) { return std::abs(a - b) <= handle; };

    if (isNear(x, r.l) && isNear(y, r.t)) return EditorHandle::NW;
    if (isNear(x, midX) && isNear(y, r.t)) return EditorHandle::N;
    if (isNear(x, r.r) && isNear(y, r.t)) return EditorHandle::NE;
    if (isNear(x, r.r) && isNear(y, midY)) return EditorHandle::E;
    if (isNear(x, r.r) && isNear(y, r.b)) return EditorHandle::SE;
    if (isNear(x, midX) && isNear(y, r.b)) return EditorHandle::S;
    if (isNear(x, r.l) && isNear(y, r.b)) return EditorHandle::SW;
    if (isNear(x, r.l) && isNear(y, midY)) return EditorHandle::W;
    return EditorHandle::None;
}

bool PointInEditorItem(const EditorItem& item, float x, float y) {
    const Rect& rect = g_editorTarget == EditorTarget::Visual ? item.visual : item.hitbox;
    return Hit(rect, x, y);
}

EditorItem* FindEditorItemAt(float x, float y) {
    if (!g_editorSelected.empty()) {
        if (auto* selected = FindEditorItem(g_editorSelected);
            selected && EditorItemVisibleOnPage(*selected)) {
            if (HitResizeHandle(EditorActiveRect(*selected), x, y) != EditorHandle::None) {
                return selected;
            }
        }
    }

    for (auto it = g_editorItems.rbegin(); it != g_editorItems.rend(); ++it) {
        if (!EditorItemVisibleOnPage(*it)) continue;
        if (PointInEditorItem(*it, x, y)) return &(*it);
    }
    return nullptr;
}

void RenderEditorOverlay() {
    if (!g_editorMode) return;

    for (const auto& item : g_editorItems) {
        if (!EditorItemVisibleOnPage(item)) continue;
        Color(0.20f, 0.85f, 0.95f, 0.16f);
        Stroke(item.visual, 1.0f, item.id == g_editorSelected && g_editorTarget == EditorTarget::Visual ? 3.0f : 1.0f);
        if (item.hitbox.l != item.visual.l || item.hitbox.t != item.visual.t ||
            item.hitbox.r != item.visual.r || item.hitbox.b != item.visual.b) {
            Color(0.95f, 0.35f, 0.35f, 0.18f);
            Stroke(item.hitbox, 1.0f, item.id == g_editorSelected && g_editorTarget == EditorTarget::Hitbox ? 3.0f : 1.0f);
        }
    }

    const EditorItem* selected = FindEditorItem(g_editorSelected);
    if (selected && EditorItemVisibleOnPage(*selected)) {
        const Rect& active = EditorActiveRect(*selected);
        Color(0.95f, 0.88f, 0.35f, 0.98f);
        Stroke(active, 2.0f, 2.0f);

        const float xs[] = {active.l, (active.l + active.r) * 0.5f, active.r};
        const float ys[] = {active.t, (active.t + active.b) * 0.5f, active.b};
        for (int yi = 0; yi < 3; ++yi) for (int xi = 0; xi < 3; ++xi) {
            if (xi == 1 && yi == 1) continue;
            Color(0.96f, 0.95f, 0.70f, 1.0f);
            const float size = 8.0f;
            Fill({xs[xi] - size, ys[yi] - size, xs[xi] + size, ys[yi] + size}, 2.0f);
        }

        std::ostringstream geometry;
        geometry << std::fixed << std::setprecision(1)
                 << "x " << active.l << "  y " << active.t
                 << "  w " << RectWidth(active) << "  h " << RectHeight(active);
        const std::string gs = geometry.str();
        const std::wstring gws(gs.begin(), gs.end());
        Color(0.70f, 0.75f, 0.82f);
        Text(gws.c_str(), {62.0f, 184.0f, 620.0f, 205.0f}, g_label);
    }

    Color(0.04f, 0.05f, 0.065f, 0.95f);
    Fill({44.0f, 92.0f, 700.0f, 218.0f}, 18.0f);
    Color(0.55f, 1.0f, 0.78f);
    Text(L"UI EDITOR  •  R toggles", {62.0f, 107.0f, 680.0f, 131.0f}, g_label);
    Color(0.88f, 0.91f, 0.94f);
    Text(EditorTargetText().c_str(), {62.0f, 137.0f, 180.0f, 160.0f}, g_body);

    const std::wstring selectedText = g_editorSelected.empty()
        ? L"Click an element to select"
        : std::wstring(g_editorSelected.begin(), g_editorSelected.end());
    Color(0.65f, 0.70f, 0.77f);
    Text(selectedText.c_str(), {184.0f, 137.0f, 680.0f, 160.0f}, g_body);
    Color(0.44f, 0.49f, 0.56f);
    Text(L"TAB visual/hitbox • arrows 1px / Shift 10px • drag resize • [ ] select • L relink • 1-4 page",
         {62.0f, 162.0f, 680.0f, 180.0f}, g_label);
}

void EditorResetSelected(HWND hwnd) {
    auto* item = FindEditorItem(g_editorSelected);
    if (!item) return;

    if (g_editorTarget == EditorTarget::Visual) {
        const Rect before = item->visual;
        item->visual = item->defaultVisual;
        ClampEditorRect(item->visual);
        SyncLinkedHitbox(*item, before, item->visual);
    } else {
        item->hitbox = item->defaultHitbox;
        item->hitboxLinked = false;
        ClampEditorRect(item->hitbox);
    }
    SaveEditorJson();
    InvalidateRect(hwnd, nullptr, FALSE);
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

void Color(float r, float g, float b, float a) {
    g_brush->SetColor(D2D1::ColorF(r, g, b, a));
}

void Fill(const Rect& r, float radius) {
    if (radius <= 0.0f) {
        g_target->FillRectangle(D2D1::RectF(r.l, r.t, r.r, r.b), g_brush.Get());
    } else {
        g_target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.l, r.t, r.r, r.b), radius, radius),
            g_brush.Get()
        );
    }
}

void Stroke(const Rect& r, float radius, float width) {
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


void RenderAnimatedBackground() {
    const float t = g_motion;
    for (int band = 0; band < 7; ++band) {
        const float baseY = 142.0f + band * 145.0f;
        const float speed = 0.18f + band * 0.018f;
        Color(0.20f + band * 0.012f, 0.62f + band * 0.015f, 0.48f + band * 0.01f,
              0.020f + (band % 2) * 0.008f);
        float px = -80.0f;
        float py = baseY + std::sin(t * speed + band) * 38.0f;
        for (int segment = 1; segment <= 16; ++segment) {
            const float x = -80.0f + segment * 130.0f;
            const float y = baseY +
                std::sin(t * speed + segment * 0.56f + band * 0.9f) * (32.0f + band * 2.0f) +
                std::cos(t * 0.08f + segment * 0.23f) * 16.0f;
            Line(px, py, x, y, 2.0f + band * 0.08f);
            px = x; py = y;
        }
    }
    const int nodeCount = 12;
    D2D1_POINT_2F nodes[nodeCount]{};
    for (int i = 0; i < nodeCount; ++i) {
        const float phase = t * (0.09f + (i % 4) * 0.018f) + i * 1.17f;
        nodes[i] = D2D1::Point2F(
            90.0f + (std::sin(phase * 0.91f) * 0.5f + 0.5f) * 1740.0f,
            126.0f + (std::cos(phase * 1.07f) * 0.5f + 0.5f) * 820.0f
        );
    }
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = i + 1; j < nodeCount; ++j) {
            const float dx = nodes[i].x - nodes[j].x;
            const float dy = nodes[i].y - nodes[j].y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < 330.0f) {
                Color(0.40f, 0.92f, 0.70f, 0.038f * (1.0f - distance / 330.0f));
                Line(nodes[i].x, nodes[i].y, nodes[j].x, nodes[j].y, 1.0f);
            }
        }
    }
    for (int i = 0; i < nodeCount; ++i) {
        const float pulse = std::sin(t * 1.35f + i * 0.8f) * 0.5f + 0.5f;
        Color(0.45f, 1.0f, 0.78f, 0.08f + pulse * 0.08f);
        Circle(nodes[i].x, nodes[i].y, 4.0f + pulse * 5.0f);
        Color(0.45f, 1.0f, 0.78f, 0.025f + pulse * 0.02f);
        Circle(nodes[i].x, nodes[i].y, 22.0f + pulse * 10.0f);
    }
    for (int i = 0; i < 6; ++i) {
        const float p = std::fmod(t * (0.12f + i * 0.01f) + i * 0.17f, 1.0f);
        const float x = 40.0f + p * 1840.0f;
        const float y = 118.0f + (std::sin(t * 0.22f + i * 1.8f) * 0.5f + 0.5f) * 830.0f;
        Color(0.66f, 1.0f, 0.86f, 0.09f);
        Circle(x, y, 2.0f + std::sin(t * 2.0f + i) * 0.8f);
    }
}

void RenderHeader(float width) {
    Color(0.035f, 0.043f, 0.055f);
    Fill({0.0f, 0.0f, width, 84.0f});

    const float headerSweep = std::sin(g_motion * 0.75f) * 0.5f + 0.5f;
    Color(0.45f, 0.98f, 0.76f, 0.16f);
    Line(90.0f + headerSweep * 520.0f, 82.0f, 330.0f + headerSweep * 520.0f, 82.0f, 1.0f);

    const Rect mark = EditorVisual("header.logo.mark", {24.0f, 23.0f, 60.0f, 59.0f});
    Color(0.60f, 0.98f, 0.79f);
    Fill(mark, std::max(6.0f, std::min(14.0f, RectHeight(mark) * 0.34f)));
    Color(0.035f, 0.055f, 0.045f);
    Circle((mark.l + mark.r) * 0.5f, mark.t + RectHeight(mark) * 0.31f, std::max(3.0f, RectWidth(mark) * 0.19f));
    Line(mark.l + RectWidth(mark) * 0.20f, mark.t + RectHeight(mark) * 0.61f,
         mark.l + RectWidth(mark) * 0.50f, mark.t + RectHeight(mark) * 0.84f, 3.0f);
    Line(mark.r - RectWidth(mark) * 0.20f, mark.t + RectHeight(mark) * 0.61f,
         mark.l + RectWidth(mark) * 0.50f, mark.t + RectHeight(mark) * 0.84f, 3.0f);

    const Rect logoText = EditorVisual("header.logo.text", {74.0f, 23.0f, 155.0f, 55.0f});
    Color(0.90f, 0.94f, 0.96f);
    Text(L"IMUX", logoText, g_title);
    const Rect logoVersion = EditorVisual("header.logo.version", {75.0f, 51.0f, 155.0f, 68.0f});
    Color(0.47f, 0.52f, 0.59f);
    Text(kVersion, logoVersion, g_label);

    const float navStart = 176.0f;
    const float navW = 106.0f;
    const wchar_t* labels[] = {L"Start", L"Library", L"Changelog", L"Settings"};
    const char* ids[] = {"header.nav.start","header.nav.library","header.nav.changelog","header.nav.settings"};
    for (int i = 0; i < 4; ++i) {
        const float x = navStart + i * navW;
        const Rect navRect = EditorVisual(ids[i], {x, 21.0f, x + navW - 7.0f, 63.0f});
        if (g_page == i) {
            Color(0.08f, 0.17f, 0.14f);
            Fill(navRect, std::min(13.0f, RectHeight(navRect) * 0.35f));

            const float navPulse = std::sin(g_motion * 2.0f) * 0.5f + 0.5f;
            Color(0.46f, 0.98f, 0.76f, 0.48f + 0.22f * navPulse);
            Fill({navRect.l + 10.0f, navRect.b - 3.0f,
                  navRect.r - 10.0f, navRect.b - 1.0f}, 1.0f);
        }
        Color(g_page == i ? 0.67f : 0.49f, g_page == i ? 0.97f : 0.55f, g_page == i ? 0.80f : 0.63f);
        Text(labels[i], {navRect.l + RectWidth(navRect)*0.12f, navRect.t + RectHeight(navRect)*0.24f,
                         navRect.r - RectWidth(navRect)*0.12f, navRect.b - RectHeight(navRect)*0.16f}, g_body);
    }

    const Rect guest = EditorVisual("header.guest", {width - 144.0f, 22.0f, width - 22.0f, 62.0f});
    Color(0.10f, 0.13f, 0.17f);
    Fill(guest, std::min(20.0f, RectHeight(guest) * 0.50f));
    const Rect guestText = EditorVisual("header.guest.text",
        {guest.l + 31.0f, guest.t + 9.0f, guest.r - 15.0f, guest.b - 8.0f});
    Color(0.54f, 0.59f, 0.65f);
    Text(L"Guest", guestText, g_body);
}

void RenderLaunchCard(const Rect& box) {
    Color(0.066f, 0.082f, 0.102f);
    Fill(box, 28.0f);

    const Rect glowTop = EditorOrFallback("start.launch.glow.top",
        {box.r - 116.0f, box.t + 18.0f, box.r + 48.0f, box.t + 182.0f});
    const Rect glowBottom = EditorOrFallback("start.launch.glow.bottom",
        {box.r - 150.0f, box.b - 158.0f, box.r + 82.0f, box.b + 74.0f});
    const float pulse = 1.0f + 0.035f * std::sin(g_motion * 1.6f);
    Color(0.25f, 0.78f, 0.57f, 0.06f);
    Circle((glowTop.l + glowTop.r) * 0.5f, (glowTop.t + glowTop.b) * 0.5f,
           std::min(RectWidth(glowTop), RectHeight(glowTop)) * 0.5f * pulse);
    Color(0.25f, 0.78f, 0.57f, 0.035f);
    Circle((glowBottom.l + glowBottom.r) * 0.5f, (glowBottom.t + glowBottom.b) * 0.5f,
           std::min(RectWidth(glowBottom), RectHeight(glowBottom)) * 0.5f * pulse);

    const Rect status = EditorOrFallback("start.launch.status", {box.l + 30.0f, box.t + 28.0f, box.r - 30.0f, box.t + 51.0f});
    Color(0.43f, 0.98f, 0.75f);
    Text(L"READY TO LAUNCH", status, g_label);

    const Rect name = EditorOrFallback("start.launch.name", {box.l + 30.0f, box.t + 59.0f, box.r - 30.0f, box.t + 105.0f});
    Color(0.94f, 0.97f, 0.98f);
    Text(L"Default Client", name, g_display);

    const Rect profile = EditorOrFallback("start.launch.profile", {box.l + 31.0f, box.t + 110.0f, box.r - 30.0f, box.t + 135.0f});
    Color(0.48f, 0.54f, 0.62f);
    Text(L"Guest profile", profile, g_body);

    const Rect infoBox = EditorOrFallback("start.launch.info.box", {box.l + 28.0f, box.t + 158.0f, box.r - 28.0f, box.t + 210.0f});
    Color(0.10f, 0.13f, 0.16f);
    Fill(infoBox, std::min(16.0f, RectHeight(infoBox) * 0.50f));

    const Rect infoLabel = EditorOrFallback("start.launch.info.label", {infoBox.l + 18.0f, infoBox.t + 14.0f, infoBox.l + 136.0f, infoBox.t + 36.0f});
    const Rect infoValue = EditorOrFallback("start.launch.info.value", {infoBox.l + 146.0f, infoBox.t + 14.0f, infoBox.r - 16.0f, infoBox.t + 36.0f});
    const bool external = !imux_launcher_find_game_executable().empty();
    Color(0.43f, 0.98f, 0.75f);
    Text(external ? L"GAME" : L"BASE WORLD", infoLabel, g_label);
    Color(0.49f, 0.55f, 0.63f);
    Text(external ? L"ImuxGame.exe" : L"Built-in first-person world", infoValue, g_label);

    const Rect version = EditorOrFallback("start.launch.version", {box.l + 31.0f, box.t + 228.0f, box.l + 100.0f, box.t + 250.0f});
    Color(0.37f, 0.42f, 0.49f);
    Text(L"0.0.1", version, g_label);

    const Rect buttonRect = EditorOrFallback("start.play",
        {box.l + 28.0f, box.b - 82.0f, box.l + 298.0f, box.b - 24.0f});
    const float buttonH = RectHeight(buttonRect);
    Color(g_hoverPlay ? 0.62f : 0.52f, g_hoverPlay ? 1.00f : 0.96f, g_hoverPlay ? 0.82f : 0.74f);
    Fill(buttonRect, std::max(8.0f, std::min(24.0f, buttonH * 0.34f)));

    const float playPulse = std::sin(g_motion * 2.8f) * 0.5f + 0.5f;
    if (g_hoverPlay) {
        Color(0.60f, 1.0f, 0.80f, 0.30f + 0.12f * playPulse);
        Stroke(buttonRect, std::max(8.0f, std::min(24.0f, buttonH * 0.34f)), 1.5f + playPulse);
    }
    const float sheen = std::fmod(g_motion * 0.55f, 1.0f);
    const float sheenX = buttonRect.l - 80.0f + sheen * (RectWidth(buttonRect) + 160.0f);
    const Rect sheenRect{sheenX, buttonRect.t + 2.0f, sheenX + 42.0f, buttonRect.b - 2.0f};
    Color(1.0f, 1.0f, 1.0f, 0.045f);
    Fill(sheenRect, std::min(12.0f, buttonH * 0.25f));

    const Rect iconRect = EditorOrFallback("start.play.icon",
        {buttonRect.l + 18.0f, buttonRect.t + buttonH * 0.20f, buttonRect.l + 44.0f, buttonRect.t + buttonH * 0.80f});
    PlayGlyph((iconRect.l + iconRect.r) * 0.5f, (iconRect.t + iconRect.b) * 0.5f,
              std::min(RectWidth(iconRect), RectHeight(iconRect)));

    const Rect playText = EditorOrFallback("start.play.text",
        {buttonRect.l + 55.0f, buttonRect.t + buttonH * 0.24f, buttonRect.r - 18.0f, buttonRect.t + buttonH * 0.76f});
    Color(0.035f, 0.055f, 0.05f);
    Text(L"PLAY", playText, g_button);
}

void RenderReleaseCard(const Rect& box) {
    Color(0.082f, 0.100f, 0.125f);
    Fill(box, 28.0f);

    const Rect heading = EditorOrFallback("start.release.heading", {box.l + 26.0f, box.t + 28.0f, box.r - 24.0f, box.t + 51.0f});
    Color(0.43f, 0.98f, 0.75f);
    Text(L"WHAT'S NEW", heading, g_label);

    const Rect version = EditorOrFallback("start.release.version", {box.l + 26.0f, box.t + 58.0f, box.r - 24.0f, box.t + 96.0f});
    Color(0.94f, 0.97f, 0.98f);
    Text(L"0.0.1", version, g_title);

    const Rect date = EditorOrFallback("start.release.date", {box.l + 27.0f, box.t + 99.0f, box.r - 24.0f, box.t + 122.0f});
    Color(0.46f, 0.52f, 0.60f);
    Text(L"2026-09-19", date, g_body);

    const Rect divider = EditorOrFallback("start.release.divider", {box.l + 24.0f, box.t + 136.0f, box.r - 24.0f, box.t + 137.0f});
    Color(0.16f, 0.19f, 0.23f);
    Fill(divider);

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
        char id[64]{};
        sprintf_s(id, "start.release.item.%d", i + 1);
        const Rect row = EditorOrFallback(id, {box.l + 26.0f, y - 2.0f, box.r - 22.0f, y + 23.0f});
        Color(0.60f, 0.96f, 0.78f);
        Circle(row.l + 5.0f, (row.t + row.b) * 0.5f, std::max(2.0f, std::min(4.0f, RectHeight(row) * 0.18f)));
        Color(0.66f, 0.71f, 0.77f);
        Text(items[i], {row.l + 14.0f, row.t, row.r, row.b}, g_body);
    }
}

void RenderStart(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Start", EditorVisual("start.title", {left, top, right, top + 45.0f}), g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"Launch the base game from one focused workspace.",
         EditorVisual("start.subtitle", {left, top + 48.0f, right, top + 74.0f}), g_body);

    const float contentTop = top + 94.0f;
    const float gap = 16.0f;
    const float available = right - left;
    if (available < 920.0f) {
        const float heroBottom = std::min(bottom - 170.0f, contentTop + 290.0f);
        RenderLaunchCard(EditorVisual("start.launch.card", {left, contentTop, right, heroBottom}));
        RenderReleaseCard(EditorVisual("start.release.card", {left, heroBottom + gap, right, bottom}));
    } else {
        const float heroRight = left + available * 0.66f;
        RenderLaunchCard(EditorVisual("start.launch.card", {left, contentTop, heroRight - gap * 0.5f, bottom}));
        RenderReleaseCard(EditorVisual("start.release.card", {heroRight + gap * 0.5f, contentTop, right, bottom}));
    }
}

void RenderChangelog(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Changelog", EditorVisual("changelog.title", {left, top, right, top + 45.0f}), g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"Release history is kept here until the next version is approved.",
         EditorVisual("changelog.subtitle", {left, top + 48.0f, right, top + 74.0f}), g_body);

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
    Text(L"0.0.1", EditorVisual("changelog.version", {left, y0, left + 120.0f, y0 + 26.0f}), g_title);
    Color(0.45f, 0.50f, 0.58f);
    Text(L"2026-09-19", EditorVisual("changelog.date", {right - 130.0f, y0 + 4.0f, right, y0 + 26.0f}), g_label);

    for (int i = 0; i < 5; ++i) {
        char rowId[64]{}, dotId[64]{}, textId[64]{};
        sprintf_s(rowId, "changelog.row.%d", i + 1);
        sprintf_s(dotId, "changelog.row.%d.dot", i + 1);
        sprintf_s(textId, "changelog.row.%d.text", i + 1);

        const float y = y0 + 38.0f + i * h;
        const Rect row = EditorVisual(rowId, {left, y, right, y + h - 8.0f});
        Color(0.066f, 0.082f, 0.102f);
        Fill(row, std::min(17.0f, RectHeight(row) * 0.35f));

        const Rect dot = EditorOrFallback(dotId, {row.l + 18.0f, row.t + 18.0f, row.l + 26.0f, row.t + 26.0f});
        Color(0.46f, 0.98f, 0.76f);
        Circle((dot.l + dot.r) * 0.5f, (dot.t + dot.b) * 0.5f,
               std::max(2.0f, std::min(4.0f, std::min(RectWidth(dot), RectHeight(dot)) * 0.5f)));

        const Rect text = EditorOrFallback(textId, {row.l + 39.0f, row.t + 10.0f, row.r - 20.0f, row.b - 4.0f});
        Color(0.78f, 0.82f, 0.87f);
        Text(entries[i], text, g_body);
    }
}

void RenderLibrary(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Library", EditorVisual("library.title", {left, top, right, top + 45.0f}), g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"The current installation is the only launch target implemented.",
         EditorVisual("library.subtitle", {left, top + 48.0f, right, top + 74.0f}), g_body);

    const float y0 = top + 96.0f;
    const Rect active = EditorVisual("library.active.card", {left, y0, right, y0 + 188.0f});
    Color(0.066f, 0.082f, 0.102f);
    Fill(active, 26.0f);

    const Rect badge = EditorOrFallback("library.active.badge", {active.l + 26.0f, active.t + 25.0f, active.l + 100.0f, active.t + 48.0f});
    const Rect name = EditorOrFallback("library.active.name", {active.l + 26.0f, active.t + 54.0f, active.r - 20.0f, active.t + 92.0f});
    const Rect versionLabel = EditorOrFallback("library.active.version.label", {active.l + 26.0f, active.t + 108.0f, active.l + 190.0f, active.t + 132.0f});
    const Rect versionValue = EditorOrFallback("library.active.version.value", {active.l + 190.0f, active.t + 108.0f, active.r - 20.0f, active.t + 132.0f});
    const Rect dirLabel = EditorOrFallback("library.active.directory.label", {active.l + 26.0f, active.t + 141.0f, active.l + 190.0f, active.t + 165.0f});
    const Rect dirValue = EditorOrFallback("library.active.directory.value", {active.l + 190.0f, active.t + 141.0f, active.r - 20.0f, active.t + 165.0f});

    Color(0.43f, 0.98f, 0.75f);
    Text(L"ACTIVE", badge, g_label);
    Color(0.94f, 0.97f, 0.98f);
    Text(L"Default Client", name, g_title);
    Color(0.50f, 0.55f, 0.63f);
    Text(L"Version", versionLabel, g_label);
    Text(L"Directory", dirLabel, g_label);
    Color(0.78f, 0.82f, 0.87f);
    Text(L"0.0.1", versionValue, g_body);
    Text(L"%APPDATA%\\Imux\\instances\\default", dirValue, g_body);

    const Rect soon = EditorVisual("library.soon", {left, y0 + 208.0f, right, y0 + 274.0f});
    Color(0.15f, 0.18f, 0.22f);
    Fill(soon, 19.0f);
    Color(0.43f, 0.98f, 0.75f);
    Text(L"SOON", EditorOrFallback("library.soon.text", {soon.l + 25.0f, soon.t + 22.0f, soon.l + 90.0f, soon.t + 45.0f}), g_label);
}

void RenderSettings(float left, float right, float top, float bottom) {
    Color(0.93f, 0.96f, 0.98f);
    Text(L"Settings", EditorVisual("settings.title", {left, top, right, top + 45.0f}), g_display);
    Color(0.48f, 0.54f, 0.62f);
    Text(L"Concrete diagnostics are shown here. Unimplemented controls stay empty.",
         EditorVisual("settings.subtitle", {left, top + 48.0f, right, top + 74.0f}), g_body);

    const float y0 = top + 96.0f;
    const float rowH = 66.0f;
    const wchar_t* labels[] = {L"Version", L"Launcher log", L"Game data"};
    const std::wstring appData = imux_launcher_app_data_root().wstring();
    const std::wstring gameData = appData.empty() ? L"%APPDATA%\\Imux" : appData;
    const std::wstring logPath = g_logPath.empty() ? L"imux.log" : std::wstring(g_logPath.begin(), g_logPath.end());
    const std::wstring values[] = {kVersion, logPath, gameData};

    for (int i = 0; i < 3; ++i) {
        const float y = y0 + i * rowH;
        char rowId[64]{}, labelId[64]{}, valueId[64]{};
        sprintf_s(rowId, "settings.row.%d", i + 1);
        sprintf_s(labelId, "settings.row.%d.label", i + 1);
        sprintf_s(valueId, "settings.row.%d.value", i + 1);

        const Rect row = EditorVisual(rowId, {left, y, right, y + rowH - 10.0f});
        Color(0.066f, 0.082f, 0.102f);
        Fill(row, std::min(17.0f, RectHeight(row) * 0.30f));

        const Rect label = EditorOrFallback(labelId, {row.l + 20.0f, row.t + 11.0f, row.l + 170.0f, row.t + 33.0f});
        const Rect value = EditorOrFallback(valueId, {row.l + 170.0f, row.t + 10.0f, row.r - 18.0f, row.t + 38.0f});
        Color(0.48f, 0.54f, 0.62f);
        Text(labels[i], label, g_label);
        Color(0.79f, 0.83f, 0.88f);
        Text(values[i].c_str(), value, g_body);
    }

    const float soonY = y0 + rowH * 3.0f + 8.0f;
    const Rect soon = EditorVisual("settings.soon", {left, soonY, right, std::min(bottom, soonY + 74.0f)});
    Color(0.15f, 0.18f, 0.22f);
    Fill(soon, 19.0f);
    Color(0.43f, 0.98f, 0.75f);
    Text(L"SOON", EditorOrFallback("settings.soon.text", {soon.l + 25.0f, soon.t + 25.0f, soon.l + 95.0f, soon.t + 49.0f}), g_label);
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

    RenderAnimatedBackground();
    RenderHeader(kDesignWidth);

    const LauncherLayout layout = CalculateLauncherLayout();
    const float left = layout.left;
    const float right = layout.right;
    const float top = layout.top;
    const float bottom = layout.bottom;

    const float transition = 1.0f - std::pow(1.0f - std::min(g_pageAnim, 1.0f), 3.0f);
    const float pageShift = (1.0f - transition) * 42.0f;
    g_target->SetTransform(D2D1::Matrix3x2F(
        viewport.scale, 0.0f, 0.0f, viewport.scale,
        viewport.offsetX + pageShift, viewport.offsetY
    ));

    switch (g_page) {
        case 0: RenderStart(left, right, top, bottom); break;
        case 1: RenderLibrary(left, right, top, bottom); break;
        case 2: RenderChangelog(left, right, top, bottom); break;
        case 3: RenderSettings(left, right, top, bottom); break;
        default: g_page = 0; RenderStart(left, right, top, bottom); break;
    }

    RenderEditorOverlay();

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

    g_target->SetDpi(96.0f, 96.0f);

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
    const char* ids[] = {
        "header.nav.start",
        "header.nav.library",
        "header.nav.changelog",
        "header.nav.settings"
    };
    const float navStart = 176.0f;
    const float navW = 106.0f;
    for (int i = 0; i < 4; ++i) {
        const Rect fallback{navStart + i * navW, 21.0f, navStart + i * navW + navW - 7.0f, 63.0f};
        if (Hit(EditorHitbox(ids[i], fallback), x, y)) return i;
    }
    return -1;
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

bool HandleEditorKey(HWND hwnd, WPARAM wp, LPARAM lp) {
    if (!g_editorMode) return false;

    if (wp == VK_TAB) {
        g_editorTarget = g_editorTarget == EditorTarget::Visual ? EditorTarget::Hitbox : EditorTarget::Visual;
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    if (wp == VK_ESCAPE) {
        g_editorMode = false;
        g_editorDragging = false;
        ReleaseCapture();
        SaveEditorJson();
        InvalidateRect(hwnd, nullptr, FALSE);
        Log("UI editor: disabled");
        return true;
    }

    if (wp >= '1' && wp <= '4') {
        g_page = static_cast<int>(wp - '1');
        g_editorSelected.clear();
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    if (wp == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        SaveEditorJson();
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    if (wp == VK_BACK) {
        EditorResetSelected(hwnd);
        return true;
    }

    if (wp == 'L' || wp == 'l') {
        RelinkSelectedHitbox();
        SaveEditorJson();
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN) {
        auto* item = FindEditorItem(g_editorSelected);
        if (!item) return true;

        const float step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10.0f : 1.0f;
        const Rect before = EditorActiveRect(*item);
        Rect& rect = EditorActiveRect(*item);
        if (wp == VK_LEFT) rect.l -= step, rect.r -= step;
        if (wp == VK_RIGHT) rect.l += step, rect.r += step;
        if (wp == VK_UP) rect.t -= step, rect.b -= step;
        if (wp == VK_DOWN) rect.t += step, rect.b += step;
        ClampEditorRect(rect);
        if (g_editorTarget == EditorTarget::Visual) SyncLinkedHitbox(*item, before, rect);
        else item->hitboxLinked = false;
        SaveEditorJson();
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }

    return false;
}

void BeginEditorDrag(HWND hwnd, float x, float y) {
    if (x < 0.0f || y < 0.0f || x > kDesignWidth || y > kDesignHeight) return;
    InitializeEditorItems();

    EditorItem* item = FindEditorItemAt(x, y);
    if (!item) {
        g_editorSelected.clear();
        g_editorHandle = EditorHandle::None;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    g_editorSelected = item->id;
    Rect& active = EditorActiveRect(*item);
    g_editorHandle = HitResizeHandle(active, x, y);
    g_editorDragging = true;
    g_editorDragStart = D2D1::Point2F(x, y);
    g_editorDragOrigin = active;
    SetCapture(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void UpdateEditorDrag(HWND hwnd, float x, float y) {
    if (!g_editorDragging) return;
    if (x < 0.0f || y < 0.0f || x > kDesignWidth || y > kDesignHeight) return;

    auto* item = FindEditorItem(g_editorSelected);
    if (!item) return;

    Rect updated = g_editorDragOrigin;
    const float dx = x - g_editorDragStart.x;
    const float dy = y - g_editorDragStart.y;

    if (g_editorHandle == EditorHandle::None) {
        updated.l += dx;
        updated.r += dx;
        updated.t += dy;
        updated.b += dy;
    } else {
        if (g_editorHandle == EditorHandle::NW || g_editorHandle == EditorHandle::W || g_editorHandle == EditorHandle::SW) updated.l += dx;
        if (g_editorHandle == EditorHandle::NE || g_editorHandle == EditorHandle::E || g_editorHandle == EditorHandle::SE) updated.r += dx;
        if (g_editorHandle == EditorHandle::NW || g_editorHandle == EditorHandle::N || g_editorHandle == EditorHandle::NE) updated.t += dy;
        if (g_editorHandle == EditorHandle::SW || g_editorHandle == EditorHandle::S || g_editorHandle == EditorHandle::SE) updated.b += dy;
    }

    const float minSize = 6.0f;
    if (updated.r - updated.l < minSize) {
        if (g_editorHandle == EditorHandle::NW || g_editorHandle == EditorHandle::W || g_editorHandle == EditorHandle::SW) updated.l = updated.r - minSize;
        else updated.r = updated.l + minSize;
    }
    if (updated.b - updated.t < minSize) {
        if (g_editorHandle == EditorHandle::NW || g_editorHandle == EditorHandle::N || g_editorHandle == EditorHandle::NE) updated.t = updated.b - minSize;
        else updated.b = updated.t + minSize;
    }

    if (g_editorTarget == EditorTarget::Visual) {
        const Rect before = g_editorDragOrigin;
        EditorActiveRect(*item) = updated;
        ClampEditorRect(EditorActiveRect(*item));
        SyncLinkedHitbox(*item, before, EditorActiveRect(*item));
    } else {
        item->hitboxLinked = false;
        EditorActiveRect(*item) = updated;
        ClampEditorRect(EditorActiveRect(*item));
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EndEditorDrag(HWND hwnd) {
    if (!g_editorDragging) return;
    g_editorDragging = false;
    g_editorHandle = EditorHandle::None;
    ReleaseCapture();
    SaveEditorJson();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void ToggleEditor(HWND hwnd) {
    InitializeEditorItems();
    g_editorMode = !g_editorMode;
    g_editorDragging = false;
    g_editorSelected.clear();
    g_editorHandle = EditorHandle::None;
    ReleaseCapture();
    if (g_editorMode) {
        SaveEditorJson();
        Log("UI editor: enabled; target=visual");
    } else {
        SaveEditorJson();
        Log("UI editor: disabled");
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_F11) {
        ToggleFullscreen(hwnd);
        return 0;
    }

    if (!g_inWorld && msg == WM_KEYDOWN && (wp == 'R' || wp == 'r')) {
        ToggleEditor(hwnd);
        return 0;
    }

    if (!g_inWorld && msg == WM_KEYDOWN && HandleEditorKey(hwnd, wp, lp)) {
        return 0;
    }

    if (g_editorMode && !g_inWorld) {
        switch (msg) {
            case WM_LBUTTONDOWN: {
                D2D1_POINT_2F point = ClientToUiPoint(
                    static_cast<float>(GET_X_LPARAM(lp)),
                    static_cast<float>(GET_Y_LPARAM(lp))
                );
                BeginEditorDrag(hwnd, point.x, point.y);
                return 0;
            }

            case WM_MOUSEMOVE: {
                D2D1_POINT_2F point = ClientToUiPoint(
                    static_cast<float>(GET_X_LPARAM(lp)),
                    static_cast<float>(GET_Y_LPARAM(lp))
                );
                if (g_editorDragging) {
                    UpdateEditorDrag(hwnd, point.x, point.y);
                }
                return 0;
            }

            case WM_LBUTTONUP:
                EndEditorDrag(hwnd);
                return 0;
        }
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
            g_mainWindow = hwnd;
            g_windowedStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
            g_windowedExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            g_windowedPlacement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(hwnd, &g_windowedPlacement);
            InitializeGraphics(hwnd);
            InitializeEditorItems();
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
            D2D1_POINT_2F point = ClientToUiPoint(x, y);
            const LauncherLayout layout = CalculateLauncherLayout();
            const bool hover = g_page == 0 && Hit(EditorHitbox("start.play", layout.playRect), point.x, point.y);
            if (hover != g_hoverPlay) {
                g_hoverPlay = hover;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            const float x = static_cast<float>(GET_X_LPARAM(lp));
            const float y = static_cast<float>(GET_Y_LPARAM(lp));
            D2D1_POINT_2F point = ClientToUiPoint(x, y);

            const int headerPage = HeaderPageAt(point.x, point.y);
            if (headerPage >= 0) {
                g_page = headerPage;
                g_pageAnim = 0.0f;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (g_page == 0) {
                const LauncherLayout layout = CalculateLauncherLayout();

                if (Hit(EditorHitbox("start.play", layout.playRect), point.x, point.y)) {
                    Log("Play clicked");
                    const int externalResult = imux_launcher_try_launch_game();
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
                g_pageAnim = std::min(1.0f, g_pageAnim + 0.11f);
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
