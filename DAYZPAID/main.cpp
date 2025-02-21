#include <ntifs.h>
#include <wdm.h>
#pragma comment(lib, "ntoskrnl.lib")

#define world 0x41B32A0
#define localplayer 0x2968
#define gameBase 0x7ff6d3a90000

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
} SHARED_DATA, * PSHARED_DATA;

int gPID;
PVOID gBaseAddr;

NTSTATUS SleepInKernelMode(ULONG milliseconds) {
	LARGE_INTEGER interval;
	interval.QuadPart = -10000 * milliseconds; // Time in 100ns units, negative value indicates sleep
	KeDelayExecutionThread(KernelMode, FALSE, &interval);
	return STATUS_SUCCESS;
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
		// Convert PID string to integer
		ULONG tempPid = 0;
		status = RtlCharToInteger(pidStr, 10, &tempPid);
		if (NT_SUCCESS(status)) {
			gPID = (int)tempPid;
		}
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

	SHARED_DATA structData = { 0 };
	SIZE_T bytesRead;

	status = MmCopyVirtualMemory(targetProcess, gBaseAddr, PsGetCurrentProcess(), &structData, sizeof(SHARED_DATA), KernelMode, &bytesRead);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read struct from process (Status: 0x%X)\n", status);
	}
	else {
		DbgPrintEx(0, 0, "Read Struct: X=%ld, Y=%ld, Z=%ld, BaseAddr=0x%llx, PID=%ld\n",
			structData.x, structData.y, structData.z);
	}

	ObDereferenceObject(targetProcess);
	return status;
}



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
	uintptr_t worldPointerAddress = gameBase + world; // Game base address + world offset - here
	uintptr_t worldPointerValue = 0;

	NTSTATUS status = PsLookupProcessByProcessId((HANDLE)1337, &targetProcess); // DayZ PID here
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to get target process\n");
		return status;
	}

	status = ReadPointer(targetProcess, worldPointerAddress, &worldPointerValue);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read world pointer (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}

	INT32 entityCount = 0;
	status = ReadMemory(targetProcess, worldPointerValue + 0xF50, &entityCount, sizeof(entityCount));
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read entity count (Status: 0x%X)\n", status);
		ObDereferenceObject(targetProcess);
		return status;
	}
	DbgPrintEx(0, 0, "Entity Count: %u\n", entityCount);

	uintptr_t entityListBase = worldPointerValue + 0xF48;
	uintptr_t entityListPointerValue = 0;
	status = ReadPointer(targetProcess, entityListBase, &entityListPointerValue);
	if (!NT_SUCCESS(status)) {
		DbgPrintEx(0, 0, "Failed to read entity list base\n");
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
		}

		//ULONG VERSION
		VECTOR3_RAW rawCoords;
		status = ReadMemory(targetProcess, visualStatePtr + 0x2C, &rawCoords, sizeof(rawCoords));

		if (NT_SUCCESS(status)) {
			DbgPrintEx(0, 0, "Raw Coords (Read as ULONGs): X=%lu, Y=%lu, Z=%lu\n",
				rawCoords.x, rawCoords.y, rawCoords.z);
		}

		WriteSharedStructCoords((HANDLE)18584, (PVOID)0x00007FF6F75CD000, rawCoords.x, rawCoords.y, rawCoords.z);
	}

	ObDereferenceObject(targetProcess);
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntryCustom() {
	// Delay
	ReadTextFile();
	SleepInKernelMode(5000);
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
