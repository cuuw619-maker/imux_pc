#define NOMINMAX
#include "imux_3d_engine.h"

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstring>

using DirectX::XMFLOAT3;
using DirectX::XMMATRIX;
using DirectX::XMVECTOR;
using DirectX::XMLoadFloat3;
using DirectX::XMConvertToRadians;
using DirectX::XMMatrixLookToLH;
using DirectX::XMMatrixPerspectiveFovLH;
using DirectX::XMMatrixTranspose;
using DirectX::XMVectorSet;
using Microsoft::WRL::ComPtr;

namespace {

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 color;
};

struct CameraBuffer {
    XMMATRIX viewProjection;
};

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_rtv;
ComPtr<ID3D11DepthStencilView> g_dsv;
ComPtr<ID3D11Buffer> g_vertexBuffer;
ComPtr<ID3D11Buffer> g_cameraBuffer;
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;
ComPtr<ID3D11InputLayout> g_inputLayout;

HWND g_hwnd = nullptr;
bool g_running = false;
bool g_mouseCaptured = false;

XMFLOAT3 g_position{0.0f, 1.8f, 6.0f};
float g_yaw = 0.0f;
float g_pitch = 0.0f;
float g_verticalVelocity = 0.0f;
float g_time = 0.0f;
int g_vertexCount = 0;

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
    const UINT width = std::max(1L, rc.right - rc.left);
    const UINT height = std::max(1L, rc.bottom - rc.top);

    D3D11_TEXTURE2D_DESC depth{};
    depth.Width = width;
    depth.Height = height;
    depth.MipLevels = 1;
    depth.ArraySize = 1;
    depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth.SampleDesc.Count = 1;
    depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTexture;
    if (FAILED(g_device->CreateTexture2D(&depth, nullptr, &depthTexture))) return false;
    return SUCCEEDED(g_device->CreateDepthStencilView(depthTexture.Get(), nullptr, &g_dsv));
}

void AddCube(std::vector<Vertex>& vertices, float cx, float cy, float cz, float sx, float sy, float sz, XMFLOAT3 color) {
    const XMFLOAT3 p[8] = {
        {cx - sx, cy - sy, cz - sz}, {cx + sx, cy - sy, cz - sz},
        {cx + sx, cy + sy, cz - sz}, {cx - sx, cy + sy, cz - sz},
        {cx - sx, cy - sy, cz + sz}, {cx + sx, cy - sy, cz + sz},
        {cx + sx, cy + sy, cz + sz}, {cx - sx, cy + sy, cz + sz}
    };

    static const int faces[6][6] = {
        {0, 1, 2, 0, 2, 3},
        {4, 7, 6, 4, 6, 5},
        {0, 4, 5, 0, 5, 1},
        {3, 2, 6, 3, 6, 7},
        {1, 5, 6, 1, 6, 2},
        {0, 3, 7, 0, 7, 4}
    };

    for (const auto& face : faces) {
        for (int i = 0; i < 6; ++i) {
            XMFLOAT3 c = color;
            if (&face != &faces[0]) c = {color.x * 0.92f, color.y * 0.92f, color.z * 0.92f};
            vertices.push_back({p[face[i]], c});
        }
    }
}

void BuildWorld() {
    std::vector<Vertex> vertices;
    vertices.reserve(36 * 9);

    AddCube(vertices, 0.0f, -0.5f, 0.0f, 14.0f, 0.5f, 14.0f, {0.20f, 0.52f, 0.28f});
    AddCube(vertices, 0.0f, 0.5f, -4.0f, 2.0f, 0.5f, 2.0f, {0.42f, 0.28f, 0.16f});
    AddCube(vertices, 4.0f, 0.5f, -7.0f, 2.0f, 0.5f, 2.0f, {0.38f, 0.32f, 0.20f});
    AddCube(vertices, -5.0f, 1.0f, -8.0f, 1.5f, 1.0f, 1.5f, {0.30f, 0.38f, 0.46f});
    AddCube(vertices, 0.0f, 1.5f, -12.0f, 3.0f, 1.5f, 0.6f, {0.46f, 0.32f, 0.24f});

    D3D11_BUFFER_DESC desc{};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = vertices.data();

    if (FAILED(g_device->CreateBuffer(&desc, &data, &g_vertexBuffer))) {
        g_vertexCount = 0;
        return;
    }

    g_vertexCount = static_cast<int>(vertices.size());
}

bool CompileShader(const char* source, const char* entry, const char* target, ComPtr<ID3DBlob>& blob) {
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    HRESULT hr = D3DCompile(
        source, strlen(source) + 1, "imux_shader", nullptr, nullptr,
        entry, target, flags, 0, &blob, &errors
    );
    return SUCCEEDED(hr);
}

bool Initialize() {
    RECT rc{};
    GetClientRect(g_hwnd, &rc);

    DXGI_SWAP_CHAIN_DESC swap{};
    swap.BufferCount = 1;
    swap.BufferDesc.Width = std::max(1L, rc.right - rc.left);
    swap.BufferDesc.Height = std::max(1L, rc.bottom - rc.top);
    swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap.OutputWindow = g_hwnd;
    swap.SampleDesc.Count = 1;
    swap.Windowed = TRUE;
    swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL level{};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &swap, &g_swapChain, &g_device, &level, &g_context
    );

    if (FAILED(hr)) {
        g_swapChain.Reset();
        g_device.Reset();
        g_context.Reset();

        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION,
            &swap, &g_swapChain, &g_device, &level, &g_context
        );
    }

    if (FAILED(hr)) return false;

    constexpr const char* shader = R"(
cbuffer Camera : register(b0) {
    matrix viewProjection;
};

struct VSIn {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VSOut {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

VSOut VSMain(VSIn input) {
    VSOut output;
    output.position = mul(float4(input.position, 1.0), viewProjection);
    output.color = input.color;
    return output;
}

float4 PSMain(VSOut input) : SV_TARGET {
    return float4(input.color, 1.0);
}
)";

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    if (!CompileShader(shader, "VSMain", "vs_5_0", vsBlob)) return false;
    if (!CompileShader(shader, "PSMain", "ps_5_0", psBlob)) return false;

    if (FAILED(g_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader
    ))) return false;

    if (FAILED(g_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader
    ))) return false;

    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    if (FAILED(g_device->CreateInputLayout(
        layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_inputLayout
    ))) return false;

    D3D11_BUFFER_DESC camera{};
    camera.Usage = D3D11_USAGE_DEFAULT;
    camera.ByteWidth = sizeof(CameraBuffer);
    camera.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(g_device->CreateBuffer(&camera, nullptr, &g_cameraBuffer))) return false;

    BuildWorld();
    if (g_vertexCount == 0) return false;

    return CreateTargets();
}

void CaptureMouse(bool capture) {
    g_mouseCaptured = capture;
    if (capture) {
        SetCapture(g_hwnd);
        ShowCursor(FALSE);
        RECT rc{};
        GetClientRect(g_hwnd, &rc);
        POINT center{
            (rc.right - rc.left) / 2,
            (rc.bottom - rc.top) / 2
        };
        ClientToScreen(g_hwnd, &center);
        SetCursorPos(center.x, center.y);
    } else {
        ReleaseCapture();
        ShowCursor(TRUE);
    }
}

void Update(float dt) {
    const float speed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 8.0f : 4.5f;
    float forwardX = std::sin(g_yaw);
    float forwardZ = -std::cos(g_yaw);
    float rightX = std::cos(g_yaw);
    float rightZ = std::sin(g_yaw);

    float x = 0.0f;
    float z = 0.0f;
    if (GetAsyncKeyState('W') & 0x8000) { x += forwardX; z += forwardZ; }
    if (GetAsyncKeyState('S') & 0x8000) { x -= forwardX; z -= forwardZ; }
    if (GetAsyncKeyState('D') & 0x8000) { x += rightX; z += rightZ; }
    if (GetAsyncKeyState('A') & 0x8000) { x -= rightX; z -= rightZ; }

    const float length = std::sqrt(x * x + z * z);
    if (length > 0.001f) {
        x /= length;
        z /= length;
    }

    g_position.x += x * speed * dt;
    g_position.z += z * speed * dt;

    if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && g_position.y <= 1.81f) {
        g_verticalVelocity = 5.0f;
    }

    g_verticalVelocity -= 12.0f * dt;
    g_position.y += g_verticalVelocity * dt;
    if (g_position.y < 1.8f) {
        g_position.y = 1.8f;
        g_verticalVelocity = 0.0f;
    }

    g_position.x = std::clamp(g_position.x, -12.0f, 12.0f);
    g_position.z = std::clamp(g_position.z, -12.0f, 12.0f);
}

void Render() {
    if (!g_context || !g_rtv || !g_dsv) return;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    const float aspect = rc.bottom > 0 ? static_cast<float>(rc.right) / static_cast<float>(rc.bottom) : 16.0f / 9.0f;

    XMVECTOR eye = XMLoadFloat3(&g_position);
    XMVECTOR direction = XMVectorSet(
        std::sin(g_yaw) * std::cos(g_pitch),
        std::sin(g_pitch),
        -std::cos(g_yaw) * std::cos(g_pitch),
        0.0f
    );
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMMATRIX view = XMMatrixLookToLH(eye, direction, up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(70.0f), aspect, 0.05f, 200.0f
    );

    CameraBuffer camera{};
    camera.viewProjection = XMMatrixTranspose(view * projection);
    g_context->UpdateSubresource(g_cameraBuffer.Get(), 0, nullptr, &camera, 0, 0);

    const float clear[4] = {0.07f, 0.12f, 0.16f, 1.0f};
    g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), g_dsv.Get());
    g_context->ClearRenderTargetView(g_rtv.Get(), clear);
    g_context->ClearDepthStencilView(g_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(rc.right);
    viewport.Height = static_cast<float>(rc.bottom);
    viewport.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &viewport);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    g_context->IASetInputLayout(g_inputLayout.Get());
    g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    g_context->VSSetConstantBuffers(0, 1, g_cameraBuffer.GetAddressOf());
    g_context->PSSetShader(g_pixelShader.Get(), nullptr, 0);
    g_context->Draw(g_vertexCount, 0);

    g_swapChain->Present(1, 0);
}

} 

extern "C" int imux_world_run(HWND owner) {
    if (g_running) return 1;

    g_hwnd = owner;
    g_position = {0.0f, 1.8f, 6.0f};
    g_yaw = 0.0f;
    g_pitch = 0.0f;
    g_verticalVelocity = 0.0f;
    g_time = 0.0f;

    g_running = Initialize();
    if (g_running) CaptureMouse(true);
    return g_running ? 1 : 0;
}

extern "C" int imux_world_is_running(void) {
    return g_running ? 1 : 0;
}

extern "C" void imux_world_update(float delta_seconds) {
    if (!g_running) return;
    g_time += delta_seconds;
    Update(std::clamp(delta_seconds, 0.0f, 0.05f));
}

extern "C" void imux_world_render(void) {
    if (g_running) Render();
}

extern "C" void imux_world_shutdown(void) {
    if (!g_running) return;
    CaptureMouse(false);
    ReleaseTargets();
    g_cameraBuffer.Reset();
    g_vertexBuffer.Reset();
    g_inputLayout.Reset();
    g_vertexShader.Reset();
    g_pixelShader.Reset();
    g_context.Reset();
    g_swapChain.Reset();
    g_device.Reset();
    g_running = false;
    g_hwnd = nullptr;
}

extern "C" LRESULT imux_world_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SIZE && g_running && g_swapChain) {
        const UINT width = LOWORD(lp);
        const UINT height = HIWORD(lp);
        if (width > 0 && height > 0) {
            ReleaseTargets();
            g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            CreateTargets();
        }
        return 1;
    }

    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        CaptureMouse(false);
        return 1;
    }

    if (msg == WM_LBUTTONDOWN) {
        CaptureMouse(true);
        return 1;
    }

    if (msg == WM_MOUSEMOVE && g_mouseCaptured) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        POINT center{(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
        const LONG x = GET_X_LPARAM(lp);
        const LONG y = GET_Y_LPARAM(lp);
        const float dx = static_cast<float>(x - center.x);
        const float dy = static_cast<float>(y - center.y);
        if (std::abs(dx) > 0.01f || std::abs(dy) > 0.01f) {
            g_yaw += dx * 0.0025f;
            g_pitch = std::clamp(g_pitch - dy * 0.0025f, -1.45f, 1.45f);
            POINT screen = center;
            ClientToScreen(hwnd, &screen);
            SetCursorPos(screen.x, screen.y);
        }
        return 1;
    }

    if (msg == WM_KILLFOCUS && g_mouseCaptured) {
        CaptureMouse(false);
        return 0;
    }

    return 0;
}
