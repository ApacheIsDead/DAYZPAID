#include <stdio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <stdbool.h>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>

#include <commdlg.h>

#include <set>

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

std::vector<SHARED_DATA> entityList;
std::set<ULONG64> processedEntityPtrs;  // Set to track unique entity pointers

std::vector<SHARED_DATA>::iterator findEntityByPtr(ULONG64 entityPtr) {
    return std::find_if(entityList.begin(), entityList.end(), [entityPtr](const SHARED_DATA& e) {
        return e.entityPtr == entityPtr;
        });
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

    printf("Waiting for DayZ to launch...\n");
    while (1) {
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
        Sleep(50);
        printf("x: %d, y: %d, z: %d, buffer: 0x%lld\n", g_SharedData.x, g_SharedData.y, g_SharedData.z, (ULONG64)g_SharedData.entityPtr);
        // Look for the entity in the list by entityPtr
        auto it = findEntityByPtr(g_SharedData.entityPtr);

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
            entityList.push_back({ g_SharedData.x, g_SharedData.y, g_SharedData.z, (ULONG64)g_SharedData.entityPtr });
            processedEntityPtrs.insert(g_SharedData.entityPtr);  // Add to set to avoid duplicates
        }
        // render coords on mini map and check for previous cords -> communicate with a overlay etc
    }

    return 0;
}

// This setup writes the initial process PID and base address, maps the driver, waits for DayZ to launch, 
// then updates the file with DayZ's PID and base address, setting the status flag for the driver.