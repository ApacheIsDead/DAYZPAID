#include <stdio.h>
#include <windows.h>

typedef struct _SHARED_DATA {
    LONG x;
    LONG y;
    LONG z;
    ULONG64 buffer;
} SHARED_DATA, * PSHARED_DATA;

SHARED_DATA g_SharedData = { 10, 20, 30, 0x123 };  // Global struct

int main() {
    printf("Struct Address: 0x%p\n", &g_SharedData);
    while (1) {
        printf("x: %d, y: %d, z: %d\n", g_SharedData.x, g_SharedData.y, g_SharedData.z);
        Sleep(1000);  // Keep running
    }
    return 0;
}