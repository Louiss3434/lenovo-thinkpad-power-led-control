#include <iostream>
#include <fstream>
#include <windows.h>

typedef void(__stdcall* lpOut32)(short, short);
typedef short(__stdcall* lpInp32)(short);

#define EC_SC          0x66 
#define EC_DATA        0x62 
#define EC_WRITE_CMD   0x81  // Protocol for Power LED
#define EC_LED_CMD     0x10  // Protocol for F4/Mic LED

const char* STATE_FILE = "led_state.bin";

bool WaitEC(lpInp32 gInp32) {
    int timeout = 500;
    while ((gInp32(EC_SC) & 0x02) && timeout > 0) { Sleep(1); timeout--; }
    return timeout > 0;
}

// Simple wrapper for the 0x81 (Write Register) protocol
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

    // 1. FORCE MIC (F4) OFF EVERY TIME
    // 0x02 is the ID for Mic LED, 0x00 is the "Off" mode
    if (WaitEC(gInp32)) {
        gOut32(EC_SC, EC_LED_CMD); 
        if (WaitEC(gInp32)) gOut32(EC_DATA, 0x02); 
    }

    // 2. TOGGLE POWER LED
    bool isOff = false;
    std::ifstream inFile(STATE_FILE);
    if (inFile.is_open()) { inFile >> isOff; inFile.close(); }

    if (isOff) {
        // TURN ON: Just set the master switch
        WriteReg(gOut32, gInp32, 0x0C, 0x80); 
        WriteReg(gOut32, gInp32, 0xA0, 0x80); // Brightness up
        isOff = false;
    } else {
        // TURN OFF: Kill the switch and the brightness
        WriteReg(gOut32, gInp32, 0x0C, 0x00); 
        WriteReg(gOut32, gInp32, 0xA0, 0x00); // Set brightness 0
        isOff = true;
    }

    // 3. SAVE STATE
    std::ofstream outFile(STATE_FILE);
    if (outFile.is_open()) { outFile << isOff; outFile.close(); }

    FreeLibrary(hInpOutDll);
    return 0;
}
