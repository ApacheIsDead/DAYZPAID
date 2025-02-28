#include <windows.h>
#include <vector>
#include <thread>
#include <chrono>

struct COORDS {
    LONG x;
    LONG y;
    LONG z;
};

// Configuration
const int overlaySize = 400;
const float worldRadius = 8000.0f;

struct City {
    const char* name;
    float x;
    float y;
};

struct Entity {
    COORDS position;
    bool isPlayer;
};

std::vector<Entity> entities;
CRITICAL_SECTION dataLock;
HWND hOverlayWnd = nullptr;

const std::vector<City> cities = {
    {"Elektro", 10400, 2400},
    {"Cherno", 6703, 2659},
    {"Berezino", 12700, 3600},
    {"NWAF", 4600, 10300},
    {"Novo", 11500, 7800},
    {"Zeleno", 2600, 5300},
    {"Staroye", 6150, 7700},
    {"Gorka", 9400, 6300},
    {"Kamyshovo", 12200, 3500},
    {"Svetlo", 13700, 3900}
};

// Sample coordinates (Replace with actual data retrieval logic)
COORDS sampleCoords[] = {
    {10400, 2400, 0},
    {6703, 2659, 0},
    {12700, 3600, 0}
};

void UpdateEntities() {
    while (true) {
        EnterCriticalSection(&dataLock);
        entities.clear();

        for (const auto& coord : sampleCoords) {
            entities.push_back({ coord, true });
        }

        LeaveCriticalSection(&dataLock);
        InvalidateRect(hOverlayWnd, nullptr, TRUE);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

POINT WorldToScreen(float worldX, float worldY) {
    POINT screenPos;
    const float scale = overlaySize / (2 * worldRadius);
    float relX = worldX - 6000.0f;
    float relY = 6000.0f - worldY;
    screenPos.x = static_cast<LONG>(relX * scale);
    screenPos.y = static_cast<LONG>(relY * scale);
    return screenPos;
}
void DrawOverlay(HDC hdc) {
    HBRUSH hBlueBrush = CreateSolidBrush(RGB(0, 0, 255));
    HBRUSH hWhiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    EnterCriticalSection(&dataLock);
    for (const Entity& ent : entities) {
        POINT screenPos = WorldToScreen(ent.position.x, ent.position.y);
        int x = overlaySize / 2 + screenPos.x;
        int y = overlaySize / 2 + screenPos.y;
        SelectObject(hdc, hBlueBrush);
        Ellipse(hdc, x - 5, y - 5, x + 5, y + 5);
    }
    LeaveCriticalSection(&dataLock);

    // Draw city labels with scaling
    SetTextColor(hdc, RGB(200, 230, 255));
    SetBkMode(hdc, TRANSPARENT);

    // Determine the scale factor for font size based on overlay size
    float fontScale = overlaySize / worldRadius / 25.0f; // Adjust as needed for optimal size

    for (const City& city : cities) {
        POINT screenPos = WorldToScreen(city.x, city.y);
        int x = overlaySize / 2 + screenPos.x;
        int y = overlaySize / 2 + screenPos.y;

        // Create a scaled font based on the scale factor
        int fontSize = static_cast<int>(12 * fontScale);  // Adjust the base size (12) as needed
        HFONT hFont = CreateFont(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Arial");
        SelectObject(hdc, hFont);

        // Draw city position circle
        SelectObject(hdc, hWhiteBrush);
        Ellipse(hdc, x - 2, y - 2, x + 2, y + 2);

        // Draw the city name with adjusted positioning
        TextOutA(hdc, x + 5, y - 8, city.name, (int)strlen(city.name));

        // Clean up the font object after drawing
        DeleteObject(hFont);
    }

    DeleteObject(hBlueBrush);
    DeleteObject(hWhiteBrush);
}


DWORD WINAPI EntityUpdater(LPVOID lpParam) {
    UpdateEntities();
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawOverlay(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    if (message == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void CreateOverlay() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), 0, WndProc, 0, 0, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"MiniMapOverlay", nullptr };
    RegisterClassEx(&wc);
    hOverlayWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE, L"MiniMapOverlay", L"MiniMapOverlay", WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, overlaySize, overlaySize, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    SetLayeredWindowAttributes(hOverlayWnd, RGB(0, 0, 0), 200, LWA_ALPHA);
    ShowWindow(hOverlayWnd, SW_SHOW);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitializeCriticalSection(&dataLock);
    CreateOverlay();
    CreateThread(nullptr, 0, EntityUpdater, nullptr, 0, nullptr);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    DeleteCriticalSection(&dataLock);
    return (int)msg.wParam;
}
