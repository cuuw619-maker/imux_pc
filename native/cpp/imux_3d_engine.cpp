#include "imux_3d_engine.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
    XMFLOAT2 uv;
};

struct CameraBuffer {
    XMMATRIX worldViewProjection;
    float time;
    XMFLOAT3 padding;
};

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_rtv;
ComPtr<ID3D11DepthStencilView> g_dsv;
ComPtr<ID3D11Buffer> g_vertexBuffer;
ComPtr<ID3D11Buffer> g_indexBuffer;
ComPtr<ID3D11Buffer> g_constantBuffer;
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;
ComPtr<ID3D11InputLayout> g_inputLayout;
ComPtr<ID3D11ShaderResourceView> g_blockTexture;
ComPtr<ID3D11SamplerState> g_blockSampler;
HWND g_hwnd = nullptr;
bool g_running = false;
bool g_mouseCaptured = false;
float g_time = 0.0f;
float g_yaw = 0.0f;
float g_pitch = 0.0f;
XMFLOAT3 g_position{0.0f, 2.0f, 6.0f};
float g_verticalVelocity = 0.0f;
POINT g_center{};
bool g_ignoreMouse = false;

void ReleaseTargets() {
    g_rtv.Reset();
    g_dsv.Reset();
}

bool CreateTargets() {
    if (!g_swapChain || !g_device) return false;
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    if (FAILED(g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_rtv))) return false;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    D3D11_TEXTURE2D_DESC depth{};
    depth.Width = static_cast<UINT>(rc.right > 0 ? rc.right : 1);
    depth.Height = static_cast<UINT>(rc.bottom > 0 ? rc.bottom : 1);
    depth.MipLevels = 1;
    depth.ArraySize = 1;
    depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth.SampleDesc.Count = 1;
    depth.Usage = D3D11_USAGE_DEFAULT;
    depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depthTexture;
    if (FAILED(g_device->CreateTexture2D(&depth, nullptr, &depthTexture))) return false;
    return SUCCEEDED(g_device->CreateDepthStencilView(depthTexture.Get(), nullptr, &g_dsv));
}

bool LoadTexture(const wchar_t* path, ComPtr<ID3D11ShaderResourceView>& view) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) return false;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);
    if (width == 0 || height == 0) return false;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()))) return false;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(), width * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(g_device->CreateTexture2D(&desc, &data, &texture))) return false;
    return SUCCEEDED(g_device->CreateShaderResourceView(texture.Get(), nullptr, &view));
}

bool CompileShader(const char* source, const char* entry, const char* profile, ComPtr<ID3DBlob>& blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(source, std::strlen(source), "imux_world.hlsl", nullptr, nullptr,
        entry, profile, flags, 0, &blob, &errors);
    return SUCCEEDED(hr);
}

void BuildScene() {
    // Explicit test scene: a flat floor and a small arrangement of blocks.
    // No procedural terrain or world generation is used here.
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto addCube = [&](float cx, float cy, float cz, float sx, float sy, float sz, XMFLOAT4 color) {
        const XMFLOAT3 p[8] = {
            {cx-sx,cy-sy,cz-sz},{cx-sx,cy+sy,cz-sz},{cx+sx,cy+sy,cz-sz},{cx+sx,cy-sy,cz-sz},
            {cx-sx,cy-sy,cz+sz},{cx-sx,cy+sy,cz+sz},{cx+sx,cy+sy,cz+sz},{cx+sx,cy-sy,cz+sz}
        };
        const int faces[6][4] = {{0,1,2,3},{4,7,6,5},{0,4,5,1},{3,2,6,7},{1,5,6,2},{0,3,7,4}};
        const XMFLOAT3 normals[6] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
        for (int f=0; f<6; ++f) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            const XMFLOAT2 uv[4] = {{0,1},{0,0},{1,0},{1,1}};
            for (int v=0; v<4; ++v) vertices.push_back({p[faces[f][v]], normals[f], color, uv[v]});
            indices.insert(indices.end(), {base,base+1,base+2,base,base+2,base+3});
        }
    };

    addCube(0,-0.25f,0,12,0.25f,12,{0.22f,0.28f,0.24f,1});
    addCube(0,0.75f,-4,2,0.75f,2,{0.28f,0.42f,0.34f,1});
    addCube(4,0.75f,-7,2,0.75f,2,{0.34f,0.30f,0.20f,1});
    addCube(-5,1.25f,-8,1.5f,1.25f,1.5f,{0.24f,0.34f,0.44f,1});
    addCube(0,1.75f,-12,3,1.75f,0.6f,{0.38f,0.30f,0.25f,1});

    D3D11_BUFFER_DESC vb{};
    vb.Usage = D3D11_USAGE_DEFAULT;
    vb.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vd{vertices.data(),0,0};
    g_device->CreateBuffer(&vb, &vd, &g_vertexBuffer);

    D3D11_BUFFER_DESC ib{};
    ib.Usage = D3D11_USAGE_DEFAULT;
    ib.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA id{indices.data(),0,0};
    g_device->CreateBuffer(&ib, &id, &g_indexBuffer);

    D3D11_BUFFER_DESC cb{};
    cb.Usage = D3D11_USAGE_DEFAULT;
    cb.ByteWidth = sizeof(CameraBuffer);
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    g_device->CreateBuffer(&cb, nullptr, &g_constantBuffer);

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    g_device->CreateSamplerState(&sampler, &g_blockSampler);

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    wchar_t* slash = wcsrchr(modulePath, L'\\');
    if (slash) *slash = L'\0';
    wchar_t texturePath[MAX_PATH]{};
    swprintf_s(texturePath, L"%s\\assets\\blocks\\dirt.png", modulePath);
    LoadTexture(texturePath, g_blockTexture);
}

bool Initialize() {
    DXGI_SWAP_CHAIN_DESC desc{};
    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    desc.BufferCount = 2;
    desc.BufferDesc.Width = static_cast<UINT>(rc.right > 0 ? rc.right : 1280);
    desc.BufferDesc.Height = static_cast<UINT>(rc.bottom > 0 ? rc.bottom : 720);
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = g_hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        creationFlags, levels, 2, D3D11_SDK_VERSION, &desc, &g_swapChain, &g_device, &level, &g_context);
    if (FAILED(hr)) return false;

    const char* shader = R"(
cbuffer Camera : register(b0) { matrix worldViewProjection; float time; float3 padding; };
struct VSInput { float3 position : POSITION; float3 normal : NORMAL; float4 color : COLOR0; float2 uv : TEXCOORD1; };
struct VSOutput { float4 position : SV_POSITION; float3 normal : NORMAL; float4 color : COLOR0; float3 worldPosition : TEXCOORD0; float2 uv : TEXCOORD1; };
VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), worldViewProjection);
    output.normal = input.normal;
    output.color = input.color;
    output.worldPosition = input.position;
    output.uv = input.uv;
    return output;
}
Texture2D blockTexture : register(t0);
SamplerState blockSampler : register(s0);
float4 PSMain(VSOutput input) {
    float3 lightDirection = normalize(float3(-0.45,0.85,-0.35));
    float diffuse = saturate(dot(normalize(input.normal),lightDirection))*0.65+0.35;
    float pulse = 0.025 * sin(time * 1.7 + input.worldPosition.x * 0.15);
    float3 texel = blockTexture.Sample(blockSampler, input.uv).rgb;
    return float4(texel * input.color.rgb * (diffuse + pulse),1.0);
})";

    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!CompileShader(shader,"VSMain","vs_5_0",vsBlob) || !CompileShader(shader,"PSMain","ps_5_0",psBlob)) return false;
    if (FAILED(g_device->CreateVertexShader(vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),nullptr,&g_vertexShader))) return false;
    if (FAILED(g_device->CreatePixelShader(psBlob->GetBufferPointer(),psBlob->GetBufferSize(),nullptr,&g_pixelShader))) return false;

    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0}
    };
    if (FAILED(g_device->CreateInputLayout(layout,static_cast<UINT>(sizeof(layout) / sizeof(layout[0])),vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),&g_inputLayout))) return false;
    BuildScene();
    return CreateTargets();
}

void CaptureMouse(bool capture) {
    g_mouseCaptured = capture;
    ShowCursor(capture ? FALSE : TRUE);
    if (capture) {
        RECT rc{};
        GetClientRect(g_hwnd,&rc);
        POINT p{(rc.right-rc.left)/2,(rc.bottom-rc.top)/2};
        ClientToScreen(g_hwnd,&p);
        g_center = p;
        SetCursorPos(p.x,p.y);
        RECT clip = rc;
        ClientToScreen(g_hwnd,reinterpret_cast<POINT*>(&clip.left));
        ClientToScreen(g_hwnd,reinterpret_cast<POINT*>(&clip.right));
        ClipCursor(&clip);
        SetFocus(g_hwnd);
    } else {
        ClipCursor(nullptr);
    }
}

void UpdateCamera(float dt) {
    const float speed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 8.0f : 4.5f;
    const float forwardX = std::sin(g_yaw);
    const float forwardZ = -std::cos(g_yaw);
    const float rightX = std::cos(g_yaw);
    const float rightZ = std::sin(g_yaw);
    float moveX = 0, moveZ = 0;
    if (GetAsyncKeyState('W') & 0x8000) { moveX += forwardX; moveZ += forwardZ; }
    if (GetAsyncKeyState('S') & 0x8000) { moveX -= forwardX; moveZ -= forwardZ; }
    if (GetAsyncKeyState('D') & 0x8000) { moveX += rightX; moveZ += rightZ; }
    if (GetAsyncKeyState('A') & 0x8000) { moveX -= rightX; moveZ -= rightZ; }
    float length = std::sqrt(moveX*moveX + moveZ*moveZ);
    if (length > 0.001f) { moveX/=length; moveZ/=length; }
    g_position.x += moveX * speed * dt;
    g_position.z += moveZ * speed * dt;

    if (GetAsyncKeyState(VK_SPACE) & 0x8000 && g_position.y <= 2.001f) g_verticalVelocity = 5.2f;
    g_verticalVelocity -= 12.0f * dt;
    g_position.y += g_verticalVelocity * dt;
    if (g_position.y < 2.0f) { g_position.y = 2.0f; g_verticalVelocity = 0; }

    if (g_mouseCaptured) {
        POINT p{};
        GetCursorPos(&p);
        if (!g_ignoreMouse) {
            const float dx = static_cast<float>(p.x - g_center.x);
            const float dy = static_cast<float>(p.y - g_center.y);
            g_yaw += dx * 0.0025f;
            g_pitch -= dy * 0.0025f;
            g_pitch = std::clamp(g_pitch, -1.45f, 1.45f);
        }
        g_ignoreMouse = false;
        SetCursorPos(g_center.x,g_center.y);
    }
}

void RenderFrame() {
    if (!g_context || !g_rtv || !g_dsv) return;
    RECT rc{}; GetClientRect(g_hwnd,&rc);
    float aspect = (rc.bottom > 0) ? static_cast<float>(rc.right)/static_cast<float>(rc.bottom) : 16.0f/9.0f;
    XMVECTOR eye = XMLoadFloat3(&g_position);
    XMVECTOR direction = XMVectorSet(std::sin(g_yaw)*std::cos(g_pitch),
        std::sin(g_pitch), -std::cos(g_yaw)*std::cos(g_pitch), 0);
    XMVECTOR up = XMVectorSet(0,1,0,0);
    XMMATRIX view = XMMatrixLookToLH(eye,direction,up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(70.0f),aspect,0.05f,100.0f);
    CameraBuffer cb{};
    cb.worldViewProjection = XMMatrixTranspose(view * projection);
    cb.time = g_time;
    g_context->UpdateSubresource(g_constantBuffer.Get(),0,nullptr,&cb,0,0);

    const float clear[4] = {0.045f,0.07f,0.10f,1};
    g_context->OMSetRenderTargets(1,g_rtv.GetAddressOf(),g_dsv.Get());
    g_context->ClearRenderTargetView(g_rtv.Get(),clear);
    g_context->ClearDepthStencilView(g_dsv.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(rc.right);
    viewport.Height = static_cast<float>(rc.bottom);
    viewport.MinDepth = 0; viewport.MaxDepth = 1;
    g_context->RSSetViewports(1,&viewport);

    UINT stride=sizeof(Vertex), offset=0;
    g_context->IASetInputLayout(g_inputLayout.Get());
    g_context->IASetVertexBuffers(0,1,g_vertexBuffer.GetAddressOf(),&stride,&offset);
    g_context->IASetIndexBuffer(g_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->VSSetShader(g_vertexShader.Get(),nullptr,0);
    g_context->VSSetConstantBuffers(0,1,g_constantBuffer.GetAddressOf());
    g_context->PSSetShader(g_pixelShader.Get(),nullptr,0);
    g_context->PSSetShaderResources(0,1,g_blockTexture.GetAddressOf());
    g_context->PSSetSamplers(0,1,g_blockSampler.GetAddressOf());
    g_context->DrawIndexed(180,0,0);
    g_swapChain->Present(1,0);
}

} // namespace

extern "C" int imux_world_run(HWND owner) {
    if (g_running) return 1;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    g_hwnd = owner;
    g_running = Initialize();
    if (g_running) CaptureMouse(true);
    else CoUninitialize();
    return g_running ? 1 : 0;
}

extern "C" int imux_world_is_running(void) { return g_running ? 1 : 0; }

extern "C" void imux_world_update(float delta_seconds) {
    if (!g_running) return;
    g_time += delta_seconds;
    UpdateCamera(delta_seconds);
}

extern "C" void imux_world_render(void) {
    if (g_running) RenderFrame();
}

extern "C" void imux_world_shutdown(void) {
    if (!g_running) return;
    CaptureMouse(false);
    ReleaseTargets();
    g_swapChain.Reset();
    g_constantBuffer.Reset();
    g_indexBuffer.Reset();
    g_vertexBuffer.Reset();
    g_inputLayout.Reset();
    g_vertexShader.Reset();
    g_pixelShader.Reset();
    g_blockTexture.Reset();
    g_blockSampler.Reset();
    g_context.Reset();
    g_device.Reset();
    g_running = false;
    g_hwnd = nullptr;
    CoUninitialize();
}

extern "C" LRESULT imux_world_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (!g_running) return 0;
    if (msg == WM_SIZE && g_swapChain) {
        UINT w=LOWORD(lp), h=HIWORD(lp);
        if (w>0 && h>0) {
            ReleaseTargets();
            g_swapChain->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0);
            CreateTargets();
        }
        return 1;
    }
    if (msg == WM_MOUSEMOVE && g_mouseCaptured) return 1;
    return 0;
}
