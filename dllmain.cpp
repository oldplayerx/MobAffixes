#include <windows.h>
#include <fstream>
#include <cstdlib>
#include <ctime>
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
char logPath[MAX_PATH];

void WriteLog(const char* format, ...) {
    FILE* f = NULL;
    fopen_s(&f, logPath, "a");
    if (f) {
        va_list args;
        va_start(args, format);
        vfprintf(f, format, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

// Функция поиска адреса по сигнатуре (Pattern Scanning) в памяти модуля
LPVOID FindPattern(HMODULE hModule, const char* pattern, const char* mask) {
    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));
    
    DWORD base = (DWORD)hModule;
    DWORD size = moduleInfo.SizeOfImage;
    
    DWORD patternLength = (DWORD)strlen(mask);
    
    for (DWORD i = 0; i < size - patternLength; i++) {
        bool found = true;
        for (DWORD j = 0; j < patternLength; j++) {
            if (mask[j] != '?' && pattern[j] != *(char*)(base + i + j)) {
                found = false;
                break;
            }
        }
        if (found) {
            return (LPVOID)(base + i);
        }
    }
    return NULL;
}

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
    
    strcpy_s(logPath, iniPath);
    strcat_s(logPath, "MobAffixes.log");
    strcat_s(iniPath, "MobAffixes.ini");

    WriteLog("--- Init MobAffixes ---");

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
            WriteLog("Created default MobAffixes.ini");
        }
    }
    testFile.close();

    config.enabled = GetPrivateProfileIntA("General", "Enabled", 1, iniPath);
    config.chanceNormal = GetPrivateProfileIntA("Chances", "Normal", 15, iniPath);
    config.chanceNightmare = GetPrivateProfileIntA("Chances", "Nightmare", 35, iniPath);
    config.chanceHell = GetPrivateProfileIntA("Chances", "Hell", 60, iniPath);
    config.maxAffixesNormal = GetPrivateProfileIntA("MaxAffixes", "Normal", 1, iniPath);
    config.maxAffixesNightmare = GetPrivateProfileIntA("MaxAffixes", "Nightmare", 2, iniPath);
    config.maxAffixesHell = GetPrivateProfileIntA("MaxAffixes", "Hell", 3, iniPath);

    WriteLog("Config loaded: Enabled=%d, Normal=%d, Nightmare=%d, Hell=%d", 
        config.enabled, config.chanceNormal, config.chanceNightmare, config.chanceHell);
}

bool RollAffixChance(int difficulty) {
    int chance = config.chanceNormal;
    if (difficulty == 1) chance = config.chanceNightmare;
    else if (difficulty >= 2) chance = config.chanceHell;

    int roll = rand() % 100;
    bool success = roll < chance;
    WriteLog("RollAffixChance: Diff=%d, Chance=%d, Roll=%d, Success=%s", difficulty, chance, roll, success ? "TRUE" : "FALSE");
    return success;
}

int GetMaxAllowedAffixes(int difficulty) {
    if (difficulty == 1) return config.maxAffixesNightmare;
    else if (difficulty >= 2) return config.maxAffixesHell;
    return config.maxAffixesNormal;
}

int GetGameDifficulty() {
    HMODULE hD2Game = GetModuleHandleA("D2Game.dll");
    if (!hD2Game) return 0;
    
    DWORD* pServerGame = (DWORD*)((DWORD)hD2Game + 0x111350);
    if (pServerGame && *pServerGame) {
        unsigned char* pDiff = (unsigned char*)(*pServerGame + 0x24); 
        return (int)(*pDiff);
    }
    return 0;
}

typedef void* (__fastcall* pfnD2GameSpawnMonster)(DWORD RoomCoordStruct, int MonsterId, int X, int Y, int a5, int a6, int a7);
pfnD2GameSpawnMonster OriginalSpawnMonster = NULL;

void* __fastcall DetourD2GameSpawnMonster(DWORD RoomCoordStruct, int MonsterId, int X, int Y, int a5, int a6, int a7) {
    void* pMonsterUnit = OriginalSpawnMonster(RoomCoordStruct, MonsterId, X, Y, a5, a6, a7);

    if (pMonsterUnit && config.enabled) {
        int difficulty = GetGameDifficulty();
        WriteLog("Monster spawned! ID: %d, Difficulty: %d", MonsterId, difficulty);

        if (RollAffixChance(difficulty)) {
            int maxAffixes = GetMaxAllowedAffixes(difficulty);
            int affixesCount = (rand() % maxAffixes) + 1;
            WriteLog("Applying %d affixes to monster ID %d", affixesCount, MonsterId);

            DWORD* pUnit = (DWORD*)pMonsterUnit;
            if (pUnit) {
                unsigned char* pMonsterData = (unsigned char*)pUnit[3];
                if (pMonsterData) {
                    pMonsterData[0] |= 0x02;
                    unsigned char* pAffixesArray = pMonsterData + 0x08;
                    
                    for (int i = 0; i < affixesCount; ++i) {
                        unsigned char affixId = (unsigned char)((rand() % 30) + 1);
                        pAffixesArray[i] = affixId;
                        WriteLog("Assigned affix ID: %d to slot %d", affixId, i);
                    }
                } else {
                    WriteLog("WARNING: pMonsterData is NULL for monster ID %d", MonsterId);
                }
            }
        }
    }

    return pMonsterUnit;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    srand(static_cast<unsigned int>(time(NULL)));

    while (!GetModuleHandleA("D2Game.dll") || !GetModuleHandleA("D2Common.dll")) {
        Sleep(100);
    }

    LoadConfig();

    if (MH_Initialize() == MH_OK) {
        HMODULE hD2Game = GetModuleHandleA("D2Game.dll");
        if (hD2Game) {
            // Ищем функцию по уникальным начальным байтам (сигнатуре) для v1.10f
            // (Заменяем жесткое смещение на автоматический поиск в памяти)
            LPVOID pTarget = FindPattern(hD2Game, "\x55\x8B\xEC\x83\xEC\x10", "xxxxxx");

            if (pTarget) {
                WriteLog("Found SpawnMonster signature at address: 0x%X", (DWORD)pTarget);
                if (MH_CreateHook(pTarget, &DetourD2GameSpawnMonster, reinterpret_cast<LPVOID*>(&OriginalSpawnMonster)) == MH_OK) {
                    MH_EnableHook(pTarget);
                    WriteLog("SpawnMonster hook installed successfully via signature!");
                } else {
                    WriteLog("ERROR: Failed to create hook for SpawnMonster");
                }
            } else {
                WriteLog("ERROR: SpawnMonster signature not found in D2Game.dll");
            }
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
        WriteLog("--- Detach MobAffixes ---");
        break;
    }
    return TRUE;
}
