#include <ntifs.h>
#include <ntddk.h>
#include <float.h>
#include <float.h>
#pragma comment(lib, "ntoskrnl.lib")

// Needs to read process ID From File and then the driver reading part should be ready, after need to translate to User Mode

extern "C" {
    NTKERNELAPI NTSTATUS MmCopyVirtualMemory(
        PEPROCESS SourceProcess,
        PVOID SourceAddress,
        PEPROCESS TargetProcess,
        PVOID TargetAddress,
        SIZE_T BufferSize,
        KPROCESSOR_MODE PreviousMode,
        PSIZE_T ReturnSize
    );

    NTSTATUS ZwProtectVirtualMemory(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        PSIZE_T RegionSize,
        ULONG NewProtect,
        PULONG OldProtect
    );

    NTKERNELAPI VOID RtlFreeUnicodeString(
        PUNICODE_STRING UnicodeString
    );
}
#define SCALE_FACTOR 10000  // A factor to simulate floating-point behavior

// Function to manually convert ULONG to a scaled integer
ULONG ConvertToScaledULONG(ULONG rawValue) {
    return rawValue * SCALE_FACTOR;  // Scale to simulate float behavior
}

// Function to round the scaled value to simulate a float
ULONG ConvertToFinalULONG(ULONG scaledValue) {
    return (scaledValue + (SCALE_FACTOR / 2)) / SCALE_FACTOR;  // Round the value
}
typedef struct _VECTOR3_FLOAT {
    float x;
    float y;
    float z;
} VECTOR3_FLOAT, * PVECTOR3_FLOAT;

typedef struct _VECTOR3_RAW {
    ULONG x;
    ULONG y;
    ULONG z;
    int pid;

} VECTOR3_RAW, * PVECTOR3_RAW;
char* globalVariable[256] = { NULL };
typedef struct _VECTOR3 {
    LONG x;
    LONG y;
    LONG z;
} VECTOR3, * PVECTOR3;

#define world 0x41B32A0
#define localplayer 0x2968
#define gameBase 0x7ff659730000

char* gPID = 0;  // Global variable for PID
char* gBaseAddr = 0;  // Global variable for Base Address
ULONG64 gBaseDayZ = 0x0;
ULONG64 gPIDDayZ = 0;

typedef struct _SHARED_DATA {
    LONG x;
    LONG y;
    LONG z;
    ULONG64 baseAddr;
    LONG pid;
} SHARED_DATA, * PSHARED_DATA;

NTSTATUS WriteSharedStructCoords(HANDLE targetPid, PVOID userStructAddress, LONG xd, LONG xy, LONG xz) {
    PEPROCESS targetProcess;
    NTSTATUS status = PsLookupProcessByProcessId(targetPid, &targetProcess);
    if (!NT_SUCCESS(status)) return status;

    SHARED_DATA localCopy = { 0 };
    SIZE_T bytes = sizeof(SHARED_DATA);

    // Ensure compatibility with PsGetCurrentProcess()
    PVOID processPtr = PsGetCurrentProcess();
    PEPROCESS currentProcess = (PEPROCESS)processPtr;

    // Read struct from user-mode process
    status = MmCopyVirtualMemory(targetProcess, userStructAddress, currentProcess, &localCopy, bytes, KernelMode, &bytes);
    if (!NT_SUCCESS(status)) {
        ObDereferenceObject(targetProcess);
        return status;
    }

    // Modify values
    localCopy.x = xd;
    localCopy.y = xy;
    localCopy.z = xz;

    // Write it back
    status = MmCopyVirtualMemory(currentProcess, &localCopy, targetProcess, userStructAddress, bytes, KernelMode, &bytes);

    ObDereferenceObject(targetProcess);
    return status;
}

NTSTATUS ReadStructCoords(HANDLE targetPid, PVOID userStructAddress) {
    PEPROCESS targetProcess;
    NTSTATUS status = PsLookupProcessByProcessId(targetPid, &targetProcess);
    if (!NT_SUCCESS(status)) return status;

    SHARED_DATA localCopy = { 0 };
    SIZE_T bytes = sizeof(SHARED_DATA);

    // Ensure compatibility with PsGetCurrentProcess()
    PVOID processPtr = PsGetCurrentProcess();
    PEPROCESS currentProcess = (PEPROCESS)processPtr;

    // Read struct from user-mode process
    status = MmCopyVirtualMemory(targetProcess, userStructAddress, currentProcess, &localCopy, bytes, KernelMode, &bytes);
    if (!NT_SUCCESS(status)) {
        ObDereferenceObject(targetProcess);
        return status;
    }
    gBaseDayZ = localCopy.baseAddr;
    gPIDDayZ = localCopy.pid;
    DbgPrintEx(0, 0, "Read dayz base: %llx", gBaseDayZ);
    DbgPrintEx(0, 0, "Read dayz base: %s", gBaseDayZ);
    return STATUS_SUCCESS;
}
#include <ntddk.h>


void ConvertToWideChar(_In_ const char* narrowStr, _Out_ wchar_t* wideStr, _In_ ULONG wideStrSize)
{
    NTSTATUS status;
    ULONG wideLength = 0;

    status = RtlMultiByteToUnicodeN(
        wideStr,               // Destination buffer (wchar_t*)
        wideStrSize,           // Size of destination buffer (in bytes)
        &wideLength,           // Receives number of bytes written
        narrowStr,             // Source multi-byte string (char*)
        (ULONG)strlen(narrowStr) // Length of the source string
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("Conversion failed with status: 0x%X\n", status);
    }
    else {
        // Null-terminate the string manually if space allows
        if (wideLength + sizeof(wchar_t) <= wideStrSize) {
            wideStr[wideLength / sizeof(wchar_t)] = L'\0';
        }
    }
}

NTSTATUS ReadTextFile() {
    // File path to read from - make sure it's a valid path
    UNICODE_STRING filePath;
    RtlInitUnicodeString(&filePath, L"\\??\\C:\\Users\\proxi\\source\\logfile.txt");

    HANDLE fileHandle = NULL;
    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status;
    
    // Open the file
    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(&objectAttributes, &filePath, OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwCreateFile(&fileHandle, GENERIC_READ, &objectAttributes, &ioStatus, NULL,
        FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to open file: 0x%X\n", status);
        return status;
    }

    // Read from the file
    char buffer[256];  // Adjust size as needed
    RtlZeroMemory(buffer, sizeof(buffer));

    status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatus, buffer, sizeof(buffer) - 1, NULL, NULL);
    if (NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "File contents:\n%s\n", buffer);

        // Parse the PID and BaseAddr from the buffer
        char* line = buffer;
        char* pidStr = line;
        char* baseAddrStr = NULL;

        // Find the first newline character to separate the lines
        while (*line != '\0') {
            if (*line == '\n') {
                *line = '\0';  // Null-terminate the first line
                baseAddrStr = line + 1;  // Start of the second line (BaseAddr)
                break;

            }
            line++;
        }
        gPID = pidStr;
        gBaseAddr = baseAddrStr;
        DbgPrintEx(0, 0, "%s", gPID);
        DbgPrintEx(0, 0, "%s", gBaseAddr);
    }

    // Close the file
    ZwClose(fileHandle);

    return STATUS_SUCCESS;
}

NTSTATUS ReadPointer(PEPROCESS targetProcess, uintptr_t address, uintptr_t* outValue) {
    SIZE_T bytesRead;
    return MmCopyVirtualMemory(targetProcess, (PVOID)address,
        PsGetCurrentProcess(), outValue,
        sizeof(uintptr_t), KernelMode, &bytesRead);
}

NTSTATUS ReadMemory(PEPROCESS targetProcess, uintptr_t address, void* buffer, SIZE_T size) {
    SIZE_T bytesRead;
    return MmCopyVirtualMemory(targetProcess, (PVOID)address,
        PsGetCurrentProcess(), buffer,
        size, KernelMode, &bytesRead);
}

NTSTATUS getAssets() {
    PEPROCESS targetProcess;
    NTSTATUS status = ReadStructCoords((HANDLE)gPID, (PVOID)gBaseAddr);
    if (NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Win");
    }
    else {
        DbgPrintEx(0, 0, "%x", status);
    }
    uintptr_t worldPointerAddress = gBaseDayZ + world;
    uintptr_t worldPointerValue = 0;
    /*
    * GET PID FROM FILE -- > (HANDLE)processID -> use below |
    */                   
    //v
    DbgPrintEx(0, 0, "%llx", gPIDDayZ);
    status = PsLookupProcessByProcessId((HANDLE)gPIDDayZ, &targetProcess);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to get target process\n");
        return status;
    }

    // Then loads process and gets world pointer
    status = ReadPointer(targetProcess, worldPointerAddress, &worldPointerValue);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to read world pointer (Status: 0x%X)\n", status);
        ObDereferenceObject(targetProcess);
        return status;
    }

    // Then Gets the entity count from the size of the entity list - s0xF50
    INT32 entityCount = 0;
    status = ReadMemory(targetProcess, worldPointerValue + 0xF50, &entityCount, sizeof(entityCount));
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to read entity count (Status: 0x%X)\n", status);
        ObDereferenceObject(targetProcess);
        return status;
    }
    DbgPrintEx(0, 0, "Entity Count: %u\n", entityCount);

    // Then Gets the Entity list from offset 0xF48
    uintptr_t entityListBase = worldPointerValue + 0xF48;
    uintptr_t entityListPointerValue = 0;
    status = ReadPointer(targetProcess, entityListBase, &entityListPointerValue);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to read entity list base\n");
        ObDereferenceObject(targetProcess);
        return status;
    }

    //cheeze loop then loops through entity list if it isnt 0; outputting cords to User Mode Map
    while (true) {
        if (entityCount <= 0) {
            DbgPrintEx(0, 0, "No Entities Detected");
        }
        else {
            for (size_t i = 0; i < entityCount; i++) {
                uintptr_t entityEntryAddress = entityListPointerValue + i * sizeof(uintptr_t);
                uintptr_t entityPtr = 0;
                status = ReadPointer(targetProcess, entityEntryAddress, &entityPtr);
                if (!NT_SUCCESS(status) || entityPtr == 0) {
                    continue;
                }

                uintptr_t visualStateAddress = entityPtr + 0x1D0;
                uintptr_t visualStatePtr = 0;
                status = ReadPointer(targetProcess, visualStateAddress, &visualStatePtr);
                if (!NT_SUCCESS(status) || visualStatePtr == 0) {
                    continue;
                }

                // Read cords into regular vector 3 LONG struct aswell
                VECTOR3 rawCords;
                status = ReadMemory(targetProcess, visualStatePtr + 0x2C, &rawCords, sizeof(rawCords));
                if (NT_SUCCESS(status)) {
                    DbgPrintEx(0, 0, "Raw Cords (LONGS): X=%ld, Y=%ld, Z=%ld\n", rawCords.x, rawCords.y, rawCords.z);
                    WriteSharedStructCoords((HANDLE)gPID, (PVOID)gBaseAddr, rawCords.x, rawCords.y, rawCords.z); // writes cords to um -- now all we need is a text file read to grab these values
                    // um just  has to grab them display them, and delete the previous ones for updates
                }

                //ULONG VERSION
                VECTOR3_RAW rawCoords;
                status = ReadMemory(targetProcess, visualStatePtr + 0x2C, &rawCoords, sizeof(rawCoords));

                if (NT_SUCCESS(status)) {
                    DbgPrintEx(0, 0, "Raw Coords (Read as ULONGs): X=%lu, Y=%lu, Z=%lu\n",
                        rawCoords.x, rawCoords.y, rawCoords.z);
                }


            }

        }
        LARGE_INTEGER interval;
        interval.QuadPart = -100000000LL; // 10 seconds in 100-nanosecond intervals (negative for relative delay)
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
    }

    ObDereferenceObject(targetProcess);
    return STATUS_SUCCESS;
}

/* // TELPORTATION LOGIC vvv
*
// Define a write helper function using MmCopyVirtualMemory.
NTSTATUS WriteMemory(PEPROCESS targetProcess, uintptr_t address, void* buffer, SIZE_T size) {
    SIZE_T bytesWritten = 0;
    return MmCopyVirtualMemory(
        PsGetCurrentProcess(),  // Our (kernel) process as source.
        buffer,                 // Buffer containing the data to write.
        targetProcess,          // Target process.
        (PVOID)address,         // Address in target process.
        size,                   // Size of data.
        KernelMode,
        &bytesWritten
    );
}

// This function sets the entity's position to the provided targetPosition.
NTSTATUS SetEntityPosition(PEPROCESS targetProcess, uintptr_t entity, VECTOR3 targetPosition)
{
    NTSTATUS status = STATUS_SUCCESS;
    uintptr_t visualStateAddress = 0;
    uintptr_t baseAddress = 0;

    // First, read the world pointer from gameBase+world.
    uintptr_t worldPointerValue = 0;
    status = ReadPointer(targetProcess, gameBase + world, &worldPointerValue);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to read world pointer: 0x%X\n", status);
        return status;
    }

    // Then get the local player pointer from the world pointer + localplayer offset.
    uintptr_t localPlayerEntity = 0;
    status = ReadPointer(targetProcess, worldPointerValue + localplayer, &localPlayerEntity);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to read local player pointer: 0x%X\n", status);
        return status;
    }

    // Choose the correct offset based on whether this entity is the local player.
    if (entity == localPlayerEntity) {
        // Local player: the visual state pointer is stored at entity + 0xF0.
        status = ReadPointer(targetProcess, entity + 0xF0, &baseAddress);
        if (!NT_SUCCESS(status)) {
            DbgPrintEx(0, 0, "Failed to read local player's visual state pointer: 0x%X\n", status);
            return status;
        }
    }
    else {
        // Other entities: the visual state pointer is stored at entity + 0x1D0.
        status = ReadPointer(targetProcess, entity + 0x1D0, &baseAddress);
        if (!NT_SUCCESS(status)) {
            DbgPrintEx(0, 0, "Failed to read entity's visual state pointer: 0x%X\n", status);
            return status;
        }
    }

    // The coordinates are located at an offset of 0x2C from the visual state pointer.
    visualStateAddress = baseAddress + 0x2C;

    // Now, write the new position (targetPosition) into the target process.
    status = WriteMemory(targetProcess, visualStateAddress, &targetPosition, sizeof(VECTOR3));
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Failed to write new position: 0x%X\n", status);
    }
    else {
        DbgPrintEx(0, 0, "Position updated: X=%ld, Y=%ld, Z=%ld\n", targetPosition.x, targetPosition.y, targetPosition.z);
    }

    return status;
}*/

// TELPORTATION LOGIC ^^^


NTSTATUS DriverEntryCustom(
    _In_ PDRIVER_OBJECT  kdmapperParam1,
    _In_ PUNICODE_STRING kdmapperParam2
)
{
    UNREFERENCED_PARAMETER(kdmapperParam1);
    UNREFERENCED_PARAMETER(kdmapperParam2);

    // Read Hex Addr From File -> hexAddr
    // Introduce a 30-second delay
    ReadTextFile();
    LARGE_INTEGER interval;
    interval.QuadPart = -100000000LL; // 30 seconds in 100-nanosecond intervals (negative for relative delay)
    KeDelayExecutionThread(KernelMode, FALSE, &interval);

    /*
    * Obtain um process ID, Struct address 
    * Read From Struct in that um application to get game Process ID and Base Address
    * Fill corresponding values - working dayz cheat
    */
    
    NTSTATUS status = getAssets();
    if (NT_SUCCESS(status)) {
        DbgPrintEx(0, 0, "Assets retrieved successfully.\n");
        return STATUS_SUCCESS;
    }
    else {

        return STATUS_ABANDONED;
    }
}


// when the driver loads it grabs PID of um app, Struct Base Address from TXT FILE, then waits and grabs the PID of DayZ from Struct, and then passes values of cords into struct wich are displayed

