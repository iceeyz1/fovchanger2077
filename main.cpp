#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <limits>

struct PatternByte {
    bool wildcard;
    unsigned char value;
};

void WaitExit() {
    std::cout << "\nPress Enter to exit...";
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

DWORD GetProcessIdByName(const std::wstring& processName) {
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (processName == entry.szExeFile) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

bool GetModuleInfoByName(DWORD pid, const std::wstring& moduleName, uintptr_t& base, DWORD& size) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W mod{};
    mod.dwSize = sizeof(mod);

    bool ok = false;
    if (Module32FirstW(snapshot, &mod)) {
        do {
            if (moduleName == mod.szModule) {
                base = reinterpret_cast<uintptr_t>(mod.modBaseAddr);
                size = mod.modBaseSize;
                ok = true;
                break;
            }
        } while (Module32NextW(snapshot, &mod));
    }

    CloseHandle(snapshot);
    return ok;
}

std::vector<PatternByte> ParsePattern(const std::string& pattern) {
    std::vector<PatternByte> result;
    std::istringstream iss(pattern);
    std::string token;

    while (iss >> token) {
        if (token == "?" || token == "??") {
            result.push_back({ true, 0 });
        }
        else {
            unsigned int byteValue = 0;
            std::stringstream ss;
            ss << std::hex << token;
            ss >> byteValue;
            result.push_back({ false, static_cast<unsigned char>(byteValue) });
        }
    }

    return result;
}

uintptr_t FindPattern(const unsigned char* data, size_t size, const std::vector<PatternByte>& pattern) {
    if (pattern.empty() || size < pattern.size()) {
        return 0;
    }

    for (size_t i = 0; i <= size - pattern.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (!pattern[j].wildcard && data[i + j] != pattern[j].value) {
                found = false;
                break;
            }
        }
        if (found) {
            return static_cast<uintptr_t>(i);
        }
    }

    return 0;
}

uintptr_t AobScanModule(HANDLE hProcess, uintptr_t moduleBase, DWORD moduleSize, const std::string& patternStr) {
    std::vector<PatternByte> pattern = ParsePattern(patternStr);
    if (pattern.empty()) {
        return 0;
    }

    std::vector<unsigned char> buffer(moduleSize);
    SIZE_T bytesRead = 0;

    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(moduleBase), buffer.data(), moduleSize, &bytesRead)) {
        return 0;
    }

    uintptr_t offset = FindPattern(buffer.data(), static_cast<size_t>(bytesRead), pattern);
    if (!offset) {
        return 0;
    }

    return moduleBase + offset;
}

bool WriteBytes(HANDLE hProcess, uintptr_t address, const void* data, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(address), data, size, &written) &&
        written == size;
}

int main() {
    std::wcout << L"Cyberpunk 2077 FOV changer by Iceeyz\n";
    std::wcout << L"-----------------------------------\n";

    float desiredFov = 0.0f;
    std::cout << "Enter new FOV: ";
    std::cin >> desiredFov;

    if (!std::cin) {
        std::cerr << "Invalid input.\n";
        WaitExit();
        return 1;
    }

    DWORD pid = GetProcessIdByName(L"Cyberpunk2077.exe");
    if (!pid) {
        std::cerr << "couldnt find Cyberpunk2077.exe.\n";
        WaitExit();
        return 1;
    }

    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_READ |
        PROCESS_VM_WRITE |
        PROCESS_VM_OPERATION,
        FALSE,
        pid
    );

    if (!hProcess) {
        std::cerr << "Failed to open process. Try running as administrator.\n";
        WaitExit();
        return 1;
    }

    uintptr_t moduleBase = 0;
    DWORD moduleSize = 0;
    if (!GetModuleInfoByName(pid, L"Cyberpunk2077.exe", moduleBase, moduleSize)) {
        std::cerr << "Failed to get game module info.\n";
        CloseHandle(hProcess);
        WaitExit();
        return 1;
    }

    std::string pattern = "8B 02 89 41 60 C3";
    uintptr_t stubAddr = AobScanModule(hProcess, moduleBase, moduleSize, pattern);

    if (!stubAddr) {
        std::cerr << "couldnt find the FOV writer.\n";
        CloseHandle(hProcess);
        WaitExit();
        return 1;
    }

    uint32_t fovBits = 0;
    std::memcpy(&fovBits, &desiredFov, sizeof(fovBits));

    unsigned char patch[8] = {
        0xC7, 0x41, 0x60,
        static_cast<unsigned char>(fovBits & 0xFF),
        static_cast<unsigned char>((fovBits >> 8) & 0xFF),
        static_cast<unsigned char>((fovBits >> 16) & 0xFF),
        static_cast<unsigned char>((fovBits >> 24) & 0xFF),
        0xC3
    };

    DWORD oldProtect = 0;
    if (!VirtualProtectEx(hProcess, reinterpret_cast<LPVOID>(stubAddr), sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cerr << "Failed to patch memory.\n";
        CloseHandle(hProcess);
        WaitExit();
        return 1;
    }

    if (!WriteBytes(hProcess, stubAddr, patch, sizeof(patch))) {
        std::cerr << "Failed to install patch.\n";
        CloseHandle(hProcess);
        WaitExit();
        return 1;
    }

    DWORD tempProtect = 0;
    VirtualProtectEx(hProcess, reinterpret_cast<LPVOID>(stubAddr), sizeof(patch), oldProtect, &tempProtect);

    std::cout << "FOV patch installed.\n";
    std::cout << "Change the FOV once in the ingame settings to apply it.\n";

    CloseHandle(hProcess);
    WaitExit();
    return 0;
}