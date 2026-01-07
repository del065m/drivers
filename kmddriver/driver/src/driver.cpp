#include <ntifs.h>
#include <ntddkbd.h>
#include "utils.h"
#include <intrin.h>
#include <ntstrsafe.h>

#define DEVICE_NAME L"\\Device\\TestDriver"
#define SYMLINK_NAME L"\\DosDevices\\TestDriver"

#define IOCTLD_BUGCHECK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_BUGCHECK5 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_TERMINATE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_READ_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_WRITE_FILE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_READ_VIRTUAL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_FREEZE_SYSTEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_BREAK_TIMER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_SET_TIME CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_SHUTDOWN_SYSTEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_A CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_WRITE_FILE2 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_DELETE_SYMLINK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_LIST_DIRECTORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTLD_CREATE_SYMLINK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS)

extern "C" {
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName, PDRIVER_INITIALIZE InitializationFunction);
	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress, PEPROCESS TargetProcess, PVOID TargetAddress, SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T ReturnSize);
	PVOID PoSetPowerButtonHoldState(char arg1);
	typedef enum _SHUTDOWN_ACTION {
		ShutdownNoReboot = 0,
		ShutdownReboot = 1,
		ShutdownPowerOff = 2,
	} SHUTDOWN_ACTION;
	NTSTATUS NtShutdownSystem(SHUTDOWN_ACTION Action);

}

struct ST_FILE_READ {
	WCHAR FilePath[260];
	LARGE_INTEGER Offset;
	ULONG Size;
};
enum EN_FILE_WRITE_TYPE {
	FileWriteOverwrite = 0,
	FileWriteAppend = 1,
	FileWriteOverwriteDevice = 2
};
struct ST_FILE_WRITE {
	WCHAR FilePath[260];
	LARGE_INTEGER Offset;
	ULONG DataSize;
	EN_FILE_WRITE_TYPE WriteType;
};

struct ST_READ_VIRTUAL {
	HANDLE ProcessId;
	UINT_PTR Address;
	SIZE_T Size;
};
struct ST_DISK_CLEAR {
	wchar_t FilePath[260];
	LARGE_INTEGER StartOffset;
	ULONGLONG SectorCount;
	ULONG SectorSize;
};

typedef struct _DIRECTORY_ENUM_INPUT {
	WCHAR Path[260];
} DIRECTORY_ENUM_INPUT, *PDIRECTORY_ENUM_INPUT;
typedef struct _DIRECTORY_ENTRY {
	WCHAR Name[256];
	BOOLEAN IsDirectory;
} DIRECTORY_ENTRY, *PDIRECTORY_ENTRY;
typedef struct _DIRECTORY_ENUM_OUTPUT {
	ULONG EntryCount;
	DIRECTORY_ENTRY Entries[1];
} DIRECTORY_ENUM_OUTPUT, * PDIRECTORY_ENUM_OUTPUT;

typedef struct _SYMLINK_CREATE_INFO {
	WCHAR SymlinkPath[260];
	WCHAR TargetPath[260];
} SYMLINK_CREATE_INFO, *PSYMLINK_CREATE_INFO;

NTSTATUS EnumerateDirectory(PWCHAR DirectoryPath, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG BytesReturned) {
	NTSTATUS status;
	HANDLE hDirectory = NULL;
	OBJECT_ATTRIBUTES objAttr;
	UNICODE_STRING uniPath;
	IO_STATUS_BLOCK ioStatus;
	PVOID buffer = NULL;
	ULONG bufferSize = 4096;
	PDIRECTORY_ENUM_OUTPUT output = (PDIRECTORY_ENUM_OUTPUT) OutputBuffer;
	ULONG entryCount = 0;
	ULONG maxEntries = (OutputBufferLength - sizeof(ULONG)) / sizeof(DIRECTORY_ENTRY);

	RtlInitUnicodeString(&uniPath, DirectoryPath);

	InitializeObjectAttributes(&objAttr, &uniPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	status = ZwOpenFile(&hDirectory, FILE_LIST_DIRECTORY | SYNCHRONIZE,
		&objAttr, &ioStatus, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);

	if (!NT_SUCCESS(status)) return status;

	buffer = ExAllocatePool2(POOL_FLAG_PAGED, bufferSize, 'tsiL');
	if (!buffer) {
		ZwClose(hDirectory);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	while (NT_SUCCESS(status)) {
		status = ZwQueryDirectoryFile(hDirectory, NULL, NULL, NULL,
			&ioStatus, buffer, bufferSize, FileDirectoryInformation,
			FALSE, NULL, entryCount == 0);
		if (status == STATUS_NO_MORE_FILES) {
			status = STATUS_SUCCESS;
			break;
		}
		if (!NT_SUCCESS(status)) {
			break;
		}

		PFILE_DIRECTORY_INFORMATION dirInfo = (PFILE_DIRECTORY_INFORMATION) buffer;

		while (TRUE) {
			if (entryCount >= maxEntries) {
				status = STATUS_BUFFER_TOO_SMALL;
				goto Cleanup;
			}

			ULONG nameLength = dirInfo->FileNameLength / sizeof(WCHAR);
			if (nameLength > 255) nameLength = 255;

			RtlCopyMemory(output->Entries[entryCount].Name,
				dirInfo->FileName, nameLength * sizeof(WCHAR));
			output->Entries[entryCount].Name[nameLength] = L'\0';

			output->Entries[entryCount].IsDirectory =
				(dirInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;

			entryCount++;

			if (dirInfo->NextEntryOffset == 0) break;

			dirInfo = (PFILE_DIRECTORY_INFORMATION) ((PUCHAR) dirInfo + dirInfo->NextEntryOffset);
		}
	}

Cleanup:
	if (buffer) ExFreePoolWithTag(buffer, 'tsiL');
	if (hDirectory) ZwClose(hDirectory);
	if (NT_SUCCESS(status) || status == STATUS_BUFFER_TOO_SMALL) {
		output->EntryCount = entryCount;
		*BytesReturned = sizeof(ULONG) + (entryCount * sizeof(DIRECTORY_ENTRY));
	}

	return status;
}

VOID ShutdownThread(PVOID Context) {
	ULONG action = *(ULONG*) Context;
	NtShutdownSystem((SHUTDOWN_ACTION) action);
	PsTerminateSystemThread(STATUS_SUCCESS);
}

typedef enum _FILE_WRITE_MODE {
	WriteMode_Overwrite = 0,
	WriteMode_Append = 1,
	WriteMode_Supersede = 2
} FILE_WRITE_MODE;
typedef struct _ST_FILE_WRITE2 {
	WCHAR NtPath[260];
	ULONG DataSize;
	FILE_WRITE_MODE WriteMode;
	BOOLEAN IsDevice;
} ST_FILE_WRITE2, *PST_FILE_WRITE2;

NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);



	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	ULONG bytesReturned = 0;

	ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	PVOID buffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG inLen = stack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
	DbgPrint("TestDriver: DeviceControl called with code 0x%08X\n", controlCode);

	switch (controlCode) {
	case IOCTLD_BUGCHECK:
	{
		if (inLen >= sizeof(ULONG)) {
			ULONG* code = (ULONG*) buffer;
			KeBugCheckEx(*code, 0, 0, 0, 0);






		} else {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		bytesReturned = 0;
		break;
	}
	case IOCTLD_BUGCHECK5:
	{
		if (inLen >= sizeof(ULONG)) {
			ULONG* p = (ULONG*) buffer;
			ULONG code = p[0];
			ULONG param1 = p[1];
			ULONG param2 = p[2];
			ULONG param3 = p[3];
			ULONG param4 = p[4];
			KeBugCheckEx(code, param1, param2, param3, param4);
		} else {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		bytesReturned = 0;
		break;
	}
	case IOCTLD_TERMINATE_PROCESS:
	{
		if (inLen >= sizeof(ULONG)) {
			ULONG* pid = (ULONG*) buffer;

			OBJECT_ATTRIBUTES objAttrs;
			CLIENT_ID clientId;
			HANDLE procHandle = nullptr;
			InitializeObjectAttributes(&objAttrs, nullptr, 0, nullptr, nullptr);
			clientId.UniqueProcess = (HANDLE) (ULONG_PTR) (*pid);
			clientId.UniqueThread = nullptr;

			status = ZwOpenProcess(&procHandle, GENERIC_ALL, &objAttrs, &clientId);
			if (NT_SUCCESS(status)) {
				status = ZwTerminateProcess(procHandle, STATUS_PROCESS_IS_TERMINATING);
				ZwClose(procHandle);
			}
		} else {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		break;
	}
	case IOCTLD_READ_FILE:
	{
		if (inLen >= sizeof(ST_FILE_READ)) {
			ST_FILE_READ in = *(ST_FILE_READ*) buffer;
			if (outLen < in.Size) {
				status = STATUS_BUFFER_TOO_SMALL;
				goto Complete;
			}
			HANDLE fileHandle;
			OBJECT_ATTRIBUTES objAttr;
			IO_STATUS_BLOCK ioStatusBlock;
			UNICODE_STRING filePath;
			LARGE_INTEGER byteOffset;
			PVOID tempBuffer = NULL;

			RtlInitUnicodeString(&filePath, in.FilePath);

			InitializeObjectAttributes(&objAttr, &filePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

			status = ZwCreateFile(&fileHandle, GENERIC_READ, &objAttr, &ioStatusBlock, NULL,
				FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT,
				NULL, 0);

			if (!NT_SUCCESS(status)) {
				goto Complete;
			}

			tempBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED | POOL_FLAG_UNINITIALIZED, in.Size, 'rFdT');

			if (!tempBuffer) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				goto CloseFile;
			}

			byteOffset = in.Offset;
			status = ZwReadFile(
				fileHandle,
				NULL,
				NULL,
				NULL,
				&ioStatusBlock,
				tempBuffer,
				in.Size,
				&byteOffset,
				NULL
			);

			if (NT_SUCCESS(status)) {
				bytesReturned = (ULONG) ioStatusBlock.Information;

				RtlCopyMemory(buffer, tempBuffer, bytesReturned);
			}
			ExFreePool(tempBuffer);

			CloseFile:
			ZwClose(fileHandle);
			goto Complete;
				
		} else {
			status = STATUS_BUFFER_TOO_SMALL;
		}
		break;

	}
	case IOCTLD_WRITE_FILE:
	{
		ST_FILE_WRITE* input;
		PVOID dataBuffer;
		ULONG totalSize;
		HANDLE fileHandle = NULL;
		OBJECT_ATTRIBUTES objAttr;
		UNICODE_STRING uniPath;
		IO_STATUS_BLOCK ioStatus;
		ULONG createDisposition;
		LARGE_INTEGER writeOffset;
		FILE_STANDARD_INFORMATION fileInfo;

		if (inLen < sizeof(ST_FILE_WRITE)) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		input = (ST_FILE_WRITE*) buffer;
		totalSize = sizeof(ST_FILE_WRITE) + input->DataSize;

		if (inLen < totalSize) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		dataBuffer = (PVOID) ((PUCHAR) input + sizeof(ST_FILE_WRITE));

		RtlInitUnicodeString(&uniPath, input->FilePath);
		InitializeObjectAttributes(&objAttr, &uniPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		if (input->WriteType == FileWriteOverwrite) {
			createDisposition = FILE_OVERWRITE_IF;
			writeOffset = input->Offset;
		} else if (input->WriteType == FileWriteAppend) {
			createDisposition = FILE_OPEN_IF;
			writeOffset.QuadPart = 0;
		} else {
			status = STATUS_INVALID_PARAMETER;
			//KeBugCheckEx(0x12340000, 0, 0, 0, 0);
			break;
		}
		//KeBugCheckEx(0x12340001, 0, 0, 0, 0);

		if (input->WriteType != FileWriteOverwriteDevice) {
			status = ZwCreateFile(&fileHandle, FILE_GENERIC_WRITE, &objAttr, &ioStatus, NULL,
				FILE_ATTRIBUTE_NORMAL, 0, createDisposition, FILE_SYNCHRONOUS_IO_NONALERT,
				NULL, 0);
		} else  {
			status = ZwCreateFile(&fileHandle,
				FILE_READ_DATA | FILE_WRITE_DATA | SYNCHRONIZE,
				&objAttr,
				&ioStatus,
				NULL,
				0,  // No file attributes for devices
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				FILE_OPEN,
				FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_NO_INTERMEDIATE_BUFFERING,
				NULL,
				0);
		}

		if (!NT_SUCCESS(status)) {
			break;
		}

		if (input->WriteType == FileWriteAppend) {
			status = ZwQueryInformationFile(fileHandle, &ioStatus, &fileInfo, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation);
			if (!NT_SUCCESS(status)) {
				ZwClose(fileHandle);
				break;
			}
			writeOffset = fileInfo.EndOfFile;
		}

	//	KeBugCheckEx(0, 0, 0, 0, 0);

		if (input->WriteType == FileWriteOverwriteDevice)
			writeOffset = input->Offset;

		status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatus, dataBuffer, input->DataSize, &writeOffset, NULL);
		
		ZwClose(fileHandle);
		break;
	}
	case IOCTLD_READ_VIRTUAL:
	{
		if (inLen < sizeof(ST_READ_VIRTUAL)) {
			status = STATUS_BUFFER_TOO_SMALL;
			goto Complete;
		}

		ST_READ_VIRTUAL* input = reinterpret_cast<ST_READ_VIRTUAL*>(buffer);
		SIZE_T sizeToRead = input->Size;

		if (outLen < sizeToRead) {
			status = STATUS_BUFFER_TOO_SMALL;
			goto Complete;
		}

		PEPROCESS srcProcess = nullptr;
		PEPROCESS tgtProcess = PsGetCurrentProcess();

		status = PsLookupProcessByProcessId(input->ProcessId, &srcProcess);
		if (!NT_SUCCESS(status)) {
			goto Complete;
		}

		SIZE_T bytesCopied = 0;
		status = MmCopyVirtualMemory(srcProcess, reinterpret_cast<PVOID>(input->Address),
			tgtProcess, buffer, sizeToRead, KernelMode, &bytesCopied);

		ObDereferenceObject(srcProcess);

		if (NT_SUCCESS(status)) {
			bytesReturned = (ULONG)bytesCopied;
		}
		break;
	}
	case IOCTLD_FREEZE_SYSTEM:
	{
		if (inLen < sizeof(ULONG)) {
			KIRQL oldIrql;
			KeRaiseIrql(HIGH_LEVEL, &oldIrql);
			for (;;) {}
		} else {
			ULONG count = *(ULONG*) buffer;
			for (volatile ULONG i = 0; i < count; i++) {
				KeStallExecutionProcessor(1);
			}
		}
		break;
	}
	case IOCTLD_BREAK_TIMER:
	{
		status = PsSetCreateProcessNotifyRoutineEx([](PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
			UNREFERENCED_PARAMETER(Process);
			UNREFERENCED_PARAMETER(ProcessId);
			if (CreateInfo) CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
			}, FALSE);
		break;
	}
	case IOCTLD_SET_TIME:
	{
		if (inLen < sizeof(LARGE_INTEGER)) {
			status = STATUS_BUFFER_TOO_SMALL;
			goto Complete;
		}
		LARGE_INTEGER newTime = *(LARGE_INTEGER*)buffer;
		LARGE_INTEGER oldTime;

		status = fn::ZwSetSystemTime(&newTime, &oldTime);
		break;
	}
	case IOCTLD_SHUTDOWN_SYSTEM:
	{
		if (inLen < sizeof(SHUTDOWN_ACTION)) {
			status = STATUS_BUFFER_TOO_SMALL;
			goto Complete;
		}
		SHUTDOWN_ACTION shutdownAction = *(SHUTDOWN_ACTION*) buffer;
		if (!(shutdownAction == ShutdownPowerOff || shutdownAction == ShutdownReboot || shutdownAction == ShutdownNoReboot)) {
			status = STATUS_INVALID_PARAMETER;
			goto Complete;
		}

		HANDLE threadHandle;
		ULONG action = *(ULONG*) buffer;
		status = PsCreateSystemThread(&threadHandle, THREAD_ALL_ACCESS, NULL, NULL, NULL, ShutdownThread, &action);
		if (NT_SUCCESS(status))
			ZwClose(threadHandle);

		//status = NtShutdownSystem(shutdownAction);
		break;
	}
	case IOCTLD_A:
	{

		ST_DISK_CLEAR* input;
		UNICODE_STRING uniPath;
		OBJECT_ATTRIBUTES objAttr;
		IO_STATUS_BLOCK ioStatus;
		HANDLE fileHandle = NULL;
		ULONG sectorSize;
		ULONGLONG sectorsToClear;
		LARGE_INTEGER offset;
		void* zeroBuffer;
		IO_STATUS_BLOCK writeStatus;
		ULONGLONG i;

		if (inLen < sizeof(ST_DISK_CLEAR)) {
			status = STATUS_INVALID_PARAMETER;
			goto Complete;
		}

		input = (ST_DISK_CLEAR*) buffer;

		RtlInitUnicodeString(&uniPath, input->FilePath);
		InitializeObjectAttributes(&objAttr, &uniPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		status = ZwCreateFile(&fileHandle, FILE_GENERIC_WRITE, &objAttr, &ioStatus, NULL, 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN,
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE|FILE_NO_INTERMEDIATE_BUFFERING|FILE_WRITE_THROUGH,
			NULL, 0);
		if (!NT_SUCCESS(status))
			goto Complete;

		sectorSize = input->SectorSize;
		sectorsToClear = input->SectorCount;
		offset = input->StartOffset;

		zeroBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, sectorSize, 'clr0');
		if (!zeroBuffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			goto CloseHandle;
		}
		RtlZeroMemory(zeroBuffer, sectorSize);

		i = 0;
		while (i < sectorsToClear) {
			status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &writeStatus,
				zeroBuffer, sectorSize, &offset, NULL);
			if (!NT_SUCCESS(status))
				break;

			offset.QuadPart += sectorSize;
			i++;
		}

		ExFreePool(zeroBuffer);

	CloseHandle:
		ZwClose(fileHandle);
		break;






















	}
	case IOCTLD_WRITE_FILE2:
	{
		PST_FILE_WRITE2 req;
		HANDLE hFile;
		OBJECT_ATTRIBUTES objAttr;
		IO_STATUS_BLOCK ioStatus;
		UNICODE_STRING filePath;
		ULONG createDisposition;
		ULONG createOptions;
		ULONG desiredAccess;
		LARGE_INTEGER byteOffset;
		FILE_STANDARD_INFORMATION fileInfo;
		PVOID dataBuffer;
		req = (PST_FILE_WRITE2) buffer;

		if (inLen < sizeof(ST_FILE_WRITE2) + req->DataSize) {
			status = STATUS_BUFFER_TOO_SMALL;
			goto Complete;
		}

		req->NtPath[259] = L'\n';
		dataBuffer = (PUCHAR) buffer + sizeof(ST_FILE_WRITE2);

		RtlInitUnicodeString(&filePath, req->NtPath);

		createDisposition = (req->WriteMode == WriteMode_Overwrite) ? FILE_OVERWRITE_IF : ((req->WriteMode == WriteMode_Supersede) ? FILE_SUPERSEDE : FILE_OPEN_IF);

		if (req->IsDevice) {
			createOptions = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NO_INTERMEDIATE_BUFFERING;
			desiredAccess = FILE_WRITE_DATA | SYNCHRONIZE;
		} else {
			createOptions = FILE_SYNCHRONOUS_IO_NONALERT;
			if (req->WriteMode == WriteMode_Supersede) {
				desiredAccess = FILE_WRITE_DATA | SYNCHRONIZE | DELETE;
			} else {
				desiredAccess = FILE_APPEND_DATA | SYNCHRONIZE | FILE_READ_ATTRIBUTES;
			}
		}

		InitializeObjectAttributes(&objAttr, &filePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		status = ZwCreateFile(&hFile, desiredAccess, &objAttr, &ioStatus, NULL,
			FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			createDisposition, createOptions, NULL, 0);

		if (!NT_SUCCESS(status)) goto Complete;

		byteOffset.QuadPart = 0;

		if (req->WriteMode == WriteMode_Append && !req->IsDevice) {
			status = ZwQueryInformationFile(hFile, &ioStatus, &fileInfo,
				sizeof(FILE_STANDARD_INFORMATION),
				FileStandardInformation);
			if (NT_SUCCESS(status)) {
				byteOffset.QuadPart = fileInfo.EndOfFile.QuadPart;
			}
		}

		status = ZwWriteFile(hFile, NULL, NULL, NULL, &ioStatus, dataBuffer,
			req->DataSize, &byteOffset, NULL);

		ZwClose(hFile);
		break;
	}
	case IOCTLD_DELETE_SYMLINK:
	{
		UNICODE_STRING symlinkName;
		PWCHAR symlinkPath;

		if (inLen < sizeof(WCHAR)) {
			status = STATUS_BUFFER_TOO_SMALL;
			goto Complete;
		}

		symlinkPath = (PWCHAR) buffer;
		RtlInitUnicodeString(&symlinkName, symlinkPath);

		status = IoDeleteSymbolicLink(&symlinkName);
		break;
	}
	case IOCTLD_CREATE_SYMLINK:
	{
		UNICODE_STRING symlinkName;
		UNICODE_STRING targetName;
		PSYMLINK_CREATE_INFO symlinkInfo;

		if (inLen < sizeof(SYMLINK_CREATE_INFO)) {
			status = STATUS_BUFFER_TOO_SMALL;
			goto Complete;
		}

		symlinkInfo = (PSYMLINK_CREATE_INFO) buffer;

		RtlInitUnicodeString(&symlinkName, symlinkInfo->SymlinkPath);
		RtlInitUnicodeString(&targetName, symlinkInfo->TargetPath);

		status = IoCreateSymbolicLink(&symlinkName, &targetName);
		break;
	}
	case IOCTLD_LIST_DIRECTORY:
	{
		PDIRECTORY_ENUM_INPUT input = (PDIRECTORY_ENUM_INPUT) buffer;
		PVOID output = buffer;
		if (inLen < sizeof(DIRECTORY_ENUM_INPUT) ||
			outLen < sizeof(DIRECTORY_ENUM_OUTPUT)) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		input->Path[259] = L'\0';

		status = EnumerateDirectory(input->Path, output, outLen, &bytesReturned);
		break;
	}
	default: {
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	}

	Complete:
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = bytesReturned;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}
NTSTATUS DeviceClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	DbgPrint("TestDriver: DeviceClose\n");
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS DeviceCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	DbgPrint("TestDriver: DeviceCreate\n");
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symbolicLink;
	DbgPrint("TestDriver: DriverUnload\n");
	RtlInitUnicodeString(&symbolicLink, SYMLINK_NAME);
	IoDeleteSymbolicLink(&symbolicLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	DbgPrint("TestDriver: Driver unloaded\n");
}



NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);


	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject = NULL;
	UNICODE_STRING deviceName, symbolicLink;

	DbgPrint("TestDriver: DriverEntry\n");

	RtlInitUnicodeString(&deviceName, DEVICE_NAME);
	status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 
		FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("TestDriver: Failed to create device (0x%08X)\n", status);
		return status;}
	RtlInitUnicodeString(&symbolicLink, SYMLINK_NAME);
	status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
	if (!NT_SUCCESS(status)) {
		DbgPrint("TestDriver: Failed to create symbolic link (0x%08X)\n", status);
		IoDeleteDevice(DeviceObject);
		return status;}
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
	DriverObject->DriverUnload = DriverUnload;


	DbgPrint("TestDriver: Driver loaded successfully\n");
















	//NtShutdownSystem(ShutdownReboot);

	return STATUS_SUCCESS;
}