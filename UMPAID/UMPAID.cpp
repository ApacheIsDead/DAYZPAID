#include <stdio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <stdbool.h>

#include <windows.h>
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
    ULONG64 buffer;
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
}

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
        printf("x: %d, y: %d, z: %d, buffer: 0x%llx\n", g_SharedData.x, g_SharedData.y, g_SharedData.z, g_SharedData.buffer);
        Sleep(25);
        // render coords on mini map and check for previous cords -> communicate with a overlay etc

    }

    return 0;
}

// This setup writes the initial process PID and base address, maps the driver, waits for DayZ to launch, 
// then updates the file with DayZ's PID and base address, setting the status flag for the driver.