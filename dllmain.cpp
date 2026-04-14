#include "pch.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include "MinHook.h"

// Typedefs for the original functions
typedef BOOL(WINAPI* tWinHttpSendRequest)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
typedef BOOL(WINAPI* tWinHttpWriteData)(HINTERNET, LPCVOID, DWORD, LPDWORD);

tWinHttpSendRequest oWinHttpSendRequest = nullptr;
tWinHttpWriteData oWinHttpWriteData = nullptr;

// Helper: check if a buffer contains a substring
bool BufferContains(const char* buffer, size_t bufferLen, const char* search) {

    size_t searchLen = strlen(search);
    if (searchLen > bufferLen) return false;
    for (size_t i = 0; i <= bufferLen - searchLen; ++i) {
        if (memcmp(&buffer[i], search, searchLen) == 0) return true;
    }
    return false;
}

// --- Hooked WriteData ---
BOOL WINAPI hWinHttpWriteData(HINTERNET hReq, LPCVOID lpBuf, DWORD len, LPDWORD written) {
    if (lpBuf && len > 0) {
        const char* data = (const char*)lpBuf;

        // Log only if it looks like JSON (starts with [ or {)
        if (data[0] == '[' || data[0] == '{') {
            printf("\n[JSON OUT] %.*s\n", (int)len, data);

            // Safe blocking: skip only specific triggers
            if (BufferContains(data, len, "take_quest_reward") ||
                BufferContains(data, len, "battle_pass_reward"))
            {
                printf("[!!!] BLOCKED EVENT (pretend sent)\n");

                // Pretend packet was sent to avoid breaking protocol
                if (written) *written = len;
                return TRUE;
            }
        }
    }

    // Let all other packets go through untouched
    return oWinHttpWriteData(hReq, lpBuf, len, written);
}

// --- Hooked SendRequest ---
BOOL WINAPI hWinHttpSendRequest(HINTERNET hReq, LPCWSTR headers, DWORD hLen,
    LPVOID lpOptional, DWORD optLen, DWORD totalLen, DWORD_PTR ctx) {
    if (lpOptional && optLen > 0) {
        printf("\n[SEND REQUEST] Optional data (len=%d): %.*s\n", (int)optLen, (int)optLen, (char*)lpOptional);
    }

    return oWinHttpSendRequest(hReq, headers, hLen, lpOptional, optLen, totalLen, ctx);
}

// --- Initialize Hook ---
void InitializeHook() {
    Sleep(3000); // wait for game to load

    AllocConsole();
    FILE* f; freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Packet Monitar");
    printf("--- PACKET MONITOR ACTIVE ---\n");

    if (MH_Initialize() != MH_OK) return;

    HMODULE hWinHttp = GetModuleHandleA("winhttp.dll");
    if (hWinHttp) {
        LPVOID pSend = GetProcAddress(hWinHttp, "WinHttpSendRequest");
        LPVOID pWrite = GetProcAddress(hWinHttp, "WinHttpWriteData");

        if (pSend) MH_CreateHook(pSend, &hWinHttpSendRequest, (LPVOID*)&oWinHttpSendRequest);
        if (pWrite) MH_CreateHook(pWrite, &hWinHttpWriteData, (LPVOID*)&oWinHttpWriteData);

        MH_EnableHook(MH_ALL_HOOKS);
        printf("[SUCCESS] Monitoring SendRequest and WriteData\n");
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)InitializeHook, nullptr, 0, nullptr);
    }
    return TRUE;
}