#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <cstdlib>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 3;

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { CoUninitialize(); return 4; }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(argv[1], nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { CoUninitialize(); return 5; }

    ComPtr<IWICBitmapFrameDecode> source;
    hr = decoder->GetFrame(0, &source);
    if (FAILED(hr)) { CoUninitialize(); return 6; }

    ComPtr<IWICBitmapFrameDecode> source32;
    hr = source.As(&source32);
    if (FAILED(hr)) { CoUninitialize(); return 7; }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatIco, nullptr, &encoder);
    if (FAILED(hr)) { CoUninitialize(); return 8; }

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) { CoUninitialize(); return 9; }
    hr = stream->InitializeFromFilename(argv[2], GENERIC_WRITE);
    if (FAILED(hr)) { CoUninitialize(); return 10; }
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) { CoUninitialize(); return 11; }

    const std::array<UINT, 6> sizes{256, 128, 64, 48, 32, 16};
    for (UINT size : sizes) {
        ComPtr<IWICBitmapScaler> scaler;
        hr = factory->CreateBitmapScaler(&scaler);
        if (FAILED(hr)) { CoUninitialize(); return 12; }
        hr = scaler->Initialize(source.Get(), size, size, WICBitmapInterpolationModeFant);
        if (FAILED(hr)) { CoUninitialize(); return 13; }

        ComPtr<IWICBitmapFrameEncode> frame;
        hr = encoder->CreateNewFrame(&frame, nullptr);
        if (FAILED(hr)) { CoUninitialize(); return 14; }
        hr = frame->Initialize(nullptr);
        if (FAILED(hr)) { CoUninitialize(); return 15; }
        hr = frame->SetSize(size, size);
        if (FAILED(hr)) { CoUninitialize(); return 16; }

        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        hr = frame->SetPixelFormat(&format);
        if (FAILED(hr)) { CoUninitialize(); return 17; }
        hr = frame->WriteSource(scaler.Get(), nullptr);
        if (FAILED(hr)) { CoUninitialize(); return 18; }
        hr = frame->Commit();
        if (FAILED(hr)) { CoUninitialize(); return 19; }
    }

    hr = encoder->Commit();
    stream->Commit(STGC_DEFAULT);
    CoUninitialize();
    return SUCCEEDED(hr) ? 0 : 20;
}
