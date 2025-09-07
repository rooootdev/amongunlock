#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

#ifndef TH32CS_SNAPMODULE32
#define TH32CS_SNAPMODULE32 0x00000010
#endif

std::map<char, char> byteDigits = { 
    { '0', 0x0 }, { '1', 0x1 }, { '2', 0x2 }, { '3', 0x3 }, 
    { '4', 0x4 }, { '5', 0x5 }, { '6', 0x6 }, { '7', 0x7 }, 
    { '8', 0x8 }, { '9', 0x9 }, { 'A', 0xA }, { 'B', 0xB }, 
    { 'C', 0xC }, { 'D', 0xD }, { 'E', 0xE }, { 'F', 0xF } 
};

HWND windowcheck(const std::wstring& windowName) {
    HWND hwnd = nullptr;
    while (!hwnd) {
        hwnd = FindWindowW(nullptr, windowName.c_str());
        if (!hwnd) Sleep(500);
    }
    return hwnd;
}

DWORD getpid(HWND hwnd) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid;
}

uintptr_t getmodulebase(DWORD pid, const std::wstring& moduleName) {
    MODULEENTRY32W modEntry;
    modEntry.dwSize = sizeof(MODULEENTRY32W);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (Module32FirstW(hSnap, &modEntry)) {
        do {
            if (moduleName == modEntry.szModule) {
                CloseHandle(hSnap);
                return reinterpret_cast<uintptr_t>(modEntry.modBaseAddr);
            }
        } while (Module32NextW(hSnap, &modEntry));
    }
    CloseHandle(hSnap);
    return 0;
}

uintptr_t signscan(HANDLE hProc, uintptr_t base, DWORD size, const std::string& sig) {
    std::vector<std::pair<char,bool>> search;
    for (size_t i = 0; i < sig.length(); i += 2) {
        if (sig[i] == ' ') i++;
        if (sig[i] == '?') {
            search.push_back({0x0,true});
            if (sig[i+1] != '?') i--;
        } else {
            search.push_back({ char((byteDigits[sig[i]] << 4) + byteDigits[sig[i+1]]), false });
        }
    }

    DWORD progress = 0;
    long long cachedProgress = 0;
    for (DWORD a = 0; a < size; a += 0x500) {
        SIZE_T currScanSize = std::min(SIZE_T(size - a), SIZE_T(0x500));
        std::vector<char> buffer(currScanSize);
        ReadProcessMemory(hProc, (LPCVOID)(base + a), buffer.data(), currScanSize, nullptr);
        for (DWORD b = 0; b < currScanSize; b++) {
            if (search[progress].second || buffer[b] == search[progress].first) {
                if (progress == 0) cachedProgress = b + 1;
                progress++;
            } else if (progress > 0) {
                b = cachedProgress;
                progress = 0;
            }
            if (progress >= search.size()) {
                return base + (a + b - (search.size() - 1));
            }
        }
    }
    return 0;
}

bool patchmem(HANDLE hProc, uintptr_t address, const std::string& bytes) {
    std::vector<char> patch;
    for (size_t i = 0; i < bytes.length(); i += 2) {
        if (bytes[i] == ' ') i++;
        patch.push_back(char((byteDigits[bytes[i]] << 4) + byteDigits[bytes[i+1]]));
    }
    DWORD oldProtect = 0;
    VirtualProtectEx(hProc, (LPVOID)address, patch.size(), PAGE_EXECUTE_READWRITE, &oldProtect);
    int success = 0;
    for (size_t i = 0; i < patch.size(); i++) {
        if (WriteProcessMemory(hProc, (LPVOID)(address + i), &patch[i], 1, nullptr)) success++;
    }
    VirtualProtectEx(hProc, (LPVOID)address, patch.size(), oldProtect, nullptr);
    return success == static_cast<int>(patch.size());
}

int main() {
    SetConsoleTitleA("amongunlock (16.1.0)");
    std::wcout << L"[ i ] waiting for amongus window\n";
    HWND hwnd = windowcheck(L"Among Us");
    DWORD pid = getpid(hwnd);
    std::wcout << L"[ + ] found PID: " << pid << "\n\n";

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        std::cerr << "[ - ] failed to open process\n";
        Sleep(5000);
        return 1;
    }

    std::wcout << L"[ i ] waiting for GameAssembly.dll to load\n";
    uintptr_t moduleBase = 0;
    DWORD moduleSize = 0;
    while (!moduleBase) {
        moduleBase = getmodulebase(pid, L"GameAssembly.dll");
        if (moduleBase) {
            MODULEENTRY32W me = { sizeof(me) };
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
            if (Module32FirstW(snap, &me)) {
                do { if (me.modBaseAddr == (BYTE*)moduleBase) moduleSize = (DWORD)me.modBaseSize; } while (Module32NextW(snap, &me));
            }
            CloseHandle(snap);
        }
        Sleep(200);
    }
    std::wcout << L"[ + ] found GameAssembly.dll at 0x" << std::hex << moduleBase 
               << " with size 0x" << moduleSize << std::dec << "\n\n";

    BYTE flag = 0;
    ReadProcessMemory(hProc, (LPCVOID)(moduleBase + 0x20), &flag, sizeof(flag), nullptr);
    if (flag == 0x69) {
        std::wcout << L"[ i ] already patched\n";
        Sleep(5000);
        CloseHandle(hProc);
        return 0;
    }

    uintptr_t scanAddr = 0;
    int tries = 0;
    while (!scanAddr && tries < 5) {
        scanAddr = signscan(hProc, moduleBase, moduleSize, "80 7E ?? 00 74 05 B0 01 5E 5D C3");
        if (!scanAddr) {
            std::wcout << L"[ i ] pattern not found, retrying...\n";
            Sleep(500);
            tries++;
        }
    }

    if (!scanAddr) {
        std::cerr << "[ - ] pattern not found\n";
        Sleep(5000);
        CloseHandle(hProc);
        return 1;
    }

    scanAddr += 4;
    std::wcout << L"[ + ] pattern found at: 0x" << std::hex << scanAddr << "\n";

    if (!patchmem(hProc, scanAddr, "90 90")) {
        std::cerr << "[ - ] failed to patch memory\n";
        Sleep(5000);
        CloseHandle(hProc);
        return 1;
    }
    std::wcout << L"[ + ] patched memory successfully.\n";

    patchmem(hProc, moduleBase + 0x20, "69");
    std::wcout << L"[ + ] process marked as patched\n";

    Sleep(5000);
    CloseHandle(hProc);
    return 0;
}
