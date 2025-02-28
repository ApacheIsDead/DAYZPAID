#include <stdio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <stdbool.h>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>

#include <commdlg.h>



namespace gui {
    bool overlayVisible = true;
    COLORREF selectedColor = RGB(255, 105, 180); // Bubblegum pink
    HWND hwnd;

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_F1) {
                overlayVisible = !overlayVisible;
                ShowWindow(hwnd, overlayVisible ? SW_SHOW : SW_HIDE);
                if (overlayVisible) {
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    UpdateWindow(hwnd);
                }
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HBRUSH hBrush = CreateSolidBrush(RGB(255, 182, 193)); // Light pink background
            FillRect(hdc, &ps.rcPaint, hBrush);
            DeleteObject(hBrush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, selectedColor);
            DrawText(hdc, L"Press F1 to Toggle Overlay", -1, &ps.rcPaint, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            CHOOSECOLOR cc = {};
            static COLORREF customColors[16] = {};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.lpCustColors = customColors;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            cc.rgbResult = selectedColor;

            if (ChooseColor(&cc)) {
                selectedColor = cc.rgbResult;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    bool render() {
        const wchar_t CLASS_NAME[] = L"ConsoleGUI";

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = CLASS_NAME;

        RegisterClass(&wc);

        hwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
            CLASS_NAME,
            L"My Console GUI Overlay",
            WS_POPUP,
            100, 100, 800, 600,
            NULL, NULL, GetModuleHandle(NULL), NULL
        );

        if (hwnd == NULL) {
            return false;
        }

        SetLayeredWindowAttributes(hwnd, 0, (BYTE)(255 * 0.6), LWA_ALPHA);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return true;
    }
}

#define MAPPER_COMMAND "kdmapper driver.sys"

typedef struct _SHARED_DATA {
    LONG x;
    LONG y;
    LONG z;
    ULONG64 entityPtr;
} SHARED_DATA, * PSHARED_DATA;

SHARED_DATA g_SharedData = { 10, 0, 30, 0x123 };  // Global struct

void WriteProcessInfoToFile(const char* filename, DWORD pid, ULONG64 baseAddr) {
    FILE* file;
    if (fopen_s(&file, filename, "w") == 0) {
        fprintf(file, "%lu\n0x%llx", pid, baseAddr);
        fclose(file);
    }
}

DWORD GetProcessIdByName(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                CloseHandle(hSnapshot);
                return pe.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return 0;
}
/*
ULONG64 GetBaseAddress(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

    MODULEENTRY32 me;
    me.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(hSnapshot, &me)) {
        CloseHandle(hSnapshot);
        return (ULONG64)me.modBaseAddr;
    }

    CloseHandle(hSnapshot);
    return 0;
}*/

int main() {
    const char* filename = "C:\\Users\\proxi\\source\\logfile.txt";
    DWORD pid = GetCurrentProcessId();
    ULONG64 baseAddr = (ULONG64)GetModuleHandle(NULL);

    printf("Struct Address: 0x%p\n", &g_SharedData);
    WriteProcessInfoToFile(filename, pid, (uintptr_t) & g_SharedData);

    printf("Mapping driver...\n");
    system(MAPPER_COMMAND);

    printf("Driver mapped. Launch DayZ and press Enter.\n");
    getchar();

    printf("Waiting for DayZ to launch...\n");
    while (1) {
        DWORD dayzPid = GetProcessIdByName(L"DayZ_x64.exe");
        if (dayzPid) {
            ULONG64 dayzBaseAddr = GetBaseAddress(dayzPid);
            printf("DayZ launched! PID: %lu, Base Addr: 0x%llx\n", dayzPid, dayzBaseAddr);
            WriteProcessInfoToFile(filename, dayzPid, dayzBaseAddr);
            // Set status to 1
            break;
        }
        Sleep(1000);
    }

    g_SharedData.y = 1;
    printf("Game detected. Status updated. Monitoring...\n");
    while (1) {
        printf("x: %d, y: %d, z: %d, buffer: 0x%lld\n", g_SharedData.x, g_SharedData.y, g_SharedData.z, (ULONG64)g_SharedData.entityPtr);
        Sleep(25);
        // render coords on mini map and check for previous cords -> communicate with a overlay etc
        
    }
    return 0;
}

// This setup writes the initial process PID and base address, maps the driver, waits for DayZ to launch, 
// then updates the file with DayZ's PID and base address, setting the status flag for the driver.

// Configuration
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

struct COORDS {
    LONG x;
    LONG y;
    LONG z;
};

struct Entity {
    COORDS position;
    bool isPlayer;
    std::string name;
};

const int overlaySize = 400;
const float worldRadius = 8000.0f;
HWND hOverlayWnd = nullptr;
CRITICAL_SECTION dataLock;
std::vector<Entity> entities;

void UpdateEntities() 
{
    while (true) 
    {
        EnterCriticalSection(&dataLock);
        entities.clear();

        for (int i = 0; i < g_SharedData.entityListSize; i++) 
        {
            Entity playerEntity;
            playerEntity.position.x = g_SharedData.x;
            playerEntity.position.y = g_SharedData.y;
            playerEntity.position.z = g_SharedData.z;
            entities.push_back(playerEntity);
        }

        LeaveCriticalSection(&dataLock);
        InvalidateRect(hOverlayWnd, nullptr, TRUE);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

DWORD WINAPI EntityUpdater(LPVOID lpParam) {
    UpdateEntities();
    return 0;
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
    for (const Entity& ent : entities) 
    {
        POINT screenPos = WorldToScreen(ent.position.x, ent.position.y);
        int x = overlaySize / 2 + screenPos.x;
        int y = overlaySize / 2 + screenPos.y;
        SelectObject(hdc, hBlueBrush);
        Ellipse(hdc, x - 5, y - 5, x + 5, y + 5);
    }
    LeaveCriticalSection(&dataLock);

    // Draw entity labels with scaling
    SetTextColor(hdc, RGB(200, 230, 255));
    SetBkMode(hdc, TRANSPARENT);

    // Determine the scale factor for font size based on overlay size
    float fontScale = overlaySize / worldRadius / 25.0f; // Adjust as needed for optimal size

    for (const Entity& entity : entities) {
        POINT screenPos = WorldToScreen(entity.position.x, entity.position.y);
        int x = overlaySize / 2 + screenPos.x;
        int y = overlaySize / 2 + screenPos.y;

        // Create a scaled font based on the scale factor
        int fontSize = static_cast<int>(12 * fontScale);  // Adjust the base size (12) as needed
        HFONT hFont = CreateFont(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Arial");
        SelectObject(hdc, hFont);

        // Draw entity position circle
        SelectObject(hdc, hWhiteBrush);
        Ellipse(hdc, x - 2, y - 2, x + 2, y + 2);

        // Draw the entity name with adjusted positioning
        auto entityName = entity.name.c_str();
        TextOutA(hdc, x + 5, y - 8, entityName, (int)strlen(entityName));

        // Clean up the font object after drawing
        DeleteObject(hFont);
    }

    DeleteObject(hBlueBrush);
    DeleteObject(hWhiteBrush);
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