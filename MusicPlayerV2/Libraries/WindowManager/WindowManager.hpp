#pragma once

#include <Windows.h>
#include <string>
#include <unordered_map>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#include "../ImGui/imgui.h"

class WindowManager_t {
private:

    bool CreateRenderTarget();
    bool CreateDeviceD3D(HWND hWnd);

public:
    inline static ID3D11Device* D3D11Device = nullptr;
    inline static ID3D11DeviceContext* D3D11DeviceContext = nullptr;
    inline static IDXGISwapChain* DXGISwapChain = nullptr;
    inline static ID3D11RenderTargetView* D3D11RenderTargetView = nullptr;

    std::unordered_map<std::string, ImFont*> Fonts = {};

    inline static ImVec2 WindowSize = { 0.0f, 0.0f };
    HWND WindowHandle = NULL;
    bool IsRunning = false;
    float CanvasColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool VSync = false;

    bool Init(const std::string& WindowName, const ImVec2 WindowSize);
    
    void Begin();
    void End();

} extern WindowManager;