#pragma once
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

class Overlay
{
public:
	bool CreateOverlay();
	void OverlayLoop();
	void DestroyOverlay();
	void RenderMenu();
private:

	// Overlay
	WNDCLASSEXA wc;
	HWND Hwnd;
	char ClassName[16] = "NULL";
	char TitleName[16] = "Overlay";

	void DrawLine(ImVec2 a, ImVec2 b, ImColor color, float width)
	{
		ImGui::GetWindowDrawList()->AddLine(a, b, color, width);
	}
	void RectFilled(float x0, float y0, float x1, float y1, ImColor color, float rounding, int rounding_corners_flags)
	{
		ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, rounding, rounding_corners_flags);
	}
	void HealthBar(float x, float y, float w, float h, int value, int v_max)
	{
		RectFilled(x, y, x + w, y + h, ImColor(0.f, 0.f, 0.f, 0.725f), 0.f, 0);
		RectFilled(x, y, x + w, y + ((h / float(v_max)) * (float)value), ImColor(min(510 * (v_max - value) / 100, 255), min(510 * value / 100, 255), 25, 255), 0.0f, 0);
	}
	void Circle(ImVec2 pos, float fov_size, ImColor color)
	{
		ImGui::GetWindowDrawList()->AddCircle(pos, fov_size, color, 100, 0);
	}
	void String(ImVec2 pos, ImColor color, const char* text)
	{
		ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), pos, color, text, text + strlen(text), 1024, 0);
	}
	void StringEx(ImVec2 pos, ImColor color, float font_size, const char* text)
	{
		ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), font_size, ImVec2(pos.x + 1.f, pos.y + 1.f), ImColor(0.f, 0.f, 0.f, 1.f), text, text + strlen(text), 1024, 0);
		ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), font_size, pos, color, text, text + strlen(text), 1024, 0);
	}
};

struct Globals
{
	bool showMenu = false;
	int menuKey = VK_INSERT;

	// Visual
	bool g_ESP = true;
	bool g_ESP_Corpse = true;

	// Item ESP
	bool g_ESP_Item = true;

};

extern Globals g;

extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

extern Overlay ov;
