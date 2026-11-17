#include <windows.h>
#include <fstream>
#include <cstdlib>
#include <ctime>

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
    // Теперь здесь безопасный вызов оригинала через наш трамплин без зацикливания
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
                    pMonsterData[0] |= 0x02; // Флаг элитности
                    unsigned char* pAffixesArray = pMonsterData + 0x08;
                    
                    for (int i = 0; i < affixesCount; ++i) {
                        unsigned char affixId = (unsigned char)((rand() % 30) + 1);
                        pAffixesArray[i] = affixId;
                        WriteLog("Assigned affix ID: %d to slot %d", affixId, i);
                    }
                    WriteLog("Affixes applied successfully!");
                }
            }
        }
    }

    return pMonsterUnit;
}

// Создание безопасного трамплина для обхода падений
bool InstallTrampolineHook(DWORD targetAddress, void* detourFunction, void** originalFunction) {
    DWORD oldProtect;
    if (!VirtualProtect((LPVOID)targetAddress, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    // Выделяем память под буфер трамплина
    void* trampoline = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline) {
        VirtualProtect((LPVOID)targetAddress, 5, oldProtect, &oldProtect);
        return false;
    }

    // 1. Копируем первые 5 байт оригинальной функции в трамплин
    memcpy(trampoline, (void*)targetAddress, 5);

    // 2. Добавляем JMP из трамплина обратно в оригинальную функцию (на targetAddress + 5)
    DWORD trampolineJmpLocation = (DWORD)trampoline + 5;
    DWORD targetContinueAddress = targetAddress + 5;
    *(unsigned char*)trampolineJmpLocation = 0xE9;
    *(DWORD*)(trampolineJmpLocation + 1) = targetContinueAddress - (trampolineJmpLocation + 5);

    // Сохраняем указатель на трамплин как оригинальную функцию
    *originalFunction = trampoline;

    // 3. Ставим JMP из оригинальной функции в наш перехватчик (detour)
    DWORD relativeAddress = (DWORD)detourFunction - (targetAddress + 5);
    *(unsigned char*)targetAddress = 0xE9;
    *(DWORD*)(targetAddress + 1) = relativeAddress;

    VirtualProtect((LPVOID)targetAddress, 5, oldProtect, &oldProtect);
    return true;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    srand(static_cast<unsigned int>(time(NULL)));

    while (!GetModuleHandleA("D2Game.dll") || !GetModuleHandleA("D2Common.dll")) {
        Sleep(100);
    }

    LoadConfig();

    HMODULE hD2Game = GetModuleHandleA("D2Game.dll");
    if (hD2Game) {
        DWORD targetAddress = (DWORD)hD2Game + 0x51E20;
        WriteLog("D2Game base: 0x%X, Target address: 0x%X", (DWORD)hD2Game, targetAddress);

        if (InstallTrampolineHook(targetAddress, &DetourD2GameSpawnMonster, (void**)&OriginalSpawnMonster)) {
            WriteLog("Trampoline hook installed successfully!");
        } else {
            WriteLog("ERROR: Failed to install trampoline hook");
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
        WriteLog("--- Detach MobAffixes ---");
        break;
    }
    return TRUE;
}
