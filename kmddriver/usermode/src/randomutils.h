#include <windows.h>
#include <string>

bool CreateDirectoryRecursive(const std::wstring& dir) {
    size_t pos = 0;
    do {
        pos = dir.find_first_of(L"\\/", pos + 1);
        std::wstring subdir = dir.substr(0, pos);
        if (!subdir.empty() && GetFileAttributesW(subdir.c_str()) == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectoryW(subdir.c_str(), NULL)) {
                if (GetLastError() != ERROR_ALREADY_EXISTS)
                    return false;
            }
        }
    } while (pos != std::wstring::npos);
    return true;
}

bool WriteBufferToFile(const std::wstring& filePath, BYTE* buffer, DWORD bytesReturned) {
    // Extract parent directory path
    size_t lastSlash = filePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        std::wstring parentDir = filePath.substr(0, lastSlash);
        if (!CreateDirectoryRecursive(parentDir))
            return false;
    }

    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD bytesWritten = 0;
    BOOL result = WriteFile(hFile, buffer, bytesReturned, &bytesWritten, NULL);

    CloseHandle(hFile);

    return (result && bytesWritten == bytesReturned);
}
