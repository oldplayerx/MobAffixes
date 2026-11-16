#include <windows.h>
#include <fstream>
#include <MinHook.h>

struct ModConfig {
    int enabled;
    int chanceNormal;
    int chanceNightmare;
    int chanceHell;
    int maxAffixesNormal;
    int maxAffixesNightmare;
    int maxAffixesHell;
};

ModConfig config = { 1, 15, 35, 60, 1, 2, 3 };
char iniPath[MAX_PATH];
bool hasWelcomed = false;

void LoadConfig() {
    HMODULE hModule = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)LoadConfig,
        &hModule
    );
    
    GetModuleFileNameA(hModule, iniPath, MAX_PATH);
    char* lastSlash = strrchr(iniPath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
    }
    strcat_s(iniPath, "MobAffixes.ini");

    std::ifstream testFile(iniPath);
    if (!testFile.is_open()) {
        std::ofstream outFile(iniPath);
        if (outFile.is_open()) {
            outFile << "[General]\n";
            outFile << "Enabled = 1\n\n";
            outFile << "[Chances]\n";
            outFile << "Normal = 15\n";
            outFile << "Nightmare = 35\n";
            outFile << "Hell = 60\n\n";
            outFile << "[MaxAffixes]\n";
            outFile << "Normal = 1\n";
            outFile << "Nightmare = 2\n";
            outFile << "Hell = 3\n";
            outFile.close();
        }
    }

    config.enabled = GetPrivateProfileIntA("General", "Enabled", 1, iniPath);
    config.chanceNormal = GetPrivateProfileIntA("Chances", "Normal", 15, iniPath);
    config.chanceNightmare = GetPrivateProfileIntA("Chances", "Nightmare", 35, iniPath);
    config.chanceHell = GetPrivateProfileIntA("Chances", "Hell", 60, iniPath);
    config.maxAffixesNormal = GetPrivateProfileIntA("MaxAffixes", "Normal", 1, iniPath);
    config.maxAffixesNightmare = GetPrivateProfileIntA("MaxAffixes", "Nightmare", 2, iniPath);
    config.maxAffixesHell = GetPrivateProfileIntA("MaxAffixes", "Hell", 3, iniPath);
}

// Указатель на оригинальную функцию печати текста для v1.10f
typedef void(__fastcall* pfnPrintGameString)(wchar_t* wszText, int nColor);
pfnPrintGameString OriginalPrintGameString = NULL;

void __fastcall DetourPrintGameString(wchar_t* wszText, int nColor) {
    if (OriginalPrintGameString) {
        OriginalPrintGameString(wszText, nColor);
    }
}

// Безопасная функция вывода приветствия
void ShowWelcomeMessage() {
    if (hasWelcomed || !OriginalPrintGameString) return;

    HMODULE hD2Client = GetModuleHandleA("D2Client.dll");
    if (hD2Client) {
        // Проверяем указатель на локального игрока в D2Client.dll (v1.10f смещение переменной игрока ~0x11BB2C или аналогичные)
        // Если указатель не равен нулю, значит мы точно внутри игры, а не в меню
        DWORD* pPlayer = (DWORD*)((DWORD)hD2Client + 0x11BB2C); // базовый указатель на структуру игрока в клиенте 1.10
        if (pPlayer && *pPlayer != 0) {
            wchar_t message[256];
            wsprintfW(message, L"\xff\x63\x2[MobAffixes] Loaded successfully for v1.10f!");
            OriginalPrintGameString(message, 0);
            hasWelcomed = true;
        }
    }
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    while (!GetModuleHandleA("D2Client.dll")) {
        Sleep(100);
    }

    LoadConfig();

    if (MH_Initialize() == MH_OK) {
        HMODULE hD2Client = GetModuleHandleA("D2Client.dll");
        if (hD2Client) {
            LPVOID pTarget = (LPVOID)((DWORD)hD2Client + 0x6F8A0);
            
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(pTarget, &mbi, sizeof(mbi)) && (mbi.State == MEM_COMMIT)) {
                if (MH_CreateHook(pTarget, &DetourPrintGameString, reinterpret_cast<LPVOID*>(&OriginalPrintGameString)) == MH_OK) {
                    MH_EnableHook(pTarget);
                }
            }
        }
    }

    // Цикл ожидания входа в игровой мир
    while (true) {
        Sleep(2000); // Проверяем каждые 2 секунды, чтобы не грузить процессор
        if (!config.enabled) continue;

        if (!hasWelcomed) {
            ShowWelcomeMessage();
        }
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, NULL, 0, NULL);
        break;

    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
