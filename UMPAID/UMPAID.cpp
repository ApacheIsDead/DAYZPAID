#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>  // For setting address format

// Define the shared data structure matching kernel's SHARED_DATA
typedef struct _SHARED_DATA {
    LONG x;
    LONG y;
    LONG z;
} SHARED_DATA, * PSHARED_DATA;

int main() {
    // Allocate memory for the structure in user mode
    PSHARED_DATA sharedData = new SHARED_DATA;
    if (!sharedData) {
        std::cerr << "Failed to allocate memory for shared struct." << std::endl;
        return 1;
    }

    // Initialize the values of the struct (or leave them as zero if not set elsewhere)
    sharedData->x = 0;
    sharedData->y = 0;
    sharedData->z = 0;

    // Print the address of the struct in user-mode address space (formatted correctly)
    printf("shared struct: %p\n", (void*)&sharedData);
    std::cout << "Shared struct: " << &sharedData << std::endl;
    // Continuously print the values of x, y, z in a loop
    while (true) {
        std::cout << "X: " << sharedData->x << ", Y: " << sharedData->y << ", Z: " << sharedData->z << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));  // Wait for 1 second before printing again
    }

    // Clean up (this will never be reached in the current infinite loop)
    delete sharedData;  // Free the allocated memory

    return 0;
}
