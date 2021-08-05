#include "Undecorate.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

static bool UndecorateImpl(const std::vector<std::string>& decoratedNames, std::vector<std::string>& undecoratedNames)
{
    static const char* INPUT_FILENAME = "__temp_undname_input.txt";
    static const char* OUTPUT_FILENAME = "__temp_undname_output.txt";

    FILE* f = nullptr;
    if (fopen_s(&f, INPUT_FILENAME, "wb") || !f) {
        printf("Failed to create temporary input file\n");
        return false;
    }
    for (const std::string& name : decoratedNames) {
        fwrite(name.data(), 1, name.size(), f);
        fputc('\n', f);
    }
    fclose(f);
    f = nullptr;

    SECURITY_ATTRIBUTES securityAttributes = { 0 };
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.bInheritHandle = TRUE;
    securityAttributes.lpSecurityDescriptor = NULL;
    HANDLE hFile = CreateFile(
        OUTPUT_FILENAME,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        &securityAttributes,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        0
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Failed to create temporary output file\n");
        return false;
    }

    char cmd[256];
    sprintf_s(cmd, "undname.exe 0x%x %s", 0x29FFF, INPUT_FILENAME);
    STARTUPINFO startInfo = { 0 };
    startInfo.cb = sizeof(STARTUPINFO);
    startInfo.dwFlags |= STARTF_USESTDHANDLES;
    startInfo.hStdInput = INVALID_HANDLE_VALUE;
    startInfo.hStdError = INVALID_HANDLE_VALUE;
    startInfo.hStdOutput = hFile;
    PROCESS_INFORMATION processInfo = { 0 };
    BOOL ok = CreateProcess(
        NULL, cmd, NULL, NULL,
        TRUE, CREATE_NO_WINDOW,
        NULL, NULL,
        &startInfo, &processInfo
    );
    if (!ok) {
        printf("Failed to run undname.exe from Visual C++ environment\n");
        CloseHandle(hFile);
        return false;
    }
    if (WaitForSingleObject(processInfo.hProcess, INFINITE) != WAIT_OBJECT_0) {
        printf("Could not wait for process completion\n");
        CloseHandle(processInfo.hProcess);
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(hFile);

    if (fopen_s(&f, OUTPUT_FILENAME, "rb") || !f) {
        printf("Failed to read temporary output file\n");
        return false;
    }
    for (int i = 0; i < decoratedNames.size(); i++) {
        char buffer[1 << 16];   //undecorated names can be quite lengthy =(
        buffer[0] = 0;
        if (!fgets(buffer, sizeof(buffer), f))
            return false;
        size_t len = strlen(buffer);
        if (len + 1 >= sizeof(buffer)) {
            printf("Undecorated name is too long\n");
            fclose(f);
            return false;
        }
        while (len > 0 && isspace(buffer[len - 1]))
            buffer[--len] = 0;
        undecoratedNames.push_back(buffer);
    }
    fclose(f);
    f = nullptr;

    return true;
}

bool Undecorate(const std::vector<std::string>& decoratedNames, std::vector<std::string>& undecoratedNames)
{
    undecoratedNames.clear();

    bool ok = UndecorateImpl(decoratedNames, undecoratedNames);

    if (undecoratedNames.size() != decoratedNames.size())
        ok = false;
    if (!ok)
        undecoratedNames.assign(decoratedNames.size(), "");

    return ok;
}
