#include <ntifs.h>
#include <wdm.h>




#pragma comment(lib, "ntoskrnl.lib")

#define world 0x41CFB68
#define localplayer 0x2960
#define gameBase 0x7ff6d3a90000

extern "C" {
	PVOID PsGetProcessSectionBaseAddress(PEPROCESS Process);
}
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

NTSTATUS WriteMemory(
	PEPROCESS targetProcess,
	PVOID targetAddress,
	PVOID buffer,
	SIZE_T size
) {
	SIZE_T bytesWritten;
	NTSTATUS status = MmCopyVirtualMemory(
		PsGetCurrentProcess(),     // Source process (current process)
		buffer,                    // Buffer to write from
		targetProcess,             // Target process
		targetAddress,             // Target address to write to
		size,                      // Size of the memory to write
		KernelMode,                // Access mode
		&bytesWritten              // Number of bytes written
	);

	return status;
}


typedef struct _VECTOR3_RAW {
	ULONG x;
	ULONG y;
	ULONG z;
} VECTOR3_RAW, * PVECTOR3_RAW;

typedef struct _VECTOR3 {
	LONG x;
	LONG y;
	LONG z;
} VECTOR3, * PVECTOR3;

typedef struct _SHARED_DATA {
	LONG x;
	LONG y;
	LONG z;
	ULONG64 entityPtr;
	ULONG64 localPlayerPtr;
	LONG InvertedViewTranslationX;
	LONG InvertedViewTranslationY;
	LONG InvertedViewTranslationZ;
	LONG InvertedViewRightX;
	LONG InvertedViewRightY;
	LONG InvertedViewRightZ;
	LONG InvertedViewUpX;
	LONG InvertedViewUpY;
	LONG InvertedViewUpZ;
	LONG InvertedViewForwardX;
	LONG InvertedViewForwardY;
	LONG InvertedViewForwardZ;
	LONG viewPortSizeX;
	LONG viewPortSizeY;
	LONG viewPortSizeZ;
	LONG projectionD1X;
	LONG projectionD1Y;
	LONG projectionD1Z;
	LONG projectionD2X;
	LONG projectionD2Y;
	LONG projectionD2Z;
	INT32 option;
} SHARED_DATA, * PSHARED_DATA;

typedef struct _SHARED_DATA_2 {
	LONG x;
	LONG y;
	LONG z;
	int entityListSize;
} SHARED_DATA_2, * PSHARED_DATA_2;

int gPID;
PVOID gBaseAddr;
int gPIDDayZ;
PVOID gBaseAddrDayZ;

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

NTSTATUS WriteSharedStructEspInformation(HANDLE targetPid, PVOID userStructAddress, LONG xd, LONG xy, LONG xz, ULONG64 entityPointer, 
								ULONG64 localPlayerPtr, LONG invertedx, LONG invertedy, LONG invertedz, LONG invertedviewrightx, 
								LONG invertedviewrighty, LONG invertedviewrightz, LONG invertedviewupx,
								LONG invertedviewupy, LONG invertedviewupz, LONG invertedviewforwardx, LONG invertedviewforwardy,
								LONG invertedviewforwardz, LONG viewportsizex, LONG viewportsizey, LONG viewportsizez, LONG projectiond1x, 
								LONG projectiond1y, LONG projectiond1z, LONG projectiond2x, LONG projectiond2y, LONG projectiond2z) {
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
	localCopy.entityPtr = entityPointer;
	localCopy.localPlayerPtr = localPlayerPtr;
	localCopy.InvertedViewTranslationX = invertedx;
	localCopy.InvertedViewTranslationY = invertedy;
	localCopy.InvertedViewTranslationZ = invertedz;
	localCopy.InvertedViewRightX = invertedviewrightx;
	localCopy.InvertedViewRightY = invertedviewrighty;
	localCopy.InvertedViewRightZ = invertedviewrightz;
	localCopy.InvertedViewUpX = invertedviewupx;
	localCopy.InvertedViewUpY = invertedviewupy;
	localCopy.InvertedViewUpZ = invertedviewupz;
	localCopy.InvertedViewForwardX = invertedviewforwardx;
	localCopy.InvertedViewForwardY = invertedviewforwardy;
	localCopy.InvertedViewForwardZ = invertedviewforwardz;
	localCopy.viewPortSizeX = viewportsizex;
	localCopy.viewPortSizeY = viewportsizey;
	localCopy.viewPortSizeZ = viewportsizez;
	localCopy.projectionD1X = projectiond1x;
	localCopy.projectionD1Y = projectiond1y;
	localCopy.projectionD1Z = projectiond1z;
	localCopy.projectionD2X = projectiond2x;
	localCopy.projectionD2Y = projectiond2y;
	localCopy.projectionD2Z = projectiond2z;
	// Write it back
	status = MmCopyVirtualMemory(currentProcess, &localCopy, targetProcess, userStructAddress, bytes, KernelMode, &bytes);
	ObDereferenceObject(targetProcess);
	return status;
}

NTSTATUS WriteSharedStructRadarInformation(HANDLE targetPid, PVOID userStructAddress, LONG xd, LONG xy, LONG xz, ULONG64 entityPointer,
	ULONG64 localPlayerPtr) {
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
	localCopy.entityPtr = entityPointer;
	localCopy.localPlayerPtr = localPlayerPtr;
	// Write it back
	status = MmCopyVirtualMemory(currentProcess, &localCopy, targetProcess, userStructAddress, bytes, KernelMode, &bytes);
	ObDereferenceObject(targetProcess);
	return status;
}


NTSTATUS noGrass() {
	// world + BF0 (float 0)
	PEPROCESS targetProcess2;  // Pointer to store the target process
	DbgPrintEx(0, 0, "%llx", (uintptr_t)gBaseAddrDayZ);

	uintptr_t worldPointerAddress = (uintptr_t)gBaseAddrDayZ + world; // Game base address + world offset
	uintptr_t worldPointerValue = 0;

	// Get handle to the process
	NTSTATUS statusLookup = PsLookupProcessByProcessId((HANDLE)gPIDDayZ, &targetProcess2); // DayZ PID here
	if (!NT_SUCCESS(statusLookup)) {
		DbgPrintEx(0, 0, "Failed to get target process\n");
		return statusLookup;
	}

	// Read world pointer
	NTSTATUS statusRead = ReadPointer(targetProcess2, worldPointerAddress, &worldPointerValue);
	if (!NT_SUCCESS(statusRead)) {
		DbgPrintEx(0, 0, "Failed to read world pointer (Status: 0x%X)\n", statusRead);
		ObDereferenceObject(targetProcess2);
		return statusRead;
	}

	if (worldPointerValue == NULL) {
		DbgPrint("Invalid pointer.\n");
		ObDereferenceObject(targetProcess2);
		return STATUS_INVALID_PARAMETER;
	}

	// Raw bytes of 0.0f (as a 32-bit integer)
	ULONG floatBits = 0x00000000;

	// Calculate the target address in user space
	PULONG targetAddress = (PULONG)((ULONG_PTR)worldPointerValue + 0xBF0);

	// Perform memory copy from kernel to user-mode memory using MmCopyVirtualMemory
	SIZE_T bytesWritten = 0;
	NTSTATUS statusCopy = MmCopyVirtualMemory(
		PsGetCurrentProcess(),            // Source process (current process, kernel space)
		&floatBits,                       // Source buffer (the raw bytes we want to write)
		targetProcess2,                   // Destination process (target process)
		targetAddress,                    // Destination address (user-space address to write to)
		sizeof(float),                    // Size of the data to copy (size of a float)
		KernelMode,                       // The operation is in KernelMode
		&bytesWritten                     // Number of bytes actually copied
	);

	if (NT_SUCCESS(statusCopy)) {
		DbgPrint("Successfully wrote 0.0f (raw bytes: 0x00000000) to address: %p\n", targetAddress);
	}
	else {
		DbgPrint("Failed to write to address: %p, status: 0x%X\n", targetAddress, statusCopy);
	}

	// Clean up
	ObDereferenceObject(targetProcess2);

	return STATUS_SUCCESS;
}


NTSTATUS sendRadarInformation() {
	PEPROCESS targetProcess;        //(uintptr_t)gBaseAddrDayZ
	DbgPrintEx(0, 0, "%llx", (uintptr_t)gBaseAddrDayZ);
	uintptr_t worldPointerAddress = (uintptr_t)gBaseAddrDayZ + world; // Game base address + world offset - here
	uintptr_t worldPointerValue = 0;
	uintptr_t localPlayerPtr = 0;
	// get handle
	NTSTATUS status = PsLookupProcessByProcessId((HANDLE)gPIDDayZ, &targetProcess); // DayZ PID here
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to get target process\n");
		return status;
	}

	// world pointer
	status = ReadPointer(targetProcess, worldPointerAddress, &worldPointerValue);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read world pointer (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}


	// local player
	status = ReadPointer(targetProcess, worldPointerAddress + 0x2960, &localPlayerPtr);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read local player pointer (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}
	// far entity list
	uintptr_t entityListBase = worldPointerValue + 0x1090;
	uintptr_t entityListPointerValue = 0;
	status = ReadPointer(targetProcess, entityListBase, &entityListPointerValue);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read entity list base\n");
		ObDereferenceObject(targetProcess);
		return status;

	}
	INT32 entityCount = 0;
	status = ReadMemory(targetProcess, worldPointerValue + 0x1098, &entityCount, sizeof(entityCount));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read entity count (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}

	for (size_t i = 0; i < entityCount; i++) {
		uintptr_t entityEntryAddress = entityListPointerValue + i * sizeof(uintptr_t);
		uintptr_t entityPtr = 0;
		status = ReadPointer(targetProcess, entityEntryAddress, &entityPtr);
		if (!NT_SUCCESS(status) || entityPtr == 0) {
			continue;
		}

		// see if its a zombie
		// variable to distuingish if client wants zombies rendered

		uintptr_t visualStateAddress = entityPtr + 0x1D0;
		uintptr_t visualStatePtr = 0;
		status = ReadPointer(targetProcess, visualStateAddress, &visualStatePtr);
		if (!NT_SUCCESS(status) || visualStatePtr == 0) {
			continue;
		}
		LARGE_INTEGER interval;
		interval.QuadPart = -10000 * 100; // Time in 100ns units, negative value indicates sleep
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
		// Read cords into regular vector 3 LONG struct aswell
		VECTOR3 rawCords;
		status = ReadMemory(targetProcess, visualStatePtr + 0x2C, &rawCords, sizeof(rawCords));
		if (NT_SUCCESS(status)) {
			DbgPrintEx(0, 0, "Raw Cords (LONGS): X=%ld, Y=%ld, Z=%ld\n", rawCords.x, rawCords.y, rawCords.z);
		}

		//ULONG VERSION
		VECTOR3_RAW rawCoords;
		status = ReadMemory(targetProcess, visualStatePtr + 0x2C, &rawCoords, sizeof(rawCoords));

		if (NT_SUCCESS(status)) {
			DbgPrintEx(0, 0, "Raw Coords (Read as ULONGs): X=%lu, Y=%lu, Z=%lu\n",
				rawCoords.x, rawCoords.y, rawCoords.z);
		}
		//NTSTATUS WriteSharedStructInfo(HANDLE targetPid, PVOID userStructAddress, LONG xd, LONG xy, LONG xz, ULONG64 entityPointer, ULONG64 localPlayerPtr, LONG invertedx, LONG invertedy, LONG invertedz, LONG invertedviewrightx, LONG invertedviewrighty, LONG invertedviewrightz, LONG invertedviewforwardx, LONG invertedviewforwardy, LONG invertedviewforwardz, LONG viewportsizex, LONG viewportsizey, LONG viewportsizez, LONG projectiond1x, LONG projectiond1y, LONG projectiond1z, LONG projectiond2x, LONG projectiond2y, LONG projectiond2z) 
		WriteSharedStructRadarInformation((HANDLE)gPID, (PVOID)gBaseAddr, rawCoords.x, rawCoords.y, rawCoords.z, entityPtr, localPlayerPtr);
	}
	ObDereferenceObject(targetProcess);
	return STATUS_SUCCESS;
}

NTSTATUS sendEspInformation() {
	PEPROCESS targetProcess;        //(uintptr_t)gBaseAddrDayZ
	DbgPrintEx(0, 0, "%llx", (uintptr_t)gBaseAddrDayZ); 
	uintptr_t worldPointerAddress = (uintptr_t)gBaseAddrDayZ + world; // Game base address + world offset - here
	uintptr_t worldPointerValue = 0;
	uintptr_t localPlayerPtr = 0;
	uintptr_t cameraPtr = 0;
	// get handle
	NTSTATUS status = PsLookupProcessByProcessId((HANDLE)gPIDDayZ, &targetProcess); // DayZ PID here
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to get target process\n");
		return status;
	}

	// world pointer
	status = ReadPointer(targetProcess, worldPointerAddress, &worldPointerValue);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read world pointer (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}

	status = ReadMemory(targetProcess, worldPointerValue + 0x1B8, &cameraPtr, sizeof(cameraPtr));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read camera pointer (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}

	// local player
	status = ReadPointer(targetProcess, worldPointerAddress + 0x2960, &localPlayerPtr);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read local player pointer (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}
	// far entity list
	uintptr_t entityListBase = worldPointerValue + 0x1090;
	uintptr_t entityListPointerValue = 0;
	status = ReadPointer(targetProcess, entityListBase, &entityListPointerValue);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read entity list base\n");
		ObDereferenceObject(targetProcess);
		return status;

	}


	///////////////////////////////////////////////////////////////////////////////////////////////////
	VECTOR3 invViewTranslation;
	status = ReadMemory(targetProcess, cameraPtr + 0x2C, &invViewTranslation, sizeof(VECTOR3));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read inverted view translation (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);

		return status;
	}

	VECTOR3 invViewRight;
	status = ReadMemory(targetProcess, cameraPtr + 0x8, &invViewRight, sizeof(VECTOR3));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read inverted view right (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);

		return status;
	}

	VECTOR3 invViewUp;
	status = ReadMemory(targetProcess, cameraPtr + 0x14, &invViewUp, sizeof(VECTOR3));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read inverted view up (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);

		return status;
	}

	// inv view forward
	VECTOR3 invViewForward;
	status = ReadMemory(targetProcess, cameraPtr + 0x58, &invViewForward, sizeof(VECTOR3));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read inverted view forward (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);

		return status;
	}

	// inv view forward
	VECTOR3 viewPortSize;
	status = ReadMemory(targetProcess, cameraPtr + 0x20, &viewPortSize, sizeof(VECTOR3));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read view port size (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);

		return status;
	}

	VECTOR3 projectionD1;
	status = ReadMemory(targetProcess, cameraPtr + 0xD0, &projectionD1, sizeof(VECTOR3));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read projection D1 (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);

		return status;
	}

	VECTOR3 projectionD2;
	status = ReadMemory(targetProcess, cameraPtr + 0xDC, &projectionD2, sizeof(VECTOR3));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read projection D2 (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);

		return status;
	}
	INT32 entityCount = 0;
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// just read into entityCount each time
	status = ReadMemory(targetProcess, worldPointerValue + 0x1098, &entityCount, sizeof(entityCount));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read entity count (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}

	for (size_t i = 0; i < entityCount; i++) {
		uintptr_t entityEntryAddress = entityListPointerValue + i * sizeof(uintptr_t);
		uintptr_t entityPtr = 0;
		status = ReadPointer(targetProcess, entityEntryAddress, &entityPtr);
		if (!NT_SUCCESS(status) || entityPtr == 0) {
			continue;
		}

		// see if its a zombie
		// variable to distuingish if client wants zombies rendered

		uintptr_t visualStateAddress = entityPtr + 0x1D0;
		uintptr_t visualStatePtr = 0;
		status = ReadPointer(targetProcess, visualStateAddress, &visualStatePtr);
		if (!NT_SUCCESS(status) || visualStatePtr == 0) {
			continue;
		}
		LARGE_INTEGER interval;
		interval.QuadPart = -10000 * 100; // Time in 100ns units, negative value indicates sleep
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
		// Read cords into regular vector 3 LONG struct aswell
		VECTOR3 rawCords;
		status = ReadMemory(targetProcess, visualStatePtr + 0x2C, &rawCords, sizeof(rawCords));
		if (NT_SUCCESS(status)) {
			DbgPrintEx(0, 0, "Raw Cords (LONGS): X=%ld, Y=%ld, Z=%ld\n", rawCords.x, rawCords.y, rawCords.z);
		}

		//ULONG VERSION
		VECTOR3_RAW rawCoords;
		status = ReadMemory(targetProcess, visualStatePtr + 0x2C, &rawCoords, sizeof(rawCoords));

		if (NT_SUCCESS(status)) {
			DbgPrintEx(0, 0, "Raw Coords (Read as ULONGs): X=%lu, Y=%lu, Z=%lu\n",
				rawCoords.x, rawCoords.y, rawCoords.z);
		}
		//NTSTATUS WriteSharedStructInfo(HANDLE targetPid, PVOID userStructAddress, LONG xd, LONG xy, LONG xz, ULONG64 entityPointer, ULONG64 localPlayerPtr, LONG invertedx, LONG invertedy, LONG invertedz, LONG invertedviewrightx, LONG invertedviewrighty, LONG invertedviewrightz, LONG invertedviewforwardx, LONG invertedviewforwardy, LONG invertedviewforwardz, LONG viewportsizex, LONG viewportsizey, LONG viewportsizez, LONG projectiond1x, LONG projectiond1y, LONG projectiond1z, LONG projectiond2x, LONG projectiond2y, LONG projectiond2z) 
		WriteSharedStructEspInformation((HANDLE)gPID, (PVOID)gBaseAddr, rawCoords.x, rawCoords.y, rawCoords.z, entityPtr, localPlayerPtr, invViewTranslation.x, invViewTranslation.y, invViewTranslation.z, invViewRight.x, invViewRight.y, invViewRight.z, invViewUp.x, invViewUp.y, invViewUp.z, invViewForward.x, invViewForward.y, invViewForward.z, viewPortSize.x, viewPortSize.y, viewPortSize.z, projectionD1.x, projectionD1.y, projectionD1.z, projectionD2.x, projectionD2.y, projectionD2.z); // cords of the shared struct and the PID of the usermode applicat
	}
	ObDereferenceObject(targetProcess);
	return STATUS_SUCCESS;
}


// Needs testing
NTSTATUS SetPosition(uintptr_t Entity, char* positionData, HANDLE ProcessId)
{
	PEPROCESS Process;
	NTSTATUS status;
	SIZE_T bytes;

	// Look up the process by its ID
	status = PsLookupProcessByProcessId(ProcessId, &Process);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	// Write the 12 bytes of position data to the entity's memory at offset 0x1D0 + 0x2C
	status = MmCopyVirtualMemory(PsGetCurrentProcess(), positionData, Process,
		(PVOID)(Entity + 0x1D0 + 0x2C), 12, KernelMode, &bytes);

	// Clean up the process object reference
	ObDereferenceObject(Process);

	return status;
}

NTSTATUS TelportCheat(uintptr_t entityPtr)
{
	if (gPIDDayZ == 0) {
		DbgPrintEx(0, 0, "Invalid PID or BaseAddr. Ensure ReadTextFile() is called first.\n");
		return STATUS_INVALID_PARAMETER;
	}

	PEPROCESS targetProcess;
	NTSTATUS status = PsLookupProcessByProcessId((HANDLE)gPID, &targetProcess);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to get target process for PID: %d (Status: 0x%X)\n", gPID, status);
		return status;
	}

	// Buffer for position data (3 floats = 12 bytes)
	char positionData[12] = { 0 };
	SIZE_T bytesRead;

	// Read position data from the target process
	status = MmCopyVirtualMemory(targetProcess, gBaseAddr, PsGetCurrentProcess(),
		positionData, sizeof(positionData), KernelMode, &bytesRead);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read position data from the process for teleportation\n");
		return STATUS_ABANDONED;
	}

	// Debug output to confirm successful read
	DbgPrintEx(0, 0, "Position data read successfully, BytesRead=%zu\n", bytesRead);

	// Set the entity's position using the raw byte data
	status = SetPosition(entityPtr, positionData, (HANDLE)gPID);
	return status;
}

NTSTATUS ReadFromTextFile2() {
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

		// Find the first newline character to separate the lines
		while (*line != '\0') {
			if (*line == '\n') {
				*line = '\0';  // Null-terminate the first line
				break;

			}
			line++;
		}

		// Convert PID string to integer
		ULONG tempPid = 0;
		status = RtlCharToInteger(pidStr, 10, &tempPid);
		if (NT_SUCCESS(status)) {
			gPIDDayZ = (int)tempPid;
		}
		else {
			DbgPrintEx(0, 0, "Failed to convert PID: %s\n", pidStr);
			ZwClose(fileHandle);
			return status;
		}


		DbgPrintEx(0, 0, "[ReadTextFile] PID DayZ: %i\n", gPIDDayZ);

	}

	// Close the file
	ZwClose(fileHandle);

	return STATUS_SUCCESS;
}


NTSTATUS ReadFromTextFile() {
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
		// Convert PID string to integer
		ULONG tempPid = 0;
		status = RtlCharToInteger(pidStr, 10, &tempPid);
		if (NT_SUCCESS(status)) {
			gPID = (int)tempPid;
		}
		/*
		* bool DayZUtil::isPointerValid(QWORD ptr) {
	if (ptr > 0x200000001 && ptr < 0xffffffff00000000)
		return true;
	return false;
}
		*/
		else {
			DbgPrintEx(0, 0, "Failed to convert PID: %s\n", pidStr);
			ZwClose(fileHandle);
			return status;
		}
		// For BaseAddr - manual hex conversion
		ULONG_PTR addr = 0;
		char* p = baseAddrStr;
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;  // Skip "0x" if present
		while (*p) {
			addr *= 16;
			if (*p >= '0' && *p <= '9')
				addr += *p - '0';
			else if (*p >= 'a' && *p <= 'f')
				addr += *p - 'a' + 10;
			else if (*p >= 'A' && *p <= 'F')
				addr += *p - 'A' + 10;
			p++;
		}
		gBaseAddr = (PVOID)addr;

		DbgPrintEx(0, 0, "[ReadTextFile] PID: %i\n", gPID);
		DbgPrintEx(0, 0, "[ReadTextFile] BaseAddr: %llx\n", gBaseAddr);
	}

	// Close the file
	ZwClose(fileHandle);

	return STATUS_SUCCESS;
}

void memeHexConversion(char* baseAddrStr, PVOID& output) {
	// For BaseAddr - manual hex conversion
	ULONG_PTR addr = 0;
	char* p = baseAddrStr;
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;  // Skip "0x" if present
	while (*p) {
		addr *= 16;
		if (*p >= '0' && *p <= '9')
			addr += *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			addr += *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			addr += *p - 'A' + 10;
		p++;
	}
	output = (PVOID)addr;
}

NTSTATUS GetProcessImageBaseAddress(HANDLE ProcessId, PVOID* ImageBase)
{
	PEPROCESS Process = NULL;
	NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &Process);
	if (!NT_SUCCESS(status))
		return status;

	// PsGetProcessSectionBaseAddress is an undocumented function that returns
	// the base address of the process’s main module.
	*ImageBase = PsGetProcessSectionBaseAddress(Process);
	DbgPrintEx(0, 0, "%llx", *ImageBase);
	// Always dereference when done.
	ObDereferenceObject(Process);
	return STATUS_SUCCESS;
}

NTSTATUS ReadStructFromProcess() {
	if (gPID == 0 || gBaseAddr == NULL) {
		DbgPrintEx(0, 0, "Invalid PID or BaseAddr. Ensure ReadTextFile() is called first.\n");
		return STATUS_INVALID_PARAMETER;
	}

	PEPROCESS targetProcess;
	NTSTATUS status = PsLookupProcessByProcessId((HANDLE)gPID, &targetProcess);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to get target process for PID: %d (Status: 0x%X)\n", gPID, status);
		return status;
	}
	
	SHARED_DATA structData = { 0, 0, 0, 0, 0 };
	SIZE_T bytesRead;
	//hi
	status = MmCopyVirtualMemory(targetProcess, gBaseAddr, PsGetCurrentProcess(), &structData, sizeof(SHARED_DATA), KernelMode, &bytesRead);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read struct from process (Status: 0x%X)\n", status);
	}
	else {
		DbgPrintEx(0, 0, "Read Struct: X=%ld, Y=%ld, Z=%ld, BaseAddr=0x%llx, PID=%ld\n",
			structData.x, structData.y, structData.z);
		int x = 0;  // Declare x outside the loop
		while (true) {
			status = MmCopyVirtualMemory(targetProcess, gBaseAddr, PsGetCurrentProcess(), &structData, sizeof(SHARED_DATA), KernelMode, &bytesRead);
			if (!NT_SUCCESS(status)) {
				DbgPrintEx(0, 0, "Failed to read struct from process (Status: 0x%X)\n", status);
			}
			// delete logic and add a sleep in UM start the program, map the driver, driver reads from struct and sleep, press enter once dayz started, driver reads dayz, boom
			DbgPrintEx(0, 0, "%lld", structData.y); // this is slightly broken, appears as 0 doesnt detect game
			if (structData.y == 1 && x < 2) { //  means game has started, and esp is the selected cheat
				LARGE_INTEGER interval;
				interval.QuadPart = -10000 * 10000; // Time in 100ns units, negative value indicates sleep
				KeDelayExecutionThread(KernelMode, FALSE, &interval);
				DbgPrintEx(0, 0, "Game Started!");
				x = 3; // Prevent re-entering
				// ************************************* RETRIEVE GLOBAL BASE ADDRESS VALUES HERE, FROM FILE OR STRUCT 
				ReadFromTextFile2(); // gets dayz base address
				GetProcessImageBaseAddress((HANDLE)gPIDDayZ, &gBaseAddrDayZ);
				//status = noGrass();
				//status = sendInformation(); // gets radar
				//break; // Ensure this loop exits
			}
			if (structData.y == 0) {
				LARGE_INTEGER interval;
				interval.QuadPart = -10000 * 10000; // Time in 100ns units, negative value indicates sleep
				KeDelayExecutionThread(KernelMode, FALSE, &interval);
				DbgPrintEx(0, 0, "Game not started yet");
				continue;
			}

			if (structData.option == 10) {
				status = noGrass();
			}

			if (structData.option == 11) {
				status = sendEspInformation(); 
			}
			if (structData.option == 12) {
				status = sendRadarInformation();
			}
			/*
			if (structData.option == 13) {
				status = fullBright();
			}*/
		}// without battle-eye: run um process -> write its shit into logging file -> launch dayz -> get its base addr and process -> change that in the driver -> map driver with UM process open -> um process maps entities

	// um just needs to start, write its info to file, load dayz, gt its info into file, set status to 1, render cords
	}
	// read from one of these as a status value, and if it changes to 1 then grab base addr from struct or file (1 means game is launched now) - !MAKE SURE TO BREAK FROM LOOP^^^
	ObDereferenceObject(targetProcess);
	return status;
}

NTSTATUS DriverEntryCustom() {

	// Delay
	ReadFromTextFile();
	ReadStructFromProcess();
	DbgPrintEx(0, 0, "Driver Started\n");
	/*
	NTSTATUS status = getAssets();
	if (NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Assets retrieved successfully.\n");
		return STATUS_SUCCESS;
	}
	else {
		return STATUS_ABANDONED;
	}*/

	return STATUS_SUCCESS;
}