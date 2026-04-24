#pragma once

#define STB_IMAGE_IMPLEMENTATION

#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"
#include "dependencies/stb_image.h"
#include "font_poppins.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (msg == WM_DESTROY)
        PostQuitMessage(0);

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

class Renderer
{
public:
    HWND hwnd{ nullptr };
    WNDCLASSEX wc;
    ID3D11Device* pd3dDevice{ nullptr };
    ID3D11DeviceContext* pd3dDeviceContext{ nullptr };
    IDXGISwapChain* pSwapChain{ nullptr };
    ID3D11RenderTargetView* mainRenderTargetView{ nullptr };

    void Init()
    {
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = L"External Overlay";

        RegisterClassEx(&wc);

        hwnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            wc.lpszClassName,
            L"Overlay",
            WS_POPUP,
            0,
            0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN),
            0,
            0,
            wc.hInstance,
            0
        );

        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

        RECT clientArea{};
        RECT windowArea{};

        GetClientRect(hwnd, &clientArea);
        GetWindowRect(hwnd, &windowArea);

        POINT diff;
        ClientToScreen(hwnd, &diff);

        MARGINS margins{
            windowArea.left + (diff.x - windowArea.left),
            windowArea.top + (diff.y - windowArea.top),
            windowArea.right,
            windowArea.bottom
        };

        DwmExtendFrameIntoClientArea(hwnd, &margins);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        CreateDevice(hwnd);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGuiIO& io{ ImGui::GetIO() };
        io.IniFilename = nullptr;

        // Poppins Medium — embedded, no runtime file needed
        ImFontConfig fontCfg;
        fontCfg.FontDataOwnedByAtlas = false;
        fontCfg.OversampleH          = 4;    // sharp horizontal rendering
        fontCfg.OversampleV          = 4;    // sharp vertical rendering
        fontCfg.PixelSnapH           = true; // snap to pixel grid — kills blur
        io.Fonts->AddFontFromMemoryTTF(
            (void*)BBQ_FONT_DATA, (int)BBQ_FONT_SIZE, 14.0f, &fontCfg);


        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(pd3dDevice, pd3dDeviceContext);
    }

    void StartRender()
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void EndRender()
    {
        ImGui::Render();

        const float color[4]{ 0.f, 0.f, 0.f, 0.f };
        pd3dDeviceContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
        pd3dDeviceContext->ClearRenderTargetView(mainRenderTargetView, color);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        pSwapChain->Present(1, 0);
    }

    void Shutdown()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();

        ImGui::DestroyContext();

        CleanupDevice();
        DestroyWindow(hwnd);

        UnregisterClass(wc.lpszClassName, wc.hInstance);
    }

    void CreateDevice(HWND hWnd)
    {
        DXGI_SWAP_CHAIN_DESC swapChainDescription;
        ZeroMemory(&swapChainDescription, sizeof(swapChainDescription));

        swapChainDescription.BufferCount = 4;
        swapChainDescription.BufferDesc.Width = 0;
        swapChainDescription.BufferDesc.Height = 0;
        swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDescription.OutputWindow = hwnd;
        swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDescription.Windowed = 1;
        swapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swapChainDescription.SampleDesc.Count = 1;
        swapChainDescription.SampleDesc.Quality = 0;
        swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

        D3D_FEATURE_LEVEL featureLevel;
        D3D_FEATURE_LEVEL featureLevelList[2]{
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0
        };

        HRESULT result{ D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            featureLevelList,
            2,
            D3D11_SDK_VERSION,
            &swapChainDescription,
            &pSwapChain,
            &pd3dDevice,
            &featureLevel,
            &pd3dDeviceContext
        ) };

        if (result == DXGI_ERROR_UNSUPPORTED)
        {
            result = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                0,
                featureLevelList,
                2,
                D3D11_SDK_VERSION,
                &swapChainDescription,
                &pSwapChain,
                &pd3dDevice,
                &featureLevel,
                &pd3dDeviceContext
            );
        }

        CreateRenderTarget();
    }

    void CleanupDevice()
    {
        CleanupRenderTarget();

        if (pSwapChain)
        {
            pSwapChain->Release();
            pSwapChain = nullptr;
        }

        if (pd3dDeviceContext)
        {
            pd3dDeviceContext->Release();
            pd3dDeviceContext = nullptr;
        }

        if (pd3dDevice)
        {
            pd3dDevice->Release();
            pd3dDevice = nullptr;
        }
    }

    void CreateRenderTarget()
    {
        ID3D11Texture2D* pBackBuffer;
        pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
        pBackBuffer->Release();
    }
        
    void CleanupRenderTarget()
    {
        if (mainRenderTargetView)
        {
            mainRenderTargetView->Release();
            mainRenderTargetView = nullptr;
        }
    }

    ID3D11ShaderResourceView* LoadTexture(const char* filename, int& w, int& h)
    {
        int x, y, channels;
        unsigned char* pixels{ stbi_load(filename, &x, &y, &channels, 4) }; // RGBA
        if (!pixels) return nullptr;

        w = x;
        h = y;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = x;
        desc.Height = y;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub{};
        sub.pSysMem = pixels;
        sub.SysMemPitch = x * 4;

        ID3D11Texture2D* tex = nullptr;
        pd3dDevice->CreateTexture2D(&desc, &sub, &tex);

        stbi_image_free(pixels);

        ID3D11ShaderResourceView* srv{ nullptr };
        pd3dDevice->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();

        return srv;
    }

    ID3D11ShaderResourceView* LoadTextureFromResource(int id, int& w, int& h)
    {
        HRSRC hRes{ FindResource(NULL, MAKEINTRESOURCE(id), RT_RCDATA) };
        if (!hRes) return nullptr;

        HGLOBAL hData{ LoadResource(NULL, hRes) };
        if (!hData) return nullptr;

        void* pData{ LockResource(hData) };
        DWORD size{ SizeofResource(NULL, hRes) };

        int x, y, channels;
        unsigned char* pixels{ stbi_load_from_memory((unsigned char*)pData, size, &x, &y, &channels, 4) };
        if (!pixels) return nullptr;

        w = x;
        h = y;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = x;
        desc.Height = y;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub{};
        sub.pSysMem = pixels;
        sub.SysMemPitch = x * 4;

        ID3D11Texture2D* tex = nullptr;
        pd3dDevice->CreateTexture2D(&desc, &sub, &tex);

        stbi_image_free(pixels);

        ID3D11ShaderResourceView* srv{ nullptr };
        pd3dDevice->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();

        return srv;
    }
};