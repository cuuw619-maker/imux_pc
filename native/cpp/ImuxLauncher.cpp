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
#include <fstream>
#include <string>
#include <exception>
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
ComPtr<IDWriteTextFormat> g_display, g_title, g_body, g_label, g_button;
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

struct Rect {
    float l, t, r, b;
    Rect(float left, float top, float right, float bottom) : l(left), t(top), r(right), b(bottom) {}
};
bool Hit(const Rect& r, float x, float y) { return x >= r.l && x <= r.r && y >= r.t && y <= r.b; }
float Lerp(float a, float b, float t) { return a + (b - a) * t; }
float Clamp(float value, float lo, float hi) { return std::max(lo, std::min(value, hi)); }

int g_page = 0;
bool g_drawerOpen = false;
bool g_hoverPlay = false;
bool g_inWorld = false;
float g_drawerProgress = 0.0f;
float g_motion = 0.0f;
float g_playPulse = 0.0f;

void Color(float r, float g, float b, float a = 1.0f) {
    g_brush->SetColor(D2D1::ColorF(r, g, b, a));
}

void Fill(const Rect& r, float radius = 0.0f) {
    if (radius <= 0.0f) {
        g_target->FillRectangle(D2D1::RectF(r.l, r.t, r.r, r.b), g_brush.Get());
    } else {
        g_target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.l, r.t, r.r, r.b), radius, radius), g_brush.Get());
    }
}

void Stroke(const Rect& r, float radius = 0.0f, float width = 1.0f) {
    if (radius <= 0.0f) {
        g_target->DrawRectangle(D2D1::RectF(r.l, r.t, r.r, r.b), g_brush.Get(), width);
    } else {
        g_target->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.l, r.t, r.r, r.b), radius, radius), g_brush.Get(), width);
    }
}

void Text(const wchar_t* value, const Rect& r, const ComPtr<IDWriteTextFormat>& format) {
    g_target->DrawTextW(value, static_cast<UINT32>(wcslen(value)), format.Get(),
        D2D1::RectF(r.l, r.t, r.r, r.b), g_brush.Get());
}

void Circle(float x, float y, float radius) {
    g_target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), g_brush.Get());
}

void Icon(int kind, float x, float y, float size, bool active = false) {
    const float s = size;
    Color(active ? 0.72f : 0.55f, active ? 0.98f : 0.61f, active ? 0.86f : 0.67f);
    if (kind == 0) { // home
        g_target->DrawLine(D2D1::Point2F(x-s*.34f,y), D2D1::Point2F(x,y-s*.32f), g_brush.Get(), 3.0f);
        g_target->DrawLine(D2D1::Point2F(x,y-s*.32f), D2D1::Point2F(x+s*.34f,y), g_brush.Get(), 3.0f);
        g_target->FillRectangle(D2D1::RectF(x-s*.22f, y, x+s*.22f, y+s*.30f), g_brush.Get());
    } else if (kind == 1) { // instances
        Fill({x-s*.32f,y-s*.26f,x+s*.32f,y-s*.06f}, 3);
        Fill({x-s*.32f,y+s*.03f,x+s*.32f,y+s*.23f}, 3);
    } else if (kind == 2) { // engine
        Circle(x, y, s*.30f);
        Color(0.055f,0.065f,0.08f); Circle(x,y,s*.11f);
        Color(active ? 0.72f : 0.55f, active ? 0.98f : 0.61f, active ? 0.86f : 0.67f);
        Stroke({x-s*.43f,y-s*.43f,x+s*.43f,y+s*.43f}, s*.43f, 2.0f);
    } else { // settings
        Stroke({x-s*.31f,y-s*.31f,x+s*.31f,y+s*.31f}, 5, 2.0f);
        Circle(x,y,s*.10f);
    }
}

void PlayGlyph(float x, float y, float size) {
    Color(0.035f, 0.055f, 0.050f);
    const D2D1_POINT_2F p0 = D2D1::Point2F(x - size*.22f, y - size*.34f);
    const D2D1_POINT_2F p1 = D2D1::Point2F(x + size*.34f, y);
    const D2D1_POINT_2F p2 = D2D1::Point2F(x - size*.22f, y + size*.34f);
    g_target->DrawLine(p0, p1, g_brush.Get(), 3.0f);
    g_target->DrawLine(p1, p2, g_brush.Get(), 3.0f);
    g_target->DrawLine(p2, p0, g_brush.Get(), 3.0f);
}

void RenderHeader(float width, float height) {
    Color(0.045f, 0.052f, 0.067f, 0.96f);
    Fill({0,0,width,76});
    Color(0.09f,0.10f,0.13f);
    Fill({20,18,60,58},12);
    Color(0.88f,0.92f,0.94f);
    for (int i=0;i<3;++i)
        Fill({32,29+i*7,48,31+i*7},1);

    Color(0.68f,0.98f,0.84f);
    Text(L"IMUX", {78,19,180,53}, g_title);

    Color(0.45f,0.49f,0.57f);
    const wchar_t* sections[] = {L"HOME",L"INSTANCES",L"ENGINE",L"SETTINGS"};
    Text(sections[g_page], {190,25,390,50}, g_label);

    Color(0.12f,0.14f,0.18f);
    Fill({width-190,17,width-20,59},21);
    Color(0.78f,0.82f,0.87f);
    Circle(width-166,38,14);
    Color(0.16f,0.20f,0.25f);
    Circle(width-166,34,5);
    Color(0.16f,0.20f,0.25f);
    Fill({width-173,41,width-159,48},5);
    Color(0.72f,0.76f,0.82f);
    Text(L"Guest", {width-142,25,width-42,48}, g_body);
}

void RenderHome(float left, float right, float top, float bottom) {
    const float w = right-left;
    const bool compact = w < 920.0f;
    Color(0.90f,0.94f,0.97f);
    Text(L"Your workspace", {left,top,right,top+42}, g_display);
    Color(0.50f,0.55f,0.63f);
    Text(L"One place to start Imux, inspect the environment and enter the test world.",
         {left,top+48,right,top+78}, g_body);

    const float bodyTop = top+102;
    const float playW = compact ? w : w*0.68f;
    const float sideL = compact ? left : left+playW+18;
    const float playR = compact ? right : left+playW;

    Color(0.075f,0.092f,0.115f);
    Fill({left,bodyTop,playR,bottom},28);

    // Large animated accent behind the primary action.
    const float pulse = 1.0f + 0.035f*std::sin(g_motion*1.7f);
    Color(0.11f,0.30f,0.24f,0.22f);
    Circle(playR-80,bodyTop+82,72*pulse);
    Color(0.16f,0.53f,0.40f,0.10f);
    Circle(playR-122,bodyTop+128,118*pulse);

    Color(0.45f,0.98f,0.78f);
    Text(L"READY TO PLAY", {left+30,bodyTop+28,playR-30,bodyTop+55}, g_label);
    Color(0.94f,0.97f,0.98f);
    Text(L"Default Client", {left+30,bodyTop+64,playR-30,bodyTop+111}, g_display);
    Color(0.54f,0.59f,0.66f);
    Text(L"Windows x64  /  Guest profile  /  Development build",
         {left+32,bodyTop+119,playR-30,bodyTop+145}, g_body);

    Color(0.12f,0.16f,0.16f);
    Fill({left+28,bodyTop+170,playR-28,bodyTop+214},15);
    Color(0.52f,0.94f,0.76f);
    Text(L"JAVA 21", {left+46,bodyTop+181,left+150,bodyTop+204}, g_label);
    Color(0.46f,0.51f,0.59f);
    Text(L"Runtime available", {left+154,bodyTop+181,playR-45,bodyTop+204}, g_label);

    const float buttonW = compact ? 220.0f : 238.0f;
    const float bx = left+28;
    const float by = bottom-74;
    Color(g_hoverPlay ? 0.56f : 0.45f, g_hoverPlay ? 1.0f : 0.94f, g_hoverPlay ? 0.80f : 0.72f);
    Fill({bx,by,bx+buttonW,by+52},18);
    PlayGlyph(bx+28,by+26,18);
    Color(0.035f,0.055f,0.05f);
    Text(L"PLAY TEST WORLD", {bx+52,by+12,bx+buttonW-16,by+39}, g_button);

    if (!compact) {
        Color(0.075f,0.092f,0.115f);
        Fill({sideL,bodyTop,right,bottom},28);
        Color(0.51f,0.55f,0.62f);
        Text(L"PROFILE", {sideL+24,bodyTop+25,right-24,bodyTop+50}, g_label);
        Color(0.92f,0.95f,0.97f);
        Text(L"Guest", {sideL+24,bodyTop+60,right-24,bodyTop+94}, g_title);

        const wchar_t* labels[] = {L"Account",L"Instance",L"Memory",L"Mods"};
        const wchar_t* values[] = {L"Without account",L"Default Client",L"1–4 GB",L"None"};
        for (int i=0;i<4;++i) {
            const float y=bodyTop+116+i*55;
            Color(0.40f,0.45f,0.52f);
            Text(labels[i],{sideL+24,y,right-24,y+20},g_label);
            Color(0.77f,0.81f,0.86f);
            Text(values[i],{sideL+24,y+20,right-24,y+42},g_body);
        }
        Color(0.14f,0.17f,0.21f);
        Fill({sideL+22,bottom-58,right-22,bottom-22},12);
        Color(0.43f,0.98f,0.75f);
        Text(L"ONLINE", {sideL+38,bottom-49,sideL+120,bottom-28},g_label);
    }

}

void RenderListPage(int page, float left, float right, float top, float bottom) {
    Color(0.90f,0.94f,0.97f);
    const wchar_t* title[] = {L"Instances",L"Engine",L"Settings"};
    const wchar_t* sub[] = {
        L"Manage isolated environments without cluttering the launch screen.",
        L"Rendering, world and interop systems currently connected to the launcher.",
        L"Launcher preferences and diagnostics."
    };
    Text(title[page-1], {left,top,right,top+42}, g_display);
    Color(0.50f,0.55f,0.63f);
    Text(sub[page-1], {left,top+48,right,top+78}, g_body);

    const float y0=top+102;
    if (page==1) {
        Color(0.075f,0.092f,0.115f); Fill({left,y0,right,y0+124},24);
        Color(0.45f,0.98f,0.78f); Text(L"ACTIVE INSTANCE",{left+26,y0+23,left+180,y0+46},g_label);
        Color(0.93f,0.96f,0.98f); Text(L"Default Client",{left+26,y0+52,right-26,y0+88},g_title);
        Color(0.50f,0.55f,0.63f); Text(L"Guest  •  Java 21  •  1–4 GB  •  No mods",{left+26,y0+91,right-26,y0+114},g_body);
        const float rowTop=y0+142;
        const wchar_t* names[]={L"Runtime",L"Storage",L"Launch target"};
        const wchar_t* vals[]={L"Java 21",L"%APPDATA%\\Imux",L"Development runtime"};
        for(int i=0;i<3;++i){
            const float y=rowTop+i*72;
            Color(0.068f,0.083f,0.105f); Fill({left,y,right,y+58},16);
            Color(0.46f,0.51f,0.59f); Text(names[i],{left+20,y+9,left+220,y+30},g_label);
            Color(0.82f,0.86f,0.90f); Text(vals[i],{left+220,y+9,right-20,y+34},g_body);
        }
    } else if (page==2) {
        const wchar_t* names[]={L"UI renderer",L"3D renderer",L"Shader path",L"Interop",L"Build"};
        const wchar_t* vals[]={L"Direct2D + DirectWrite",L"Direct3D 11",L"HLSL",L"C / Rust ABI",L"CMake + Gradle + CI"};
        for(int i=0;i<5;++i){
            const float y=y0+i*70;
            Color(0.068f,0.083f,0.105f); Fill({left,y,right,y+56},16);
            Color(0.45f,0.98f,0.78f); Text(names[i],{left+20,y+10,left+210,y+33},g_label);
            Color(0.78f,0.82f,0.87f); Text(vals[i],{left+210,y+10,right-20,y+34},g_body);
        }
    } else {
        const wchar_t* names[]={L"Appearance",L"Runtime",L"Diagnostics",L"Advanced"};
        const wchar_t* vals[]={
            L"Material 3 inspired dark surface system",
            L"Automatic Java runtime validation",
            L"imux.log beside the launched executable",
            L"Native renderer and engine development options"
        };
        for(int i=0;i<4;++i){
            const float y=y0+i*78;
            Color(0.068f,0.083f,0.105f); Fill({left,y,right,y+64},18);
            Color(0.82f,0.86f,0.90f); Text(names[i],{left+22,y+11,left+220,y+35},g_body);
            Color(0.48f,0.53f,0.61f); Text(vals[i],{left+220,y+12,right-22,y+36},g_label);
        }
    }
}

void RenderDrawer(float width, float height) {
    if (g_drawerProgress <= 0.001f) return;
    Color(0,0,0,0.52f*g_drawerProgress);
    Fill({0,76,width,height});
    const float drawerW = std::min(360.0f,width*0.84f);
    const float x=Lerp(-drawerW,0,g_drawerProgress);
    Color(0.055f,0.066f,0.085f);
    Fill({x,0,x+drawerW,height});
    Color(0.45f,0.98f,0.78f);
    Text(L"IMUX",{x+28,30,x+drawerW-24,68},g_display);
    Color(0.43f,0.48f,0.56f);
    Text(L"NAVIGATION",{x+30,82,x+drawerW-20,104},g_label);

    const wchar_t* names[]={L"Home",L"Instances",L"Engine",L"Settings"};
    for(int i=0;i<4;++i){
        const Rect item{x+14,124+i*62,x+drawerW-14,176+i*62};
        Color(i==g_page?0.12f:0.075f,i==g_page?0.20f:0.09f,i==g_page?0.16f:0.115f);
        Fill(item,15);
        Icon(i,item.l+31,(item.t+item.b)*.5f,22,i==g_page);
        Color(i==g_page?0.88f:0.70f,i==g_page?0.94f:0.75f,i==g_page?0.90f:0.81f);
        Text(names[i],{item.l+58,item.t+13,item.r-18,item.b-8},g_body);
    }
    Color(0.37f,0.42f,0.50f);
    Text(L"Guest  •  Without account",{x+30,height-56,x+drawerW-18,height-30},g_label);
}

void InitializeGraphics(HWND hwnd);

void Render(HWND hwnd) {
    if (!g_target) return;
    g_target->BeginDraw();
    RECT rc{}; GetClientRect(hwnd,&rc);
    const float width=static_cast<float>(rc.right);
    const float height=static_cast<float>(rc.bottom);

    Color(0.032f,0.038f,0.050f);
    g_target->Clear(D2D1::ColorF(0.032f,0.038f,0.050f));
    RenderHeader(width,height);

    const float edge=width<1000.0f?22.0f:44.0f;
    const float contentW=std::min(1240.0f,std::max(320.0f,width-edge*2.0f));
    const float left=(width-contentW)*.5f;
    const float right=left+contentW;
    const float top=108.0f;
    const float bottom=height-34.0f;

    if(g_page==0) RenderHome(left,right,top,bottom);
    else RenderListPage(g_page,left,right,top,bottom);

    RenderDrawer(width,height);

    const HRESULT hr=g_target->EndDraw();
    if(hr==D2DERR_RECREATE_TARGET){
        g_target.Reset();
        g_brush.Reset();
        Log("Direct2D requested render target recreation");
        InitializeGraphics(hwnd);
    }
}

void InitializeGraphics(HWND hwnd) {
    Log("InitializeGraphics: begin");
    HRESULT hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,g_d2dFactory.GetAddressOf());
    Log("D2D1CreateFactory: hr=0x%08lX",static_cast<unsigned long>(hr));
    hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_writeFactory.GetAddressOf()));
    Log("DWriteCreateFactory: hr=0x%08lX",static_cast<unsigned long>(hr));

    RECT rc{}; GetClientRect(hwnd,&rc);
    hr=g_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT),
        D2D1::HwndRenderTargetProperties(hwnd,D2D1::SizeU(
            static_cast<UINT>(std::max(1L,rc.right)),
            static_cast<UINT>(std::max(1L,rc.bottom)))),&g_target);
    Log("CreateHwndRenderTarget: hr=0x%08lX",static_cast<unsigned long>(hr));
    if(FAILED(hr)) return;

    hr=g_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),&g_brush);
    Log("CreateSolidColorBrush: hr=0x%08lX",static_cast<unsigned long>(hr));

    g_writeFactory->CreateTextFormat(L"Segoe UI Variable",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,34.0f,L"en-us",&g_display);
    g_writeFactory->CreateTextFormat(L"Segoe UI Variable",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,22.0f,L"en-us",&g_title);
    g_writeFactory->CreateTextFormat(L"Segoe UI Variable",nullptr,DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,16.0f,L"en-us",&g_body);
    g_writeFactory->CreateTextFormat(L"Segoe UI Variable",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,11.0f,L"en-us",&g_label);
    g_writeFactory->CreateTextFormat(L"Segoe UI Variable",nullptr,DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,15.0f,L"en-us",&g_button);

    D3D_FEATURE_LEVEL featureLevel{};
    const HRESULT d3dHr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &g_d3dDevice,&featureLevel,&g_d3dContext);
    Log("D3D11CreateDevice: hr=0x%08lX featureLevel=0x%08lX",
        static_cast<unsigned long>(d3dHr),static_cast<unsigned long>(featureLevel));

    const char* shader=R"(
        float4 main() : SV_TARGET {
            return float4(0.16,0.42,0.31,1.0);
        }
    )";
    const HRESULT shaderHr=D3DCompile(shader,strlen(shader),"imux_ui.hlsl",nullptr,nullptr,
        "main","ps_5_0",0,0,&g_pixelShader,nullptr);
    Log("D3DCompile: hr=0x%08lX",static_cast<unsigned long>(shaderHr));
    Log("InitializeGraphics: complete");
}

void SetDrawer(HWND hwnd,bool open) {
    g_drawerOpen=open;
    SetTimer(hwnd,2,10,nullptr);
}

LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(g_inWorld) {
        if(msg==WM_KEYDOWN && wp==VK_ESCAPE) {
            Log("Escape: leaving 3D world");
            imux_world_shutdown();
            g_inWorld=false;
            InitializeGraphics(hwnd);
            SetTimer(hwnd,3,16,nullptr);
            InvalidateRect(hwnd,nullptr,FALSE);
            return 0;
        }
        if(msg==WM_SIZE) { imux_world_wndproc(hwnd,msg,wp,lp); return 0; }
        if(msg==WM_TIMER && wp==3) { imux_world_update(0.016f); imux_world_render(); return 0; }
        if(msg==WM_PAINT) { PAINTSTRUCT ps{}; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); return 0; }
        return DefWindowProc(hwnd,msg,wp,lp);
    }

    switch(msg) {
    case WM_CREATE:
        Log("WM_CREATE received");
        InitializeGraphics(hwnd);
        SetTimer(hwnd,3,16,nullptr);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMinTrackSize.x=880;
        info->ptMinTrackSize.y=600;
        return 0;
    }
    case WM_SIZE:
        if(g_target) {
            const UINT w=static_cast<UINT>(LOWORD(lp));
            const UINT h=static_cast<UINT>(HIWORD(lp));
            if(w>0 && h>0) g_target->Resize(D2D1::SizeU(w,h));
            InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    case WM_MOUSEMOVE: {
        const float x=static_cast<float>(GET_X_LPARAM(lp));
        const float y=static_cast<float>(GET_Y_LPARAM(lp));
        RECT rc{}; GetClientRect(hwnd,&rc);
        const float width=static_cast<float>(rc.right);
        const float height=static_cast<float>(rc.bottom);
        const float edge=width<1000.0f?22.0f:44.0f;
        const float contentW=std::min(1240.0f,std::max(320.0f,width-edge*2.0f));
        const float left=(width-contentW)*.5f;
        const float right=left+contentW;
        const float top=108.0f;
        const float bottom=height-34.0f;
        const bool compact=contentW<920.0f;
        const float playR=compact?right:left+contentW*.68f;
        const float bx=left+28.0f, by=bottom-74.0f;
        const Rect play{bx,by,bx+(compact?220.0f:238.0f),by+52.0f};
        const bool next=g_page==0&&!g_drawerOpen&&Hit(play,x,y);
        if(next!=g_hoverPlay){g_hoverPlay=next;InvalidateRect(hwnd,nullptr,FALSE);}
        return 0;
    }
    case WM_LBUTTONUP: {
        const float x=static_cast<float>(GET_X_LPARAM(lp));
        const float y=static_cast<float>(GET_Y_LPARAM(lp));
        RECT rc{}; GetClientRect(hwnd,&rc);
        const float width=static_cast<float>(rc.right);
        const float height=static_cast<float>(rc.bottom);
        const float edge=width<1000.0f?22.0f:44.0f;
        const float contentW=std::min(1240.0f,std::max(320.0f,width-edge*2.0f));
        const float left=(width-contentW)*.5f;
        const float right=left+contentW;
        const float top=108.0f;
        const float bottom=height-34.0f;
        const Rect menu{20,18,60,58};
        if(Hit(menu,x,y)){SetDrawer(hwnd,!g_drawerOpen);return 0;}
        if(g_drawerOpen){
            const float drawerW=std::min(360.0f,width*.84f);
            if(x<=drawerW){
                for(int i=0;i<4;++i){
                    const Rect item{14,124+i*62,drawerW-14,176+i*62};
                    if(Hit(item,x,y)){g_page=i;SetDrawer(hwnd,false);InvalidateRect(hwnd,nullptr,FALSE);return 0;}
                }
            }
            if(x>drawerW)SetDrawer(hwnd,false);
            return 0;
        }
        const bool compact=contentW<920.0f;
        const float bx=left+28.0f, by=bottom-74.0f;
        const Rect play{bx,by,bx+(compact?220.0f:238.0f),by+52.0f};
        if(g_page==0&&Hit(play,x,y)){
            g_target.Reset();g_brush.Reset();
            Log("Play clicked: starting 3D world");
            g_inWorld=imux_world_run(hwnd)!=0;
            Log("3D world start result: %d",g_inWorld?1:0);
            if(!g_inWorld)InitializeGraphics(hwnd);
        }
        return 0;
    }
    case WM_TIMER:
        if(wp==2){
            const float target=g_drawerOpen?1.0f:0.0f;
            g_drawerProgress+=(target-g_drawerProgress)*0.24f;
            if(std::fabs(target-g_drawerProgress)<0.01f){g_drawerProgress=target;KillTimer(hwnd,2);}
            InvalidateRect(hwnd,nullptr,FALSE);
        } else if(wp==3){
            g_motion+=0.016f;
            if(g_inWorld) imux_world_update(0.016f),imux_world_render();
            else InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};BeginPaint(hwnd,&ps);Render(hwnd);EndPaint(hwnd,&ps);return 0;
    }
    case WM_ERASEBKGND:return 1;
    case WM_DESTROY:
        Log("WM_DESTROY received; inWorld=%d",g_inWorld?1:0);
        if(g_inWorld)imux_world_shutdown();
        KillTimer(hwnd,2);KillTimer(hwnd,3);
        PostQuitMessage(0);return 0;
    }
    return DefWindowProc(hwnd,msg,wp,lp);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    InitializeLogging();
    SetUnhandledExceptionFilter(ImuxUnhandledException);
    std::set_terminate(ImuxTerminate);
    Log("wWinMain entered");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.hInstance=instance;
    wc.lpfnWndProc=WndProc;
    wc.lpszClassName=kWindowClass;
    wc.lpszMenuName=nullptr;
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=nullptr;
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
    wc.hIconSm=LoadIconW(instance,MAKEINTRESOURCEW(101));
    Log("Registering window class");
    if(!RegisterClassExW(&wc)){
        Log("RegisterClassExW failed: error=%lu",GetLastError());
        return 1;
    }
    Log("RegisterClassExW succeeded");
    Log("Creating main window");
    HWND hwnd=CreateWindowExW(0,kWindowClass,kWindowTitle,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,1280,760,nullptr,nullptr,instance,nullptr);
    if(!hwnd){Log("CreateWindowExW failed: error=%lu",GetLastError());return 1;}
    Log("CreateWindowExW succeeded hwnd=%p",hwnd);
    ShowWindow(hwnd,SW_MAXIMIZE);
    Log("ShowWindow complete");
    UpdateWindow(hwnd);
    Log("UpdateWindow complete; entering message loop");
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    Log("Message loop ended: exitCode=%lld",static_cast<long long>(msg.wParam));
    Log("========== Imux launcher end ==========");
    g_log.close();
    return static_cast<int>(msg.wParam);
}
