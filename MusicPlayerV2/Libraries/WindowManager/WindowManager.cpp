#define WIN32_LEAN_AND_MEAN
#include "WindowManager.hpp"
#include "../ImGui/imgui_impl_dx11.h"
#include "../ImGui/imgui_impl_win32.h"

#include <iostream>
#include <dwmapi.h>
#include <filesystem>

WindowManager_t WindowManager;

bool WindowManager_t::CreateRenderTarget() {
	ID3D11Texture2D* pBackBuffer;
	DXGISwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	D3D11Device->CreateRenderTargetView(pBackBuffer, NULL, &D3D11RenderTargetView);
	pBackBuffer->Release();
	return true;
}

bool WindowManager_t::CreateDeviceD3D(HWND hWnd) {
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 1;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT CreateDeviceFlags = 0;
	D3D_FEATURE_LEVEL FeatureLevel;
	const D3D_FEATURE_LEVEL FeatureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT Result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, CreateDeviceFlags, FeatureLevelArray, 2, D3D11_SDK_VERSION, &sd, &DXGISwapChain, &D3D11Device, &FeatureLevel, &D3D11DeviceContext);
	if (Result == DXGI_ERROR_UNSUPPORTED)
		Result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, CreateDeviceFlags, FeatureLevelArray, 2, D3D11_SDK_VERSION, &sd, &DXGISwapChain, &D3D11Device, &FeatureLevel, &D3D11DeviceContext);
	if (Result != S_OK)
		return false;

	this->CreateRenderTarget();
	return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
		return true;
	
	static RECT BorderThickness = { 0 };
	static int TitelbarY = 1;

	switch (Msg) {

		case WM_NCHITTEST: {
		
			LRESULT Result = DefWindowProc(hWnd, Msg, wParam, lParam);
		
			RECT WindowRect = { 0, 0, 0, 0 };
			GetWindowRect(hWnd, &WindowRect);
		
			POINT MousePos = { 0, 0 };
			GetPhysicalCursorPos(&MousePos);
		
			int WindowHeight = WindowRect.bottom - WindowRect.top;
			static int TitleBarHeight = 30;
			if (MousePos.y - WindowRect.top > 0 && MousePos.y - WindowRect.top < TitleBarHeight && WindowRect.right - MousePos.x > 60)
				return HTCAPTION;
		
			if (Result == 11 || Result == 15 || Result == 12 || Result == 10) {
				return FALSE;
			}
		
			return Result;
		}
		
		case WM_NCCALCSIZE: {
		
			if (wParam) {
				RECT& r = reinterpret_cast<LPNCCALCSIZE_PARAMS>(lParam)->rgrc[0];
				r.left += BorderThickness.left;
				r.right -= BorderThickness.right;
				r.bottom -= BorderThickness.bottom;
				return FALSE;
			}
			break;
		}
		
		case WM_CREATE: {
		
			if (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_THICKFRAME) {
				AdjustWindowRectEx(&BorderThickness,
					GetWindowLongPtr(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
				BorderThickness.left *= -1;
				BorderThickness.top *= -1;
			} else if (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_BORDER) {
				BorderThickness = { 1,1,1,1 };
			}
		
			MARGINS margins = { 0, 0, TitelbarY, 0 };
			DwmExtendFrameIntoClientArea(hWnd, &margins);
			SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
			return 0;
		}

		case WM_SIZE: {

			if (!WindowManager_t::DXGISwapChain)
				return 0;

			if (WindowManager_t::D3D11RenderTargetView) { WindowManager_t::D3D11RenderTargetView->Release(); WindowManager_t::D3D11RenderTargetView = nullptr; };

			WindowManager_t::DXGISwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			

			ID3D11Texture2D* pBackBuffer;
			WindowManager_t::DXGISwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
			WindowManager_t::D3D11Device->CreateRenderTargetView(pBackBuffer, NULL, &WindowManager_t::D3D11RenderTargetView);
			pBackBuffer->Release();
			return true;
		}

		case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_KEYMENU) // Disable ALT application menu
				return 0;
			break;
		case WM_DESTROY:
			::PostQuitMessage(0);
			return 0;
		}
	return ::DefWindowProcW(hWnd, Msg, wParam, lParam);
}

bool WindowManager_t::Init(const std::string& WindowName, const ImVec2 WindowSize) {

	this->WindowSize = WindowSize;

	// Center the window
	ImVec2 WindowPos = { 0.0f, 0.0f };

	const int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	const int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	WindowPos.x = ScreenWidth / 2 - WindowSize.x / 2;
	WindowPos.y = ScreenHeight / 2 - WindowSize.y / 2;

	std::wstring WWindowName(WindowName.begin(), WindowName.end());

	const WNDCLASSEXW WindowClass = { 
		sizeof(WindowClass), 
		CS_CLASSDC, 
		WndProc, 
		0L, 
		0L, 
		GetModuleHandle(nullptr), 
		nullptr, 
		nullptr, 
		nullptr, 
		nullptr, 
		WWindowName.c_str(),
		nullptr
	};
	if (!RegisterClassExW(&WindowClass)) {
		return false;
	}

	this->WindowHandle = CreateWindowW(
		WWindowName.c_str(), 
		WWindowName.c_str(),
		WS_POPUP, 
		static_cast<int>(WindowPos.x),
		static_cast<int>(WindowPos.y),
		static_cast<int>(WindowSize.x), 
		static_cast<int>(WindowSize.y), 
		NULL, 
		NULL, 
		WindowClass.hInstance, 
		NULL
	);

	if (this->WindowHandle == INVALID_HANDLE_VALUE || this->WindowHandle == NULL) {
		return false;
	}

	if (!this->CreateDeviceD3D(this->WindowHandle)) {
		return false;
	}

	SetWindowLong(this->WindowHandle, GWL_EXSTYLE, GetWindowLong(this->WindowHandle, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(this->WindowHandle, RGB(0, 0, 0), 0, LWA_COLORKEY);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO* io = &ImGui::GetIO();
	
	std::string CurDir = std::filesystem::current_path().string() + "\\Fonts\\";
	if (std::filesystem::exists(CurDir)) {
		for (const auto& Entry : std::filesystem::directory_iterator(CurDir)) {
			const std::string& Name = Entry.path().filename().string();
			if (Name.find("Bold") != std::string::npos) {
				Fonts["SanFranciscoBold"] = io->Fonts->AddFontFromFileTTF(Entry.path().string().c_str(), 24.0f);
			}

			if (Name.find("Medium") != std::string::npos) {
				Fonts["SanFranciscoMedium"] = io->Fonts->AddFontFromFileTTF(Entry.path().string().c_str(), 18.0f);
			}
		}
	}

	if (!ImGui_ImplWin32_Init(this->WindowHandle)) {
		this->IsRunning = false;
		return false;
	}
	if (!ImGui_ImplDX11_Init(this->D3D11Device, this->D3D11DeviceContext)) {
		this->IsRunning = false;
		return false;
	}

	// Show window
	ShowWindow(this->WindowHandle, SW_SHOWDEFAULT);
	UpdateWindow(this->WindowHandle);

	this->IsRunning = true;

	return true;
}

void WindowManager_t::Begin() {
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	
	this->D3D11DeviceContext->ClearRenderTargetView(this->D3D11RenderTargetView, CanvasColor);
}

void WindowManager_t::End() {
	ImGui::Render();

	ID3D11RenderTargetView* RenderTargetView = this->D3D11RenderTargetView;
	this->D3D11DeviceContext->OMSetRenderTargets(1, &this->D3D11RenderTargetView, nullptr);

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	this->DXGISwapChain->Present(this->VSync, 0);

	MSG Message;
	while (::PeekMessage(&Message, nullptr, 0U, 0U, PM_REMOVE)) {
		switch (Message.message) {
			case WM_QUIT || !this->IsRunning: {
				this->D3D11Device->Release();
				this->D3D11DeviceContext->Release();
				this->D3D11RenderTargetView->Release();
				this->IsRunning = false;
				exit(0);
				break;
			}
		}

		::TranslateMessage(&Message);
		::DispatchMessage(&Message);
	}
}