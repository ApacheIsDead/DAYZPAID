#include "Overlay.h"
#include <dwmapi.h>
#include <iostream>


ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

void CleanupRenderTarget();
void CleanupDeviceD3D();
void CreateRenderTarget();
bool CreateDeviceD3D(HWND hWnd);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
Overlay ov;
Globals g;

bool Overlay::CreateOverlay()
{
    wc = { sizeof(WNDCLASSEXA), 0, WndProc, 0, 0, NULL, NULL, NULL, NULL, TitleName, ClassName, NULL };
    RegisterClassExA(&wc);
    Hwnd = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST, wc.lpszClassName, wc.lpszMenuName, WS_POPUP | WS_VISIBLE, 100, 100, 100, 100, NULL, NULL, wc.hInstance, NULL);
    SetLayeredWindowAttributes(Hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margin = { -1 };
    DwmExtendFrameIntoClientArea(Hwnd, &margin);

    if (!CreateDeviceD3D(Hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        exit(0);
    }

    ShowWindow(Hwnd, SW_SHOWDEFAULT);
    UpdateWindow(Hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.LogFilename = nullptr;
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(Hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    SetWindowPos(Hwnd, nullptr, 0, 0, 1920, 1080, SWP_NOREDRAW);
}

void Overlay::DestroyOverlay()
{
    std::cout << "Shutting down overlay...\n";

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(Hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    std::cout << "Overlay closed!\n";
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

bool CreateDeviceD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;


	switch (msg)
	{
	case WM_SIZE:
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

DirectX::SimpleMath::Vector3 Overlay::WorldToScreen(const DirectX::SimpleMath::Vector3 position)
{
    DirectX::SimpleMath::Vector3 temp = position - InvertedViewTranslation;

    float x = temp.Dot(InvertedViewRight);
    float y = temp.Dot(InvertedViewUp);
    float z = temp.Dot(InvertedViewForward);

    if (z < 0.01f)
        return DirectX::SimpleMath::Vector3(0.f, 0.f, 0.f);

    DirectX::SimpleMath::Vector3 res(
        ViewportSize.x * (1 + (x / ProjectionD1.x / z)),
        ViewportSize.y * (1 - (y / ProjectionD2.y / z)),
        z);

    return res;
}

static int quadrant = -1; // -1 for full map view
void Overlay::RenderEsp(std::vector<SHARED_DATA> entityList) {
    INT32 x = 0;

    // Convert camera data from long to float
    float InvertedViewTranslationX = static_cast<float>(ov.InvertedViewTranslationX);
    float InvertedViewTranslationY = static_cast<float>(ov.InvertedViewTranslationY);
    float InvertedViewTranslationZ = *(float*)&ov.InvertedViewTranslationZ;
    float InvertedViewRightX = *(float*)&ov.InvertedViewRightX;
    float InvertedViewRightY = *(float*)&ov.InvertedViewRightY;
    float InvertedViewRightZ = *(float*)&ov.InvertedViewRightZ;
    float InvertedViewUpX = *(float*)&ov.InvertedViewUpX;
    float InvertedViewUpY = *(float*)&ov.InvertedViewUpY;
    float InvertedViewUpZ = *(float*)&ov.InvertedViewUpZ;
    float InvertedViewForwardX = *(float*)&ov.InvertedViewForwardX;
    float InvertedViewForwardY = *(float*)&ov.InvertedViewForwardY;
    float InvertedViewForwardZ = *(float*)&ov.InvertedViewForwardZ;
    float viewPortSizeX = *(float*)&ov.viewPortSizeX;
    float viewPortSizeY = *(float*)&ov.viewPortSizeY;
    float viewPortSizeZ = *(float*)&ov.viewPortSizeZ;
    float projectionD1X = *(float*)&ov.projectionD1X;
    float projectionD1Y = *(float*)&ov.projectionD1Y;
    float projectionD1Z = *(float*)&ov.projectionD1Z;
    float projectionD2X = *(float*)&ov.projectionD2X;
    float projectionD2Y = *(float*)&ov.projectionD2Y;
    float projectionD2Z = *(float*)&ov.projectionD2Z;


    InvertedViewTranslation = DirectX::SimpleMath::Vector3(InvertedViewTranslationX, InvertedViewTranslationY, InvertedViewTranslationZ);
    InvertedViewRight = DirectX::SimpleMath::Vector3(InvertedViewRightX, InvertedViewRightY, InvertedViewRightZ);
    InvertedViewUp = DirectX::SimpleMath::Vector3(InvertedViewUpX, InvertedViewUpY, InvertedViewUpZ);
    InvertedViewForward = DirectX::SimpleMath::Vector3(InvertedViewForwardX, InvertedViewForwardY, InvertedViewForwardZ);
    ViewportSize = DirectX::SimpleMath::Vector3(viewPortSizeX, viewPortSizeY, viewPortSizeZ);
    ProjectionD1 = DirectX::SimpleMath::Vector3(projectionD1X, projectionD1Y, projectionD1Z);
    ProjectionD2 = DirectX::SimpleMath::Vector3(projectionD2X, projectionD2Y, projectionD2Z);

    for (const auto& entity2 : entityList) {
        x += 1;
        float entityx = *(float*)&entity2.x;
        float entityy = *(float*)&entity2.y;
        float entityz = *(float*)&entity2.z;
        
        // calculate the screen positions for each of the player's bones
        DirectX::SimpleMath::Vector3 screenPosition = WorldToScreen(DirectX::SimpleMath::Vector3(entityx, entityy, entityz));
        ImColor pColor = ImColor(0.f, 0.f, 255.f, 255.f);
        std::string toWrite = "Player";
        ImGui::GetWindowDrawList()->AddText(ImVec2(screenPosition.x, screenPosition.y), pColor, toWrite.c_str());

        
    }
    DirectX::SimpleMath::Vector3 testPos(1000.0f, 1000.0f, 1000.0f);
    DirectX::SimpleMath::Vector3 screenPos = WorldToScreen(testPos);
    ImGui::GetWindowDrawList()->AddText(ImVec2(screenPos.x, screenPos.y), ImColor(255, 0, 0), "Test Entity");
    
}

void Overlay::RenderEntityDistances(std::vector<SHARED_DATA> entityList) {
    ULONG64 localPlayerPtr = 0;
    float localX = 0.0f, localY = 0.0f, localZ = 0.0f;

    // First pass: Identify local player
    for (const auto& entity : entityList) {
        if ((ULONG64)entity.entityPtr == (ULONG64)entity.localPlayerPtr) {
            localPlayerPtr = (ULONG64)entity.entityPtr;
            localX = *(float*)&entity.x;
            localY = *(float*)&entity.y;
            localZ = *(float*)&entity.z;
            break;
        }
    }

    if (localPlayerPtr == 0) return; // No local player found, abort

    // Second pass: Calculate distances
    for (const auto& entity : entityList) {
        float entityX = *(float*)&entity.x;
        float entityY = *(float*)&entity.y;
        float entityZ = *(float*)&entity.z;

        if ((ULONG64)entity.entityPtr != localPlayerPtr) {
            float deltaX = entityX - localX;
            float deltaY = entityY - localY;
            float deltaZ = entityZ - localZ;

            float distance = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

            // Log or render the distance
            ImGui::Text("Distance to entity 0x%llx: %.2f\n", (ULONG64)entity.entityPtr, distance);
        }
    }
}

void Overlay::RenderRadar(std::vector<SHARED_DATA> entityList) {
    const float radarWidth = 600.0f;
    const float radarHeight = 600.0f;
    const float mapWidth = 15360.0f;
    const float mapHeight = 15360.0f;

    if (ImGui::IsMouseClicked(0)) {
        quadrant = (quadrant + 1) % 4;
    }
    if (ImGui::IsMouseClicked(1)) {
        quadrant = -1; // Reset to full map view
    }

    float startX = 0.0f, startY = 0.0f;
    float viewWidth = mapWidth, viewHeight = mapHeight;

    if (quadrant != -1) {
        startX = (quadrant % 2) * (mapWidth / 2);
        startY = (quadrant / 2) * (mapHeight / 2);
        viewWidth = mapWidth / 2;
        viewHeight = mapHeight / 2;
    }

    RectFilled(10, 10, 10 + radarWidth, 10 + radarHeight, ImColor(0.f, 0.f, 0.f, 0.725f), 0.f, 0);

    int x = 0;
    for (const auto& entity : entityList) {
        x += 1;
        float entityx = *(float*)&entity.x;
        float entityy = *(float*)&entity.z;

        // Subtract 15360.0f from entityy (Z coordinate)
        entityy = 15360.0f - entityy;

        if (entityx >= startX && entityx < startX + viewWidth && entityy >= startY && entityy < startY + viewHeight) {
            // Scale X normally
            float scaledX = ((entityx - startX) / viewWidth) * radarWidth;

            // Flip Y correctly
            float scaledY = ((entityy - startY) / viewHeight) * radarHeight;

            // Check if entityPtr is = localPlayerPtr, if so draw this circle as red
            if ((BYTE)entity.entityPtr == (BYTE)entity.localPlayerPtr) {
                Circle(ImVec2(10 + scaledX, 10 + scaledY), 5.0f, ImColor(255.f, 0.f, 0.f, 255.f));
            }
            Circle(ImVec2(10 + scaledX, 10 + scaledY), 3.4f, ImColor(0.f, 0.f, 255.f, 255.f));
        }
    }

    ImGui::Text("Entity Count: %i", x);

    Circle(ImVec2(10 + radarWidth / 2, 10 + radarHeight / 2), 5.0f, ImColor(255.f, 0.f, 0.f, 255.f));

    std::vector<std::pair<std::pair<float, float>, const char*>> cities = {
        {{6900, 2830}, "Chernogorsk"},
        {{10157, 2100}, "Elektro"},
        {{4400, 2500}, "Balota"},
        {{4500, 10100}, "Nwaf"},
        {{3727, 3230}, "Komarovo"},
        {{3070, 3725}, "Kamenka"},
        {{7425, 14360}, "Severograd"},
        {{11000, 12300}, "Krasnostav"},
        {{9770, 8940}, "Novy Sobor"},
        {{9500, 8850}, "Gorka"}
    };

    for (const auto& city : cities) {
        float cityX = city.first.first;
        float cityY = city.first.second;

        if (cityX >= startX && cityX < startX + viewWidth && cityY >= startY && cityY < startY + viewHeight) {
            float scaledCityX = ((cityX - startX) / viewWidth) * radarWidth;
            float scaledCityY = (1.0f - ((cityY - startY) / viewHeight)) * radarHeight;
            ImVec2 cityPos(10 + scaledCityX, 10 + scaledCityY);
            ImGui::GetWindowDrawList()->AddText(cityPos, ImColor(255.f, 255.f, 255.f, 255.f), city.second);
        }
    }
}


void Overlay::RenderMenu()
{
    // Setup
    static int Index = 0;

    ImGui::SetNextWindowBgAlpha(0.975f);
    ImGui::SetNextWindowSize(ImVec2(725.f, 450.f));
    ImGui::Begin("DayZ", &g.showMenu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::SameLine();

    //---// Child 1 //-----------------------------------//
    ImGui::BeginChild("##ContextChild", ImVec2(ImGui::GetContentRegionAvail()), false);

    ImGui::Spacing();

    switch (Index)
    {
    case 0: // Visual
        ImGui::Text("Visual");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("ESP", &g.g_ESP);
        ImGui::NewLine();
        ImGui::Spacing();
        ImGui::Checkbox("Radar", &g.g_ESP_Radar);
        ImGui::NewLine();
        ImGui::Spacing();
        ImGui::Checkbox("Distance ESP", &g.g_ESP_Distance);
        break;
    case 1:
        ImGui::Text("Visual");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Checkbox("Item ESP", &g.g_ESP_Item);
        ImGui::Checkbox("Corpse ESP", &g.g_ESP_Corpse);
        ImGui::NewLine();
        ImGui::Spacing();
        break;
    default:
        break;
    }

    ImGui::EndChild();
    //---------------------------------------------------//

    ImGui::End();
}
