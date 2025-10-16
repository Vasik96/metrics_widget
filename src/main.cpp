#include <iostream>
#include <format>
#include <print>
#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <thread>
#include <chrono>
#include <tchar.h>
#include <functional>

//imgui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "FormattedInfo.h"
#include "font.hpp"
#include <unordered_map>

#define GWLP_HWNDPARENT (-8)

//predefinitions
void DrawOverlay();

bool ShouldShowOverlay() {
	HWND fg = GetForegroundWindow();
	if (!fg) return true;

	// Ignore desktop windows
	HWND progman = FindWindow(L"Progman", nullptr);
	HWND shell = GetShellWindow();
	if (fg == progman || fg == shell) return true;

	// Check if window is minimized or invisible
	if (!IsWindowVisible(fg) || IsIconic(fg))
		return true;

	// Get process name for safety (optional)
	wchar_t title[256];
	GetWindowTextW(fg, title, 256);

	// If no title, likely system or background window
	if (wcslen(title) == 0)
		return true;

	// Otherwise, a normal app window is focused — hide overlay
	return false;
}


// BELOW ALL PROGRAMS, BUT ABOVE DESKTOP ICONS.
// MAKE SURE TO CALL ShowWindow(window, SW_SHOW) or SW_SHOWNA to prevent it from activating (being above everything at first) in a loop as otherwise the window will be below the desktop
// You cannot render below the desktop icons without using a 2D rendering API (GDI, Direct2D, etc...)
bool AttachToDesktop(HWND hWnd)
{


	// Find the Progman window
	HWND progman = FindWindowA("Progman", NULL);
	HWND defView = NULL;

	// Find SHELLDLL_DefView under Progman
	if (progman)
		defView = FindWindowExA(progman, NULL, "SHELLDLL_DefView", NULL);

	// If not found, try under WorkerW windows
	if (!defView)
	{
		HWND desktopHWnd = GetDesktopWindow();
		HWND workerW = NULL;

		do
		{
			workerW = FindWindowExA(desktopHWnd, workerW, "WorkerW", NULL);
			defView = FindWindowExA(workerW, NULL, "SHELLDLL_DefView", NULL);
		} while (!defView && workerW);
	}

	if (!defView)
		return false;

	// Set SHELLDLL_DefView as parent of hWnd
	SetWindowLongPtrA(hWnd, GWLP_HWNDPARENT, (LONG_PTR)defView);

	return true;
}




void allocate_console()
{
	// Allocate a console for this process
	if (AllocConsole())
	{
		// Redirect STDOUT to the new console
		FILE* fp;
		freopen_s(&fp, "CONOUT$", "w", stdout);
		freopen_s(&fp, "CONOUT$", "w", stderr);
		freopen_s(&fp, "CONIN$", "r", stdin);


		std::ios::sync_with_stdio();

		std::cout << "Console allocated successfully!" << std::endl;
	}
}


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param)) {
		return 0L;
	}

	switch (message) {
	case WM_SYSCOMMAND: //this doesnt even work!!!!
		if ((w_param & 0xFFF0) == SC_MINIMIZE) {
			return 0L;
		}
		break;


	case WM_DESTROY:
		PostQuitMessage(0);
		return 0L;
	}


	return DefWindowProc(window, message, w_param, l_param);
}



int screen_width = GetSystemMetrics(SM_CXSCREEN);
int screen_height = GetSystemMetrics(SM_CYSCREEN);

void SetWorkingDirectoryToExeFolder() {
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);

	std::wstring pathStr(exePath);
	size_t lastSlash = pathStr.find_last_of(L"\\/");
	if (lastSlash != std::wstring::npos) {
		std::wstring exeFolder = pathStr.substr(0, lastSlash);
		SetCurrentDirectoryW(exeFolder.c_str());
	}
}

INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show) {
	//allocate_console();

	// this is really REALLY IMPORTANT, if you do any file operations and launch the program from somewhere else, this will make it not break any file operations
	// because a lot of stuff likes to use the working directory which is sometimes where the program got launched from, this method just sets that to the exe's 
	// directory.
	SetWorkingDirectoryToExeFolder();

	WNDCLASSEX wc = { };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = instance;
	wc.lpszClassName = L"DesktopMetricsClass";

	RegisterClassExW(&wc);


	const HWND window = CreateWindowExW(
		WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
		wc.lpszClassName,
		L"DesktopMetrics",
		WS_POPUP,
		0,
		0,
		screen_width,
		screen_height,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	SetLayeredWindowAttributes(window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);
	
	HWND progman = FindWindowA("Progman", nullptr);
	if (!progman)
		return false;

	// Set Progman as the parent of hWnd
	SetWindowLongPtrA(window, GWLP_HWNDPARENT, (LONG_PTR)progman);

	ShowWindow(window, SW_SHOWNA);
	//SetWindowPos(window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	{
		RECT client_area{};
		GetClientRect(window, &client_area);

		RECT window_area{};
		GetClientRect(window, &window_area);

		POINT diff{};
		ClientToScreen(window, &diff);

		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),
			window_area.top + (diff.y - window_area.top),
			client_area.right,
			client_area.bottom
		};

		DwmExtendFrameIntoClientArea(window, &margins);

	}



	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 0U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


	constexpr D3D_FEATURE_LEVEL levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};


	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{};

	//create device & other stuff
	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&level,
		&device_context
	);

	ID3D11Texture2D* back_buffer{ nullptr };


	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer) {
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	}
	else {
		return 1;
	}


	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);

	ImGuiIO& io = ImGui::GetIO();
	ImFontConfig font_cfg;
	font_cfg.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF(
		(void*)font,
		sizeof(font),
		14.0f,
		&font_cfg
	);

	bool windowVisible = true;
	bool running = true;

	static auto programStartTime = std::chrono::steady_clock::now();

	MSG msg;
	while (running) {
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				running = false;
			}
		}

		if (!running) {
			break;
		}

		bool interact_keys_pressed =
			(GetAsyncKeyState(VK_LCONTROL) & 0x8000) &&
			(GetAsyncKeyState(VK_LSHIFT) & 0x8000);

		/*
		bool showOverlay = ShouldShowOverlay();

		if (showOverlay && !windowVisible) {
			ShowWindow(window, SW_SHOW);
			windowVisible = true;
		}
		else if (!showOverlay && windowVisible) {
			ShowWindow(window, SW_HIDE);
			windowVisible = false;
		}*/

		//ShowWindow(window, SW_SHOWNA); // needed otherwise will dissappear from the desktop



		if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) &&
			(GetAsyncKeyState(VK_LSHIFT) & 0x8000) &&
			(GetAsyncKeyState(VK_NEXT) & 0x8000)) // VK_NEXT = Page Down
		{
			running = false;
		}


		static bool wasTransparent = true; // assume starts as transparent

		bool shouldBeTransparent = !interact_keys_pressed;

		if (shouldBeTransparent != wasTransparent) {
			LONG_PTR exStyle = GetWindowLongPtr(window, GWL_EXSTYLE);

			if (shouldBeTransparent) {
				SetWindowLongPtr(window, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
			}
			else {
				SetWindowLongPtr(window, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
			}

			SetWindowPos(window, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

			wasTransparent = shouldBeTransparent;
		}


		// Process ImGui rendering only when the window is visible
		if (windowVisible) {
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();

			ImGui::NewFrame();

			DrawOverlay();

			auto elapsed = std::chrono::steady_clock::now() - programStartTime;
			if (std::chrono::duration<float>(elapsed).count() < 3.0f) {
				ImGui::Text("Drag window -> [LCtrl + LShift]");
				ImGui::Text("Exit program -> [LCtrl + LShift + PgDown]");
			}


			// Rendering here
			ImGui::Render();


			constexpr float color[4]{ 0.f, 0.f, 0.f, 0.f };
			device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
			device_context->ClearRenderTargetView(render_target_view, color);

			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			swap_chain->Present(1U, 0U);
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 64 fps
		}
	}




	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();


	if (swap_chain) swap_chain->Release();

	if (device_context) device_context->Release();

	if (device) device->Release();

	if (render_target_view) render_target_view->Release();


	DestroyWindow(window);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);




	return 0;
}

void Overlay_AddSection(const char* title, float windowWidth)
{
	float text_width = ImGui::CalcTextSize(title).x;

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::SetCursorPosX((windowWidth - text_width) / 2.0f);
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "%s", title);
	ImGui::Separator();
	ImGui::Spacing();
}
/*
void button_interactable(const char* label, const std::function<void()>& onClick) {
	static std::unordered_map<const char*, bool> hovered_last_frame_map;
	static std::unordered_map<const char*, bool> lmb_was_down_map;

	bool& hovered_last_frame = hovered_last_frame_map[label];
	bool& lmb_was_down = lmb_was_down_map[label];

	ImVec2 buttonPos = ImGui::GetCursorScreenPos();

	// Push hovered style if last frame was hovering
	if (hovered_last_frame)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));

	ImGui::Button(label);

	if (hovered_last_frame)
		ImGui::PopStyleColor();

	ImVec2 buttonSize = ImGui::GetItemRectSize();

	// Detect hover manually
	POINT cursorPos;
	GetCursorPos(&cursorPos);

	bool hovered = cursorPos.x >= buttonPos.x && cursorPos.x <= buttonPos.x + buttonSize.x &&
		cursorPos.y >= buttonPos.y && cursorPos.y <= buttonPos.y + buttonSize.y;

	// Detect left mouse button state
	bool lmb_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	// Trigger callback on release while hovering
	if (hovered && !lmb_down && lmb_was_down) {
		onClick();
	}

	hovered_last_frame = hovered;
	lmb_was_down = lmb_down;
}
*/


void DrawOverlay() {
	static bool hovered_last_frame = false;
	static bool lmb_was_down = false;
	ImGuiIO& io = ImGui::GetIO();

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoNavInputs |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoMove;

	ImGuiWindowFlags window_flags2 =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavInputs |
		ImGuiWindowFlags_NoNavFocus;

	bool interact_keys_pressed =
		(GetAsyncKeyState(VK_LCONTROL) & 0x8000) &&
		(GetAsyncKeyState(VK_LSHIFT) & 0x8000);

	ImGuiWindowFlags final_flags = interact_keys_pressed ? window_flags2 : window_flags;

	ImGui::SetNextWindowBgAlpha(0.35f);
	if (ImGui::Begin("##InfoOverlay", nullptr, final_flags))
	{
		const char* title = "Desktop Metrics";
		float windowWidth = ImGui::GetWindowSize().x;
		float textWidth = ImGui::CalcTextSize(title).x;
		ImGui::SetCursorPosX((windowWidth - textWidth) / 2.0f);
		ImGui::TextColored(ImVec4(0, 255, 0, 255), "%s", title);

		ImGui::Spacing();
		ImGui::Separator();

		ImGui::Text("Time: %s", FormattedInfo::GetFormattedTime().c_str());
		ImGui::Text("Date: %s", FormattedInfo::GetFormattedDate().c_str());
		ImGui::Text("CPU: %s", FormattedInfo::GetFormattedCPUUsage().c_str());
		ImGui::Text("RAM: %s", FormattedInfo::GetFormattedRAMUsage().c_str());

		Overlay_AddSection("Menu", windowWidth);

		ImGui::Text("Overlay FPS: %.0f", io.Framerate);


		//ImGui::Spacing();

		/*button_interactable("button", []() {
			//ShellExecuteW(NULL, NULL, L"shutdown.exe", L"/s /t 0", NULL, SW_HIDE);
			MessageBoxA(NULL, "example", "Info", MB_OK);
			});*/


	}
	ImGui::End();
}
