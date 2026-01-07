#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include "helper.h"
#include "randomutils.h"
#include <map>

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

#define DEVICE_PATH "\\\\.\\TestDriver"

typedef struct _DIRECTORY_ENUM_INPUT {
	WCHAR Path[260];
} DIRECTORY_ENUM_INPUT, * PDIRECTORY_ENUM_INPUT;
typedef struct _DIRECTORY_ENTRY {
	WCHAR Name[256];
	BOOLEAN IsDirectory;
} DIRECTORY_ENTRY, * PDIRECTORY_ENTRY;
typedef struct _DIRECTORY_ENUM_OUTPUT {
	ULONG EntryCount;
	DIRECTORY_ENTRY Entries[1];
} DIRECTORY_ENUM_OUTPUT, * PDIRECTORY_ENUM_OUTPUT;




typedef enum _SHUTDOWN_ACTION {
	ShutdownNoReboot = 0,
	ShutdownReboot = 1,
	ShutdownPowerOff = 2,
} SHUTDOWN_ACTION;
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
} ST_FILE_WRITE2, * PST_FILE_WRITE2;

typedef struct _SYMLINK_CREATE_INFO {
	WCHAR SymlinkPath[260];
	WCHAR TargetPath[260];
} SYMLINK_CREATE_INFO, * PSYMLINK_CREATE_INFO;

LARGE_INTEGER DateTimeToLargeInteger(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
	constexpr uint8_t daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	auto countLeapYears = [](uint16_t y, uint8_t m) -> int {
		int years = y - 1;
		return years / 4 - years / 100 + years / 400;
		};
	int days = 0;
	days += (year - 1601) * 365;
	days += countLeapYears(year, month) - countLeapYears(1601, 1);
	for (uint8_t i = 1; i < month; ++i)
		days += daysInMonth[i - 1];
	if (month > 2 && ((year % 400 == 0) || (year % 4 == 0 && year % 100 != 0)))
		days += 1;
	days += day - 1;
	uint64_t totalSeconds = static_cast<uint64_t>(days) * 86400 + hour * 3600 + minute * 60 + second;
	LARGE_INTEGER li;
	li.QuadPart = totalSeconds * 10000000ULL;
	return li;
}

void clsCmd() {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == INVALID_HANDLE_VALUE)
		return;

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
		return;

	DWORD consoleSize = csbi.dwSize.X * csbi.dwSize.Y;
	COORD topLeft = { 0, 0 };
	DWORD charsWritten;

	FillConsoleOutputCharacter(hConsole, ' ', consoleSize, topLeft, &charsWritten);
	FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, topLeft, &charsWritten);
	SetConsoleCursorPosition(hConsole, topLeft);
}

int main() {
	HANDLE hDevice;

	hDevice = CreateFileA(DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error opening driver device\n");
		//return 1;
	}

	int testValVal = 100;
	int* testVal = &testValVal;
	while (true) {
		printf("Command: ");
		CommandHelper cmdP = CommandHelper::fromCin();
		if (!cmdP.isNextWord()) {
			printf("No command\n");
			continue;
		}
		std::string cmd = cmdP.getNextWord();
		if (cmd == "exit") {
			printf("Exiting...\n");
			break;
		}
		else if (cmd == "help") {
			if (cmdP.isNextWord()) {
				std::string cmd2 = cmdP.getNextWord();
				if (cmd2 == "ntpath") {
					printf("NT style paths:\n");
					printf(" File from normal path: `\\??\\C:\\test\\test.txt`\n");
					printf(" Raw hard disk: `\\Device\\Harddisk0`\n");
					printf(" Raw partition: `\\Device\\Harddisk0\\Partition0`\n");
					printf(" From volume: `\\Device\\HarddiskVolume1\\Windows\\System32\\notepad.exe`\n");
					printf(" Other ones\n");
				} 
				else if (cmd2 == "redirectors" || cmd2 == "redirections") {
					printf("Redirectors do something with the output of something.\n");
					printf("List of redirectors\n");
					printf(" ` > file [wstring path]` - output to a file\n");
					printf(" ` > file+ [wstring+ path] - output to a file`\n");
					printf(" ` < file [wstring path]` - input from a file\n");
					printf(" ` < file+ [wstring+ path] - input from a file`\n");
					printf(" ` < null [uint32_t count]` - null data (0x00)\n");
				} 
				else if (cmd2 == "types") {
					printf("Types:\n");
					printf("string - text\n");
					printf("string+ - text, but fancy\n");
					printf("wstring - text\n");
					printf("wstring+ - text, but fancy\n");
					printf("uint8, uint16, uint32, uint64 - N bit numbers, not negative\n");
					printf("int8, int16, int32, int64 - N bit numbers, can be negative\n");
				} 
				else {
					printf("Unknown help command\n");
				}
				continue;
			}
			printf("Help:\n");
			printf(" !cls - clear screen\n");
			printf(" help (ntpath/redirectors/types) - different help pages\n");
			printf(" btests - tests for non-driver things\n");
			printf(" bugcheck [uint32 code] - bugcheck (bsod, no params)\n");
			printf(" bugcheck [uint32 code] [uint32 param1] [uint32 param2] [uint32 param3] [uint32 param4] - bugcheck (bsod, with params)\n");
			printf(" readFile [wstring+ path, max len 260] [uint64 offset] [uint32 length] (redirectors: file, file+) - read any NT style path (help ntpath), with a offset and size\n");
			printf(" writeFile [wstring+ path, max len 260] [uint64 offset] [overwrite/append/overwriteDevice] [redirectors: file, file+, null] - write any NT style path (help ntpath), with offset. use file redirector to specify data\n");
			printf(" terminateProcess [uint32 pid] - terminate a process\n");
			printf(" freezeSystem.1 - first variant of freeze system, works\n");
			printf(" systemBreakTimer - does not work\n");
			printf(" time [uint16 year] [uint8 month] [uint8 day] [uint8 hour] [uint8 minute] [uint8 second] - set system time, but the input isnt the same as what actually is set.\n");
			printf(" time2 [uint64 time] - set system time, in raw value\n");
			printf(" shutdown [reboot/noReboot/powerOff] - shutdown system\n");
			printf(" dfs - most likely doesnt work\n");
			printf(" writeFile2 [wstring+ path, max ~260] [overwrite/supersede/append/overwriteDevice] - a version of writeFile, but witchout a offset.\n");
			printf(" rmd [wstring+ path] - starts overwriting every file in the (NON NTPATH) path with 16 bytes of null, file list is from usermode\n");
			printf(" dsym [wstring symlink] - delete synlink (e.g. \"\\??\\C:\"\n");
			printf(" mksym [wstring symlinkPath] [wstring targetPath]\n");
		}
		else if (cmd == "!cls") {
			clsCmd();
			continue;
		}
		else if (cmd == "bugcheck") {
			if (!cmdP.isNextUInt32()) {
				printf("Missing/invalid code and/or params\n");
				continue;
			}
			uint32_t code = cmdP.getNextUInt32();
			if (!cmdP.hasNext()) {
				DWORD bytesReturned;
				BOOL result;
				result = DeviceIoControl(hDevice, IOCTLD_BUGCHECK, &code, sizeof(code), NULL, 0, &bytesReturned, NULL);

				if (result) {
					printf("Should blue screen\n");
					continue;
				} else {
					printf("IOCTL failed. Error: %d\n", GetLastError());
					continue;
				}
			} 
			else {
				if (!cmdP.isNextUInt32()) {
					printf("Wrong type for param1, should be uint32\n");
					continue;
				}
				uint32_t param1 = cmdP.getNextUInt32();
				if (!cmdP.isNextUInt32()) {
					printf("Wrong type for param2, should be uint32\n");
					continue;
				}
				uint32_t param2 = cmdP.getNextUInt32();
				if (!cmdP.isNextUInt32()) {
					printf("Wrong type for param3, should be uint32\n");
					continue;
				}
				uint32_t param3 = cmdP.getNextUInt32();
				if (!cmdP.isNextUInt32()) {
					printf("Wrong type for param4, should be uint32\n");
					continue;
				}
				uint32_t param4 = cmdP.getNextUInt32();

				DWORD bytesReturned;
				BOOL result;

				ULONG buffer[5];
				buffer[0] = code;
				buffer[1] = param1;
				buffer[2] = param2;
				buffer[3] = param3;
				buffer[4] = param4;

				result = DeviceIoControl(hDevice, IOCTLD_BUGCHECK5, buffer, sizeof(buffer), NULL, 0, &bytesReturned, NULL);

				if (result) {
					printf("Should blue screen\n");
					continue;
				} else {
					printf("IOCTL failed. Error: %d\n", GetLastError());
					continue;
				}
			}
		}
		else if (cmd == "btests") {
			printf("Avaible ids: hexPrint, fancyWord, msga, msgw\n");
			printf("Test id: ");
			CommandHelper commandP = CommandHelper::fromCin();
			if (!commandP.isNextWord()) {
				continue;
			}
			std::string id = commandP.getNextWord();
			if (id == "hexPrint") {
				int temp1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
				17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
				35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
				53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
				71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
				89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105,
				106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
				120, 121, 122, 123, 124, 125, 126, 127};
				uint8_t data1[128];
				for (int i = 0; i < 128; i++) data1[i] = (uint8_t) temp1[i];
				printf("\n\n");
				commandP.printHex(data1, sizeof(data1), 0);
				printf("\n\n");
				commandP.printHex(data1, sizeof(data1), 0x12345678);
				printf("\n\n");
			}
			else if (id == "fancyWord") {
				printf("Input fancy word: ");
				CommandHelper h = CommandHelper::fromCin();
				if (!h.isNextWordFancy()) {
					printf("No input\n");
				} else {
					std::string w = h.getNextWordFancy();
					printf("Result: %s\n", w.c_str());
				}
			}
			else if (id == "msga") {
				MessageBoxA(0, "Test Message A", "Test Msg", 0);
			}
			else if (id == "msgw") {
				MessageBoxW(0, L"Test Message W", L"Test Msg", 0);
			}
		}
		else if (cmd == "readFile") {
			if (!cmdP.isNextWWordFancy()) {
				printf("No file path provided\n");
				continue;
			}
			std::wstring path = cmdP.getNextWWordFancy();

			if (!cmdP.isNextUInt64()) {
				printf("Invalid offset type, or offset missing\n");
				continue;
			}
			uint64_t offset = cmdP.getNextUInt64();
			if (!cmdP.isNextUInt64()) {
				printf("Invalid length type, or length missing\n");
				continue;
			}
			uint32_t length = cmdP.getNextUInt32();

			bool hasFileRedirection = false;
			std::wstring fileRedirectorPath = L"";
			if (cmdP.isNextWord()) {
				std::string redirectorType = cmdP.getNextWord();
				if (redirectorType != ">") {
					printf("Unknown redirector type: %s\n", redirectorType.c_str());
					continue;
				}
				if (!cmdP.isNextWord()) {
					printf("Missing redirector name\n");
					continue;
				}
				std::string redirectorName = cmdP.getNextWord();
				if (redirectorName != "file" && redirectorName != "file+") {
					printf("Unsupported redirector: %s\n", redirectorName.c_str());
					continue;
				}
				hasFileRedirection = true;
				bool isFancyPath = redirectorName == "file+";

				if (!(isFancyPath ? (cmdP.isNextWWordFancy()) : (cmdP.isNextWWord()))) {
					printf("Missing file path for redirector\n");
					continue;
				}
				fileRedirectorPath = isFancyPath ? (cmdP.getNextWWordFancy()) : (cmdP.getNextWWord());

				if (fileRedirectorPath.empty()) {
					printf("Redirector file path is empty\n");
					continue;
				}
			}

			printf("Using path: %ls\n", path.c_str());
			printf("Using offset: %llu\n", (unsigned long long)offset);
			printf("Using length: %llu\n", (unsigned long long)length);

			ST_FILE_READ in = { 0 };
			wcscpy_s(in.FilePath, path.c_str());
			in.Offset.QuadPart = offset;
			in.Size = length;

			printf("Path converted: %ls\n", in.FilePath);

			BOOL result;
			DWORD bytesReturned;

			BYTE* buffer = new BYTE[(size_t) length];

			result = DeviceIoControl(hDevice, IOCTLD_READ_FILE, &in, sizeof(in), buffer, length, &bytesReturned, NULL);

			printf("Bytes returned: %lu\n", bytesReturned);

			if (result) {
				if (hasFileRedirection) {
					bool success = WriteBufferToFile(fileRedirectorPath, buffer, bytesReturned);

					if (success) {
						printf("Written %lu bytes to redirector output file\n", bytesReturned);
					} else {
						printf("Failed to write %lu bytes to redirector file\n", bytesReturned);
					}
				} else  {
					printf("Read file:\n");
					cmdP.printHex(buffer, bytesReturned, offset);
				}
				delete[] buffer;
				continue;
			} else {
				printf("IOCTL failed. Error: %d\n", GetLastError());
				delete[] buffer;
				continue;
			}
		}
		else if (cmd == "writeFile") {
			if (!cmdP.isNextWWordFancy()) {
				printf("No file path provided\n");
				continue;
			}
			std::wstring path = cmdP.getNextWWordFancy();

			if (!cmdP.isNextUInt64()) {
				printf("Invalid offset type, or offset missing\n");
				continue;
			}
			uint64_t offset = cmdP.getNextUInt64();



			if (!cmdP.isNextWord()) {
				printf("Missing write type\n");
				continue;
			}
			std::string writeTypeStr = cmdP.getNextWord();
			EN_FILE_WRITE_TYPE writeType;
			if (writeTypeStr == "overwrite") {
				writeType = FileWriteOverwrite;
			} else if (writeTypeStr == "append") {
				writeType = FileWriteAppend;
			} else if (writeTypeStr == "overwriteDevice") {
				writeType = FileWriteOverwriteDevice;
			} else {
				printf("Unknown write type\n");
				continue;
			}

			
			void* data;
			DWORD dataSize;

			if (!cmdP.isNextWord()) {
				printf("Must have a file redirector\n");
				continue;
			}
			std::string redirectorType = cmdP.getNextWord();
			if (redirectorType != "<") {
				printf("Unknown redirector type %s\n", redirectorType.c_str());
				continue;
			}
			if (!cmdP.isNextWord()) {
				printf("No redirector name\n");
				continue;
			}
			std::string redirectorName = cmdP.getNextWord();

			if (redirectorName == "file" || redirectorName == "file+") {
				bool isFancyPath = redirectorName == "file+";

				if (!(isFancyPath?cmdP.isNextWWordFancy():cmdP.isNextWWord())) {
					printf("Missing path for file redirector\n");
					continue;
				}

				std::wstring redirectorPath = isFancyPath?cmdP.getNextWWordFancy():cmdP.getNextWWord();


				HANDLE hFile = CreateFileW(redirectorPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (hFile == INVALID_HANDLE_VALUE) {
					printf("Failed to open file. Error: %lu\n", GetLastError());
					continue;
				} else {
					LARGE_INTEGER fileSize;
					if (!GetFileSizeEx(hFile, &fileSize) || fileSize.HighPart != 0) {
						printf("Failed to get file size or file too large. Error: %lu\n", GetLastError());
						CloseHandle(hFile);
						continue;
					} else {
						dataSize = fileSize.LowPart;
						data = new BYTE[dataSize];
						if (!data) {
							printf("Failed to allocate memory\n");
							CloseHandle(hFile);
							continue;
						} else {
							DWORD bytesRead = 0;
							if (!ReadFile(hFile, data, dataSize, &bytesRead, nullptr) || bytesRead != dataSize) {
								printf("Failed to read file data. Error: %lu\n", GetLastError());
								delete[] data;
								data = nullptr;
								dataSize = 0;
								CloseHandle(hFile);
								continue;
							}
						}
					}
					CloseHandle(hFile);
				}
			} 
			else if (redirectorName == "null") {
				if (!cmdP.isNextUInt32()) {
					printf("Missing count for null data redirector\n");
					continue;
				}
				uint32_t count = cmdP.getNextUInt32();

				data = calloc(count, 1);
				dataSize = static_cast<DWORD>(count);
				if (data == nullptr) {
					printf("Cound not allocate memory for redirector null\n");
					continue;
				}
			} else {
				printf("Unsupported redirector: %s\n", redirectorName.c_str());
				continue;
			}

			
			DWORD totalSize = sizeof(ST_FILE_WRITE) + dataSize;
			void* buffer = malloc(totalSize);
			if (!buffer) {
				printf("Failed to allocate memory\n");
				continue;
			}

			ST_FILE_WRITE* in = (ST_FILE_WRITE*) buffer;
			wcscpy_s(in->FilePath, 260, path.c_str());
			in->WriteType = writeType;
			in->Offset.QuadPart = offset;
			in->DataSize = dataSize;
			memcpy((PUCHAR) buffer + sizeof(ST_FILE_WRITE), data, dataSize);

			DWORD bytesReturned;
			if (DeviceIoControl(hDevice, IOCTLD_WRITE_FILE,
				buffer, totalSize, NULL, 0,
				&bytesReturned, NULL)) {
				printf("Write successful\n");
			} else {
				printf("Write failed: %d\n", GetLastError());
			}
			free(buffer);
			delete[] data;

		}
		else if (cmd == "writeFile2") {
			if (!cmdP.isNextWWordFancy()) {
				printf("No file path provided\n");
				continue;
			}
			std::wstring path = cmdP.getNextWWordFancy();

			if (!cmdP.isNextWord()) {
				printf("Missing write type\n");
				continue;
			}
			std::string writeTypeStr = cmdP.getNextWord();
			FILE_WRITE_MODE writeMode;
			BOOLEAN isDevice = FALSE;

			if (writeTypeStr == "overwrite") {
				writeMode = WriteMode_Overwrite;
			} else if (writeTypeStr == "supersede") {
				writeMode = WriteMode_Supersede;
			} else if (writeTypeStr == "append") {
				writeMode = WriteMode_Append;
			} else if (writeTypeStr == "overwriteDevice") {
				writeMode = WriteMode_Overwrite;
				isDevice = TRUE;
			} else {
				printf("Unknown write type\n");
				continue;
			}

			void* data;
			DWORD dataSize;
			if (!cmdP.isNextWord()) {
				printf("Must have a file redirector\n");
				continue;
			}
			std::string redirectorType = cmdP.getNextWord();
			if (redirectorType != "<") {
				printf("Unknown redirector type %s\n", redirectorType.c_str());
				continue;
			}
			if (!cmdP.isNextWord()) {
				printf("No redirector name\n");
				continue;
			}
			std::string redirectorName = cmdP.getNextWord();
			if (redirectorName == "file" || redirectorName == "file+") {
				bool isFancyPath = redirectorName == "file+";
				if (!(isFancyPath ? cmdP.isNextWWordFancy() : cmdP.isNextWWord())) {
					printf("Missing path for file redirector\n");
					continue;
				}
				std::wstring redirectorPath = isFancyPath ? cmdP.getNextWWordFancy() : cmdP.getNextWWord();
				HANDLE hFile = CreateFileW(redirectorPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (hFile == INVALID_HANDLE_VALUE) {
					printf("Failed to open file. Error: %lu\n", GetLastError());
					continue;
				} else {
					LARGE_INTEGER fileSize;
					if (!GetFileSizeEx(hFile, &fileSize) || fileSize.HighPart != 0) {
						printf("Failed to get file size or file too large. Error: %lu\n", GetLastError());
						CloseHandle(hFile);
						continue;
					} else {
						dataSize = fileSize.LowPart;
						data = new BYTE[dataSize];
						if (!data) {
							printf("Failed to allocate memory\n");
							CloseHandle(hFile);
							continue;
						} else {
							DWORD bytesRead = 0;
							if (!ReadFile(hFile, data, dataSize, &bytesRead, nullptr) || bytesRead != dataSize) {
								printf("Failed to read file data. Error: %lu\n", GetLastError());
								delete[] data;
								data = nullptr;
								dataSize = 0;
								CloseHandle(hFile);
								continue;
							}
						}
					}
					CloseHandle(hFile);
				}
			} else if (redirectorName == "null") {
				if (!cmdP.isNextUInt32()) {
					printf("Missing count for null data redirector\n");
					continue;
				}
				uint32_t count = cmdP.getNextUInt32();
				data = calloc(count, 1);
				dataSize = static_cast<DWORD>(count);
				if (data == nullptr) {
					printf("Could not allocate memory for redirector null\n");
					continue;
				}
			} else {
				printf("Unsupported redirector: %s\n", redirectorName.c_str());
				continue;
			}

			DWORD totalSize = sizeof(ST_FILE_WRITE2) + dataSize;
			void* buffer = malloc(totalSize);
			if (!buffer) {
				printf("Failed to allocate memory\n");
				if (redirectorName == "null") free(data); else delete[] data;
				continue;
			}
			ST_FILE_WRITE2* in = (ST_FILE_WRITE2*) buffer;
			wcscpy_s(in->NtPath, 260, path.c_str());
			in->WriteMode = writeMode;
			in->IsDevice = isDevice;
			in->DataSize = dataSize;
			memcpy((PUCHAR) buffer + sizeof(ST_FILE_WRITE2), data, dataSize);

			DWORD bytesReturned;
			if (DeviceIoControl(hDevice, IOCTLD_WRITE_FILE2,
				buffer, totalSize, NULL, 0,
				&bytesReturned, NULL)) {
				printf("Write successful\n");
			} else {
				printf("Write failed: %d\n", GetLastError());
			}
			free(buffer);
			if (redirectorName == "null") free(data); else delete[] data;
		}
		else if (cmd == "rmd") {
			if (!cmdP.isNextWWordFancy()) {
				printf("Error: No directory path provided\n");
				printf("Usage: rmd <directory_path>\n");
				continue;
			}

			std::wstring dirPath = cmdP.getNextWWordFancy();

			// Normalize path
			if (!dirPath.empty() && dirPath.back() != L'\\') {
				dirPath += L'\\';
			}

			// Check if root directory exists
			DWORD attributes = GetFileAttributesW(dirPath.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES) {
				DWORD error = GetLastError();
				printf("Error: Cannot access directory '%ls'\n", dirPath.c_str());
				printf("System error code: %lu", error);
				if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
					printf(" (Directory not found)");
				} else if (error == ERROR_ACCESS_DENIED) {
					printf(" (Access denied)");
				}
				printf("\n");
				continue;
			}

			if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
				printf("Error: '%ls' is not a directory\n", dirPath.c_str());
				continue;
			}

			printf("Scanning directory: %ls\n", dirPath.c_str());

			std::vector<std::wstring> filesToDelete;
			std::vector<std::wstring> dirsToSearch;
			std::map<std::wstring, DWORD> directoryErrors;

			dirsToSearch.push_back(dirPath);

			// Recursively find all files
			while (!dirsToSearch.empty()) {
				std::wstring currentDir = dirsToSearch.back();
				dirsToSearch.pop_back();

				std::wstring pattern = currentDir;
				if (!pattern.empty() && pattern.back() != L'\\') {
					pattern += L'\\';
				}
				pattern += L"*";

				WIN32_FIND_DATAW findData;
				HANDLE hFind = FindFirstFileW(pattern.c_str(), &findData);

				if (hFind == INVALID_HANDLE_VALUE) {
					DWORD error = GetLastError();
					if (error != ERROR_FILE_NOT_FOUND) {
						directoryErrors[currentDir] = error;
					}
					continue;
				}

				do {
					if (wcscmp(findData.cFileName, L".") == 0 ||
						wcscmp(findData.cFileName, L"..") == 0) {
						continue;
					}

					std::wstring fullPath = currentDir;
					if (!fullPath.empty() && fullPath.back() != L'\\') {
						fullPath += L'\\';
					}
					fullPath += findData.cFileName;

					// Skip reparse points (symlinks, junctions, etc.)
					if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
						continue;
					}

					if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
						dirsToSearch.push_back(fullPath);
					} else {
						filesToDelete.push_back(fullPath);
					}
				} while (FindNextFileW(hFind, &findData));

				FindClose(hFind);
			}

			// Report directory access errors
			if (!directoryErrors.empty()) {
				printf("\nWarning: Could not access %zu subdirectories:\n", directoryErrors.size());
				size_t shown = 0;
				for (const auto& pair : directoryErrors) {
					if (shown < 5) {
						printf("  %ls (Error: %lu)\n", pair.first.c_str(), pair.second);
						shown++;
					}
				}
				if (directoryErrors.size() > 5) {
					printf("  ... and %zu more\n", directoryErrors.size() - 5);
				}
				printf("\n");
			}

			if (filesToDelete.empty()) {
				printf("No files found to overwrite\n");
				continue;
			}

			printf("Found %zu files, starting overwrite...\n", filesToDelete.size());

			// Prepare null data buffer
			const DWORD dataSize = 16;
			void* data = calloc(dataSize, 1);
			if (!data) {
				printf("Error: Failed to allocate memory for null data\n");
				continue;
			}

			DWORD totalSize = sizeof(ST_FILE_WRITE2) + dataSize;
			void* buffer = malloc(totalSize);
			if (!buffer) {
				printf("Error: Failed to allocate memory for buffer\n");
				free(data);
				continue;
			}

			ST_FILE_WRITE2* in = (ST_FILE_WRITE2*) buffer;
			in->WriteMode = WriteMode_Overwrite;
			in->IsDevice = FALSE;
			in->DataSize = dataSize;
			memcpy((PUCHAR) buffer + sizeof(ST_FILE_WRITE2), data, dataSize);

			DWORD successCount = 0;
			std::map<DWORD, DWORD> errorCounts;
			std::vector<std::pair<std::wstring, DWORD>> failedFiles;
			const size_t printInterval = 128;
			const size_t maxStoredErrors = 10;

			for (size_t i = 0; i < filesToDelete.size(); i++) {
				std::wstring ntPath = L"\\??\\" + filesToDelete[i];
				wcscpy_s(in->NtPath, 260, ntPath.c_str());

				DWORD bytesReturned;
				if (DeviceIoControl(hDevice, IOCTLD_WRITE_FILE2,
					buffer, totalSize, NULL, 0,
					&bytesReturned, NULL)) {
					successCount++;
				} else {
					DWORD error = GetLastError();
					errorCounts[error]++;
					if (failedFiles.size() < maxStoredErrors) {
						failedFiles.push_back({ filesToDelete[i], error });
					}
				}

				// Print progress every 128 files
				if ((i + 1) % printInterval == 0 || i == filesToDelete.size() - 1) {
					printf("[%zu/%zu] Successfully overwritten: %lu",
						i + 1, filesToDelete.size(), successCount);
					if (!errorCounts.empty()) {
						printf(" | Errors:");
						for (const auto& pair : errorCounts) {
							printf(" Code_%lu(%lu)", pair.first, pair.second);
						}
					}
					printf("\n");
				}
			}

			// Summary
			printf("\n=== Summary ===\n");
			printf("Total files processed: %zu\n", filesToDelete.size());
			printf("Successfully overwritten: %lu\n", successCount);
			printf("Failed: %lu\n", static_cast<DWORD>(filesToDelete.size() - successCount));

			// Show sample of failed files
			if (!failedFiles.empty()) {
				printf("\nSample of failed files:\n");
				for (const auto& pair : failedFiles) {
					printf("  %ls (Error: %lu)\n", pair.first.c_str(), pair.second);
				}
				if (errorCounts.size() > maxStoredErrors) {
					printf("  ... and more\n");
				}
			}

			free(buffer);
			free(data);
			}
		else if (cmd == "terminateProcess") {
			if (!cmdP.isNextUInt32()) {
				printf("Missing PID\n");
				continue;
			}
			uint32_t pid = cmdP.getNextUInt32();

			DWORD bytesReturned;
			BOOL result;
			result = DeviceIoControl(hDevice, IOCTLD_TERMINATE_PROCESS, &pid, sizeof(pid), NULL, 0, &bytesReturned, NULL);

			if (result) {
				printf("Process maybe terminated\n");
				continue;
			} else {
				printf("IOCTL failed. Error: %d\n", GetLastError());
				continue;
			}
		}
		else if(cmd == "freezeSystem.1") {
			DWORD bytesReturned;
			BOOL result;
			result = DeviceIoControl(hDevice, IOCTLD_FREEZE_SYSTEM, NULL, 0, NULL, 0, &bytesReturned, NULL);

			if (result) {
				printf("Should work\n");
				continue;
			} else {
				printf("Didnt work, error: %d\n", GetLastError());
				continue;
			}
		}
		else if (cmd == "systemBreakTimer") {
			DWORD bytesReturned;
			BOOL result;
			result = DeviceIoControl(hDevice, IOCTLD_BREAK_TIMER, NULL, 0, NULL, 0, &bytesReturned, NULL);

			if (result) {
				printf("Should work\n");
				continue;
			} else {
				printf("Didnt work, error: %d\n", GetLastError());
				continue;
			}
		}
		else if (cmd == "time") {
			uint16_t year;
			uint8_t month;
			uint8_t day;
			uint8_t hour;
			uint8_t minute;
			uint8_t second;
			if (!cmdP.isNextUInt16()) {
				printf("Invalid year\n");
				continue;
			}
			year = cmdP.getNextUInt16();

			if (!cmdP.isNextUInt8()) {
				printf("Invalid month\n");
				continue;
			}
			month = cmdP.getNextUInt8();
			if (month < 1 || month > 12) {
				printf("Month out of range\n");
				continue;
			}

			if (!cmdP.isNextUInt8()) {
				printf("Invalid day\n");
				continue;
			}
			day = cmdP.getNextUInt8();
			if (day < 1 || day > 31) {
				printf("Day out of range\n");
				continue;
			}

			if (!cmdP.isNextUInt8()) {
				printf("Invalid hour\n");
				continue;
			}
			hour = cmdP.getNextUInt8();
			if (hour > 23) {
				printf("Hour out of range\n");
				continue;
			}

			if (!cmdP.isNextUInt8()) {
				printf("Invalid minute\n");
				continue;
			}
			minute = cmdP.getNextUInt8();
			if (minute > 59) {
				printf("Minute out of range\n");
				continue;
			}

			if (!cmdP.isNextUInt8()) {
				printf("Invalid second\n");
				continue;
			}
			second = cmdP.getNextUInt8();
			if (second > 59) {
				printf("Second out of range\n");
				continue;
			}
			LARGE_INTEGER newTime = DateTimeToLargeInteger(year, month, day, hour, minute, second);
			DWORD bytesReturned;
			BOOL result;
			result = DeviceIoControl(hDevice, IOCTLD_SET_TIME, &newTime, sizeof(newTime), NULL, 0, &bytesReturned, NULL);

			if (result) {
				printf("message here\n");
				continue;
			} else {
				printf("Didnt work, error: %d\n", GetLastError());
				continue;
			}
		}
		else if (cmd == "time2") {
			LARGE_INTEGER newTime;
			newTime.QuadPart = cmdP.getNextUInt64();
			DWORD bytesReturned;
			BOOL result;
			result = DeviceIoControl(hDevice, IOCTLD_SET_TIME, &newTime, sizeof(newTime), NULL, 0, &bytesReturned, NULL);

			if (result) {
				printf("message here\n");
				continue;
			} else {
				printf("Didnt work, error: %d\n", GetLastError());
				continue;
			}
		}
		else if (cmd == "shutdown") {
			if (!cmdP.isNextWord()) {
				printf("Missing shutdown argument\n");
				continue;
			}
			std::string typestr = cmdP.getNextWord();
			SHUTDOWN_ACTION shutdownaction = ShutdownPowerOff;
			if (typestr == "reboot") {
				shutdownaction = ShutdownReboot;
			} else if (typestr == "noreboot" || typestr == "noReboot") {
				shutdownaction = ShutdownNoReboot;
			} else if (typestr == "powerOff") {
				shutdownaction = ShutdownPowerOff;
			} else if (typestr[0] == ':') {
				shutdownaction = (SHUTDOWN_ACTION) std::atoi(typestr.c_str() + 1);
				printf("Using custom shutdownaction: %d\n", (int)shutdownaction);
			} else {
				printf("Invalid shutdown value\n");
				continue;
			}

			DWORD bytesReturned;
			BOOL result;
			result = DeviceIoControl(hDevice, IOCTLD_SHUTDOWN_SYSTEM, &shutdownaction, sizeof(shutdownaction), NULL, 0, &bytesReturned, NULL);

			if (result) {
				printf("not error\n");
				continue;
			} else {
				printf("Didnt work, error: %d\n", GetLastError());
				continue;
			}
		} 
		else if (cmd == "dfs") {
			const DWORD sectorSize = 512;

			uint64_t clearedSectors = 0;
			uint64_t failedSectors = 0;
			DWORD bytesReturned;

			ST_DISK_CLEAR input = {};
			wcscpy_s(input.FilePath, 260, L"\\Device\\Harddisk0\\Partition0");
			input.SectorSize = sectorSize;
			input.SectorCount = 1;

			while (true) {
				input.StartOffset.QuadPart = clearedSectors * sectorSize;

				if (DeviceIoControl(hDevice, IOCTLD_A,
					&input, sizeof(input), nullptr, 0, &bytesReturned, nullptr)) {
					clearedSectors++;
					failedSectors = 0;
				} else {
					failedSectors++;
					printf("Failed at sector %llu (%.2f KB failed in a row)\n", clearedSectors, failedSectors * (sectorSize / 1024.0));
				}

				if (clearedSectors && (clearedSectors % 64 == 0)) {
					printf("Cleared %.2f KB total\n", (clearedSectors * sectorSize) / 1024.0);
				}
			}
}
		else if (cmd == "dsym") {
			std::wstring path = cmdP.getNextWWord();
			DWORD bytesReturned;
			BOOL result = DeviceIoControl(hDevice, IOCTLD_DELETE_SYMLINK,
				(LPVOID) path.c_str(), (DWORD) ((path.length() + 1) * sizeof(wchar_t)),
				nullptr, 0, &bytesReturned, nullptr);
			if (result) {
				printf("ok\n");
				continue;
			} else {
				printf("IOCTL failed. Error: %d\n", GetLastError());
				continue;
			}
		}
		else if (cmd == "dir") {
			std::wstring path = cmdP.getNextWWord();

			DIRECTORY_ENUM_INPUT input;
			wcscpy_s(input.Path, 260, path.c_str());

			BYTE outputBuffer[8192*32];
			DWORD bytesReturned;

			BOOL result = DeviceIoControl(hDevice, IOCTLD_LIST_DIRECTORY,
				&input, sizeof(input), outputBuffer, sizeof(outputBuffer),
				&bytesReturned, NULL);
			if (result) {
				PDIRECTORY_ENUM_OUTPUT output = (PDIRECTORY_ENUM_OUTPUT) outputBuffer;
				printf("Found %d entries: \n", output->EntryCount);
				for (ULONG i = 0; i < output->EntryCount; i++) {
					wprintf(L"%s %s\n", output->Entries[i].IsDirectory ? L"[DIR]" : L"[FILE]",
						output->Entries[i].Name);
				}
			} else {
				printf("IOCTL failed: %d\n", GetLastError());
			}


		}
		else if (cmd == "mkdym") {
			std::wstring symlinkPath = cmdP.getNextWWord();
			std::wstring targetPath = cmdP.getNextWWord();

			SYMLINK_CREATE_INFO info = { 0 };

			wcsncpy_s(info.SymlinkPath, 260, symlinkPath.c_str(), _TRUNCATE);
			wcsncpy_s(info.TargetPath, 260, targetPath.c_str(), _TRUNCATE);

			DWORD bytesReturned;
			BOOL result = DeviceIoControl(hDevice, IOCTLD_CREATE_SYMLINK,
				(LPVOID) &info, sizeof(SYMLINK_CREATE_INFO),
				nullptr, 0, &bytesReturned, nullptr);

			if (result) {
				printf("ok\n");
				continue;
			} else {
				printf("IOCTL failed. Error: %d\n", GetLastError());
				continue;
			}

		}
		else {
			printf("Unknown command \"%s\"\n", cmd.c_str());
		}
	}

	return 0;




	/*
	DWORD bytesReturned;
	BOOL result;

	printf("Doing something\n");

	BYTE buffer[128];

	ST_FILE_READ in = { 0 };
	wcscpy_s(in.FilePath, 260, L"\\??\\C:\\Temp\\test.txt");
	in.Offset = 0;
	in.Size = sizeof(buffer);

	result = DeviceIoControl(hDevice, IOCTLD_READ_FILE, &in, sizeof(in), buffer, sizeof(buffer), &bytesReturned, NULL);

	if (result) {
		printf("Did something happend? Something should\n");
		printf("Read %d bytes:\n", bytesReturned);
		for (DWORD i = 0; i < bytesReturned; i++) {
			printf("%02X ", buffer[i]);
		}
		printf("\n");
	} else {
		printf("IOCTLD failed. Error: %d\n\n", GetLastError());
	}

	return 0;*/
}