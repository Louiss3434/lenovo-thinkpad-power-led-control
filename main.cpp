#include <iostream>
#include <fstream>
#include <windows.h>
#include <string>
#include <direct.h>     
#include <errno.h>      

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

#define EC_CMD_PORT       0x66
#define EC_DATA_PORT      0x62
#define LED_REG           0x0C
#define EC_CTRLPORT_WRITE 0x81

#define POWER_OFF         0x00
#define POWER_ON          0x80
#define MIC_MUTE_OFF      0x0E

typedef void(__stdcall* lpOut32)(short, short);
typedef short(__stdcall* lpInp32)(short);

std::string GetInternalPath(const std::string& fileName) {
    std::string base;
    char* appData = getenv("APPDATA");
    
    if (appData != NULL) {
        base = std::string(appData) + "\\LedToggle";
    } else {
        base = "C:\\LedToggle";
    }
    
    _mkdir(base.c_str());
    
    SetFileAttributesA(base.c_str(), FILE_ATTRIBUTE_HIDDEN);
    
    return base + "\\" + fileName;
}
const std::string STATE_FILE = GetInternalPath("state.bin");
const std::string LOG_FILE   = GetInternalPath("error.log");

void LogError(const std::string& msg) {
    std::ofstream log(LOG_FILE.c_str(), std::ios::app);
    if (log.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        log << "[" << st.wYear << "-" << st.wMonth << "-" << st.wDay << " "
            << st.wHour << ":" << st.wMinute << ":" << st.wSecond 
            << "] " << msg << std::endl;
    }
}

bool WaitEC(lpInp32 gInp32) {
    if (!gInp32) return false;
    for (int i = 0; i < 3; ++i) {
        int timeout = 400; 
        while ((gInp32(EC_CMD_PORT) & 0x02) && --timeout > 0) {
            Sleep(1);
        }
        if (timeout > 0) return true;
        Sleep(15); 
    }
    return false;
}

bool WriteReg(lpOut32 gOut32, lpInp32 gInp32, BYTE addr, BYTE val) {
    if (!gOut32 || !gInp32) return false;
    if (!WaitEC(gInp32)) return false;
    gOut32(EC_CMD_PORT, EC_CTRLPORT_WRITE);
    if (!WaitEC(gInp32)) return false;
    gOut32(EC_DATA_PORT, addr);
    if (!WaitEC(gInp32)) return false;
    gOut32(EC_DATA_PORT, val);
    return true;
}

bool IsElevated() {
    BOOL isElevated = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(token, TokenElevation, &elevation, size, &size)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return isElevated != FALSE;
}

int main() {
    if (!IsElevated()) {
        std::cerr << "Admin privileges required.\n";
        return 1;
    }

    HANDLE mutex = CreateMutexA(NULL, TRUE, "Global\\LedToggle_v4_Fortress");
    if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) return 1;
    std::string pathStr(exePath);
    std::string dllPath = pathStr.substr(0, pathStr.find_last_of("\\/")) + "\\InpOutx64.dll";

    HANDLE hDllFile = CreateFileA(dllPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDllFile == INVALID_HANDLE_VALUE) {
        LogError("Security Alert: DLL missing or locked: " + dllPath);
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 1;
    }

    HINSTANCE hDll = LoadLibraryExA(dllPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hDll) {
        LogError("Failed to load DLL securely.");
        CloseHandle(hDllFile);
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 1;
    }

    {
        lpOut32 gOut32 = (lpOut32)GetProcAddress(hDll, "Out32");
        lpInp32 gInp32 = (lpInp32)GetProcAddress(hDll, "Inp32");

        if (gOut32 && gInp32) {
            bool shouldTurnOff = false; 
            std::ifstream in(STATE_FILE.c_str(), std::ios::binary);
            if (in.is_open()) {
                in >> shouldTurnOff;
                in.close();
            }

            bool success = true;
            success &= WriteReg(gOut32, gInp32, LED_REG, MIC_MUTE_OFF);

            if (shouldTurnOff) {
                success &= WriteReg(gOut32, gInp32, LED_REG, POWER_ON);
                shouldTurnOff = false;
            } else {
                success &= WriteReg(gOut32, gInp32, LED_REG, POWER_OFF);
                shouldTurnOff = true;
            }

            if (success) {
                std::ofstream out(STATE_FILE.c_str(), std::ios::binary | std::ios::trunc);
                if (out.is_open()) {
                    out << shouldTurnOff;
                    out.close();
                }
            } else {
                LogError("Hardware communication failure.");
            }
        }
    }
    FreeLibrary(hDll);
    CloseHandle(hDllFile);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    
    return 0;
}
