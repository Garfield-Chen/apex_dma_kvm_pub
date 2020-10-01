#include "overlay.h"

extern int aim;
extern bool esp;
extern bool item_glow;
extern bool ready;
extern bool use_nvidia;
extern int safe_level;
extern int spectators;
extern int allied_spectators;
extern float max_dist;
extern float smooth;
int width;
int height;
bool k_leftclick = false;
bool k_ins = false;
bool show_menu = false;
visuals v;

extern bool IsKeyDown(int vk);

LONG nv_default = WS_POPUP | WS_CLIPSIBLINGS;
LONG nv_default_in_game = nv_default | WS_DISABLED;
LONG nv_edit = nv_default_in_game | WS_VISIBLE;

LONG nv_ex_default = WS_EX_TOOLWINDOW;
LONG nv_ex_edit = nv_ex_default | WS_EX_LAYERED | WS_EX_TRANSPARENT;
LONG nv_ex_edit_menu = nv_ex_default | WS_EX_TRANSPARENT;

static DWORD WINAPI StaticMessageStart(void* Param)
{
	Overlay* ov = (Overlay*)Param;
	ov->CreateOverlay();
	return 0;
}

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
	wchar_t className[255] = L"";
	GetClassName(hwnd, className, 255);
	if (use_nvidia)
	{
		if (wcscmp(L"CEF-OSC-WIDGET", className) == 0) //Nvidia overlay
		{
			Process_Informations* proc = (Process_Informations*)lParam;
			if (GetWindowLong(hwnd, GWL_STYLE) != nv_default && GetWindowLong(hwnd, GWL_STYLE) != nv_default_in_game)
				return TRUE;
			proc->overlayHWND = hwnd;
			return TRUE;
		}
	}
	else
	{
		if (wcscmp(L"overlay", className) == 0) //Custom overlay
		{
			Process_Informations* proc = (Process_Informations*)lParam;
			proc->overlayHWND = hwnd;
			return TRUE;
		}
	}
	return TRUE;
}

// Data
static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();

void Overlay::RenderMenu()
{
	static bool aim_enable = false;
	static bool vis_check = false;
	static bool spec_disable = false;
	static bool all_spec_disable = false;

	if (aim > 0)
	{
		aim_enable = true;
		if (aim > 1)
		{
			vis_check = true;
		}
		else
		{
			vis_check = false;
		}
	}
	else
	{
		aim_enable = false;
		vis_check = false;
	}

	if (safe_level > 0)
	{
		spec_disable = true;
		if (safe_level > 1)
		{
			all_spec_disable = true;
		}
		else
		{
			all_spec_disable = false;
		}
	}
	else
	{
		spec_disable = false;
		all_spec_disable = false;
	}
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(490, 240));
	ImGui::Begin("##title", (bool*)true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
	if (ImGui::BeginTabBar("Tab"))
	{
		if (ImGui::BeginTabItem("Main"))
		{
			ImGui::Checkbox("Disable with enemy spectators", &spec_disable);
			if (spec_disable)
			{
				ImGui::SameLine();
				ImGui::Checkbox("Disable with allied spectators", &all_spec_disable);
				if (all_spec_disable)
				{
					safe_level = 2;
				}
				else
				{
					safe_level = 1;
				}
			}
			else
			{
				safe_level = 0;
			}

			ImGui::Checkbox("ESP", &esp);

			ImGui::Checkbox("AIM", &aim_enable);

			if (aim_enable)
			{
				ImGui::SameLine();
				ImGui::Checkbox("Visibility check", &vis_check);
				if (vis_check)
				{
					aim = 2;
				}
				else
				{
					aim = 1;
				}
			}
			else
			{
				aim = 0;
			}

			ImGui::Checkbox("Glow items", &item_glow);

			ImGui::Text("Max distance:");
			ImGui::SliderFloat("##1", &max_dist, 100.0f * 40, 800.0f * 40, "%.2f");
			ImGui::SameLine();
			ImGui::Text("(%d meters)", (int)(max_dist / 40));

			ImGui::Text("Smooth aim value:");
			ImGui::SliderFloat("##2", &smooth, 12.0f, 150.0f, "%.2f");

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Visuals"))
		{
			ImGui::Text("ESP options:");
			ImGui::Checkbox("Box", &v.box);
			ImGui::Checkbox("Line", &v.line);
			ImGui::Checkbox("Distance", &v.distance);
			ImGui::Checkbox("Health bar", &v.healthbar);
			ImGui::Checkbox("Shield bar", &v.shieldbar);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::Text("Overlay FPS: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();
}

void Overlay::RenderInfo()
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(50, 20));
	ImGui::Begin("##info", (bool*)true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
	switch (safe_level)
	{
	case 0:
		DrawLine(ImVec2(18, 5), ImVec2(35, 5), RED, 2);
		break;
	case 1:
		DrawLine(ImVec2(18, 5), ImVec2(35, 5), ORANGE, 2);
		break;
	case 2:
		DrawLine(ImVec2(18, 5), ImVec2(35, 5), GREEN, 2);
		break;
	default:
		break;
	}
	ImGui::TextColored(RED, "%d", spectators);
	ImGui::SameLine();
	ImGui::Text("-");
	ImGui::SameLine();
	ImGui::TextColored(GREEN, "%d", allied_spectators);
	ImGui::End();
}

void Overlay::ClickThrough(bool v)
{
	if (v)
	{
		nv_edit = nv_default_in_game | WS_VISIBLE;
		SetWindowLong(proc.overlayHWND, GWL_EXSTYLE, nv_ex_edit);
	}
	else
	{
		nv_edit = nv_default | WS_VISIBLE;
		SetWindowLong(proc.overlayHWND, GWL_EXSTYLE, nv_ex_edit_menu);
	}
}

DWORD Overlay::CreateOverlay()
{
	EnumWindows(EnumWindowsCallback, (LPARAM)&proc);
	Sleep(300);
	if (proc.overlayHWND == 0)
	{
		printf("Can't find the overlay\n");
		Sleep(1000);
		exit(0);
	}

	HDC hDC = ::GetWindowDC(NULL);
	width = ::GetDeviceCaps(hDC, HORZRES);
	height = ::GetDeviceCaps(hDC, VERTRES);
		
	running = true;

	// Initialize Direct3D
	if (!CreateDeviceD3D(proc.overlayHWND))
	{
		CleanupDeviceD3D();
		return 1;
	}

	// Show the window
	::ShowWindow(proc.overlayHWND, SW_SHOWDEFAULT);
	::UpdateWindow(proc.overlayHWND);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(proc.overlayHWND);
	ImGui_ImplDX9_Init(g_pd3dDevice);

	ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);

	// Main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	ClickThrough(true);
	while (running)
	{
		HWND wnd = GetWindow(GetForegroundWindow(), GW_HWNDPREV);
		if (use_nvidia)
		{
			if (GetWindowLong(proc.overlayHWND, GWL_STYLE) != nv_edit)
				SetWindowLong(proc.overlayHWND, GWL_STYLE, nv_edit);
			if (GetWindowLong(proc.overlayHWND, GWL_EXSTYLE) != nv_ex_edit)
				SetWindowLong(proc.overlayHWND, GWL_EXSTYLE, nv_ex_edit);
			if (show_menu)
			{
				ClickThrough(false);
			}
			else
			{
				ClickThrough(true);
			}
		}
		if (wnd != proc.overlayHWND)
		{
			SetWindowPos(proc.overlayHWND, wnd, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOSIZE);
			::UpdateWindow(proc.overlayHWND);
		}

		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (IsKeyDown(VK_LBUTTON) && !k_leftclick)
		{
			io.MouseDown[0] = true;
			k_leftclick = true;
		}
		else if (!IsKeyDown(VK_LBUTTON) && k_leftclick)
		{
			io.MouseDown[0] = false;
			k_leftclick = false;
		}

		if (IsKeyDown(VK_INSERT) && !k_ins && ready)
		{
			show_menu = !show_menu;
			if (show_menu)
			{
				ClickThrough(false);
			}
			else
			{
				ClickThrough(true);
			}
			k_ins = true;
		}
		else if (!IsKeyDown(VK_INSERT) && k_ins)
		{	
			k_ins = false;
		}
		
		if(show_menu)
			RenderMenu();
		else
			RenderInfo();

		RenderEsp();
		// Rendering
		ImGui::EndFrame();
		g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * 255.0f), (int)(clear_color.y * 255.0f), (int)(clear_color.z * 255.0f), (int)(clear_color.w * 255.0f));
		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if (g_pd3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			g_pd3dDevice->EndScene();
		}
		HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

		// Handle loss of D3D9 device
		if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
			ResetDevice();

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ClickThrough(true);

	CleanupDeviceD3D();
	::DestroyWindow(proc.overlayHWND);
	return 0;
}

void Overlay::Start()
{
	DWORD ThreadID;
	CreateThread(NULL, 0, StaticMessageStart, (void*)this, 0, &ThreadID);
}

void Overlay::Clear()
{
	running = 0;
	Sleep(50);
	if (use_nvidia)
	{
		SetWindowLong(proc.overlayHWND, GWL_STYLE, nv_default);
		SetWindowLong(proc.overlayHWND, GWL_EXSTYLE, nv_ex_default);
	}
}

int Overlay::getWidth()
{
	return width;
}

int Overlay::getHeight()
{
	return height;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
	if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
		return false;

	// Create the D3DDevice
	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	g_d3dpp.hDeviceWindow = hWnd;
	g_d3dpp.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	g_d3dpp.BackBufferCount = 1;
	g_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
	g_d3dpp.BackBufferWidth = 0;
	g_d3dpp.BackBufferHeight = 0;
	if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
		return false;

	return true;
}

void CleanupDeviceD3D()
{
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
	if (g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; }
}

void ResetDevice()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
	if (hr == D3DERR_INVALIDCALL)
		IM_ASSERT(0);
	ImGui_ImplDX9_CreateDeviceObjects();
}

void Overlay::DrawLine(ImVec2 a, ImVec2 b, ImColor color, float width)
{
	ImGui::GetWindowDrawList()->AddLine(a, b, color, width);
}

void Overlay::DrawBox(ImColor color, float x, float y, float w, float h)
{
	DrawLine(ImVec2(x, y), ImVec2(x + w, y), color, 1.0f);
	DrawLine(ImVec2(x, y), ImVec2(x, y + h), color, 1.0f);
	DrawLine(ImVec2(x + w, y), ImVec2(x + w, y + h), color, 1.0f);
	DrawLine(ImVec2(x, y + h), ImVec2(x + w, y + h), color, 1.0f);
}

void Overlay::Text(ImVec2 pos, ImColor color, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect)
{
	ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), pos, color, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
}

void Overlay::String(ImVec2 pos, ImColor color, const char* text)
{
	Text(pos, color, text, text + strlen(text), 200, 0);
}

void Overlay::RectFilled(float x0, float y0, float x1, float y1, ImColor color, float rounding, int rounding_corners_flags)
{
	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, rounding, rounding_corners_flags);
}

void Overlay::ProgressBar(float x, float y, float w, float h, int value, int v_max)
{
	ImColor barColor = ImColor(
		min(510 * (v_max - value) / 100, 255),
		min(510 * value / 100, 255),
		25,
		255
	);

	RectFilled(x, y, x + w, y + ((h / float(v_max)) * (float)value), barColor, 0.0f, 0);
}