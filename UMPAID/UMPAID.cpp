#include <stdio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <stdbool.h>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <set>
#include <commdlg.h>
#include <string>
#include "Overlay.h"

#define MAPPER_COMMAND "kdmapper driver.sys"

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

std::vector<SHARED_DATA> entityList;
std::set<ULONG64> processedEntityPtrs;  // Set to track unique entity pointers

std::vector<SHARED_DATA>::iterator findEntityByPtr(ULONG64 entityPtr) {
    return std::find_if(entityList.begin(), entityList.end(), [entityPtr](const SHARED_DATA& e) {
        return e.entityPtr == entityPtr;
        });
}
//world + 0x2974 = Brightness; 1.0-200.0;

void Overlay::OverlayLoop()
{
    while (1)
    {
        //auto startTime = std::chrono::steady_clock::now();
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                // break other thread someehow
                break;
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(1920, 1080));

            ImGui::Begin("##ESP", (bool*)NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);

            // Open Menu
            HWND ForegroundWindow = GetForegroundWindow();
            LONG TmpLong = GetWindowLong(Hwnd, GWL_EXSTYLE);
            LONG MenuStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
            LONG ESPStyle = WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

            if (g.showMenu && MenuStyle != TmpLong)
                SetWindowLong(Hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
            else if (!g.showMenu && ESPStyle != TmpLong)
                SetWindowLong(Hwnd, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST);

            // MenuToggle
            if (GetKeyState(g.menuKey) && !g.showMenu) {
                g.showMenu = !g.showMenu;

                if (ForegroundWindow != Hwnd)
                    SetForegroundWindow(Hwnd);
            }
            else if (!GetKeyState(g.menuKey) && g.showMenu) {
                g.showMenu = !g.showMenu;
            }

            if (g.showMenu)
                RenderMenu();

            std::string InfoText = std::to_string((int)ImGui::GetIO().Framerate) + " FPS";
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(0.f, 0.f), ImColor(0, 255, 0), InfoText.c_str());

            // do radar here
            if (g.g_ESP_Radar) {
                //fakeMethod(); render();
                RenderRadar(entityList);
            }
            if (g.g_ESP) {
                RenderEsp(entityList);
            }

            ImGui::End();
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(0, 0);

        //auto endTime = std::chrono::steady_clock::now();
        //std::chrono::duration<double> elapsed_time = endTime - startTime;
        //std::cout << "[GameManager] Time taken for OverlayLoop(): " << elapsed_time.count() << " seconds\n";
    }
}

void coding_stuff(const char* filename)
{
    Sleep(1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    printf("Waiting for DayZ to launch...\n");
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        DWORD dayzPid = GetProcessIdByName(L"DayZ_x64.exe");
        if (dayzPid) {
            printf("DayZ launched! PID: %lu\n", dayzPid);
            WriteProcessInfoToFile(filename, dayzPid, 0x123456678);
            // Set status to 1
            // Set status to 1
            break;
        }
        Sleep(1000);
    }

    g_SharedData.y = 1;
    printf("Game detected. Status updated. Monitoring...\n");

    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        Sleep(30);
        printf("x: %d, y: %d, z: %d, entPtr: 0x%lld, playerPtr: 0x%lld\n", g_SharedData.x, g_SharedData.y, g_SharedData.z, (ULONG64)g_SharedData.entityPtr, (ULONG64)g_SharedData.localPlayerPtr);
        // Look for the entity in the list by entityPtr
        auto it = findEntityByPtr(g_SharedData.entityPtr); // at this point we have all angles

        if (it != entityList.end()) {
            // If the entity exists and the position is different, update it
            if (it->x != g_SharedData.x || it->y != g_SharedData.y || it->z != g_SharedData.z) {
                // Remove the old entry
                entityList.erase(it);
                // Insert the new entity with updated position
                entityList.push_back({ g_SharedData.x, g_SharedData.y, g_SharedData.z, (ULONG64)g_SharedData.entityPtr });

            }
        }
        else {
            // If the entity is not in the list, add it to the list
            entityList.push_back({g_SharedData.x, g_SharedData.y, g_SharedData.z, (ULONG64)g_SharedData.entityPtr });
            processedEntityPtrs.insert(g_SharedData.entityPtr);  // Add to set to avoid duplicates
        }
        // render coords on mini map and check for previous cords -> communicate with a overlay etc
        // Grab camera data
        ov.InvertedViewTranslationX = g_SharedData.InvertedViewTranslationX;
        ov.InvertedViewTranslationY = g_SharedData.InvertedViewTranslationY;
        ov.InvertedViewTranslationZ = g_SharedData.InvertedViewTranslationZ;
        ov.InvertedViewRightX = g_SharedData.InvertedViewRightX;
        ov.InvertedViewRightY = g_SharedData.InvertedViewRightY;
        ov.InvertedViewRightZ = g_SharedData.InvertedViewRightZ;
        ov.InvertedViewUpX = g_SharedData.InvertedViewUpX;
        ov.InvertedViewUpY = g_SharedData.InvertedViewUpY;
        ov.InvertedViewUpZ = g_SharedData.InvertedViewUpZ;
        ov.InvertedViewForwardX = g_SharedData.InvertedViewForwardX;
        ov.InvertedViewForwardY = g_SharedData.InvertedViewForwardY;
        ov.InvertedViewForwardZ = g_SharedData.InvertedViewForwardZ;
        ov.viewPortSizeX = g_SharedData.viewPortSizeX;
        ov.viewPortSizeY = g_SharedData.viewPortSizeY;
        ov.viewPortSizeZ = g_SharedData.viewPortSizeZ;
        ov.projectionD1X = g_SharedData.projectionD1X;
        ov.projectionD1Y = g_SharedData.projectionD1Y;
        ov.projectionD1Z = g_SharedData.projectionD1Z;
        ov.projectionD2X = g_SharedData.projectionD2X;
        ov.projectionD2Y = g_SharedData.projectionD2Y;
        ov.projectionD2Z = g_SharedData.projectionD2Z;
    }
}

int main() {
    const char* filename = "C:\\Users\\proxi\\source\\logfile.txt";
    DWORD pid = GetCurrentProcessId();
    ULONG64 baseAddr = (ULONG64)GetModuleHandle(NULL);

    printf("Struct Address: 0x%p\n", &g_SharedData);
    WriteProcessInfoToFile(filename, pid, (uintptr_t)&g_SharedData);

    printf("Mapping driver...\n");
    system(MAPPER_COMMAND);

    printf("Driver mapped. Launch DayZ and press Enter.\n");
    getchar();

    if (!ov.CreateOverlay()) // DONE
        return 2;

    std::thread([&]() { coding_stuff(filename); }).detach();
    ov.OverlayLoop();
    return 0;
}

// This setup writes the initial process PID and base address, maps the driver, waits for DayZ to launch, 
// then updates the file with DayZ's PID and base address, setting the status flag for the driver.