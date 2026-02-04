#include <iostream>
#include <fstream>
#include <windows.h>

typedef void(__stdcall* lpOut32)(short, short);
typedef short(__stdcall* lpInp32)(short);

#define EC_SC          0x66 
#define EC_DATA        0x62 
#define EC_WRITE_CMD   0x81

const char* STATE_FILE = "led_state.bin";

bool WaitEC(lpInp32 gInp32) {
    int timeout = 500;
    while ((gInp32(EC_SC) & 0x02) && timeout > 0) { Sleep(1); timeout--; }
    return timeout > 0;
}

void WriteReg(lpOut32 gOut32, lpInp32 gInp32, BYTE addr, BYTE val) {
    if (!WaitEC(gInp32)) return;
    gOut32(EC_SC, EC_WRITE_CMD);
    if (!WaitEC(gInp32)) return;
    gOut32(EC_DATA, addr);
    if (!WaitEC(gInp32)) return;
    gOut32(EC_DATA, val);
}

int main() {
    HINSTANCE hInpOutDll = LoadLibraryA("InpOutx64.dll");
    if (!hInpOutDll) return 1;
    lpOut32 gOut32 = (lpOut32)GetProcAddress(hInpOutDll, "Out32");
    lpInp32 gInp32 = (lpInp32)GetProcAddress(hInpOutDll, "Inp32");

    bool isOff = false;
    std::ifstream inFile(STATE_FILE);
    if (inFile.is_open()) { inFile >> isOff; inFile.close(); }

    if (isOff) {
        WriteReg(gOut32, gInp32, 0x0C, 0x80); 
        WriteReg(gOut32, gInp32, 0xA0, 0x80); 
        isOff = false;
    } else {
        WriteReg(gOut32, gInp32, 0x0C, 0x00); 
        WriteReg(gOut32, gInp32, 0xA0, 0x00);
        isOff = true;
    }

    std::ofstream outFile(STATE_FILE);
    if (outFile.is_open()) { outFile << isOff; outFile.close(); }

    FreeLibrary(hInpOutDll);
    return 0;
}
