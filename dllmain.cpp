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

    // Автоматическое создание файла конфигурации, если его нет
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
    testFile.close();

    config.enabled = GetPrivateProfileIntA("General", "Enabled", 1, iniPath);
    config.chanceNormal = GetPrivateProfileIntA("Chances", "Normal", 15, iniPath);
    config.chanceNightmare = GetPrivateProfileIntA("Chances", "Nightmare", 35, iniPath);
    config.chanceHell = GetPrivateProfileIntA("Chances", "Hell", 60, iniPath);
    config.maxAffixesNormal = GetPrivateProfileIntA("MaxAffixes", "Normal", 1, iniPath);
    config.maxAffixesNightmare = GetPrivateProfileIntA("MaxAffixes", "Nightmare", 2, iniPath);
    config.maxAffixesHell = GetPrivateProfileIntA("MaxAffixes", "Hell", 3, iniPath);
}

// Проверка шанса в зависимости от сложности игры
bool RollAffixChance(int difficulty) {
    int chance = config.chanceNormal;
    if (difficulty == 1) chance = config.chanceNightmare;
    else if (difficulty >= 2) chance = config.chanceHell;

    return (rand() % 100) < chance;
}

int GetMaxAllowedAffixes(int difficulty) {
    if (difficulty == 1) return config.maxAffixesNightmare;
    else if (difficulty >= 2) return config.maxAffixesHell;
    return config.maxAffixesNormal;
}

// Получение текущей сложности из памяти D2Game.dll v1.10f
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

// Указатель на оригинальную функцию спавна монстра в D2Game.dll
typedef void* (__fastcall* pfnD2GameSpawnMonster)(DWORD RoomCoordStruct, int MonsterId, int X, int Y, int a5, int a6, int a7);
pfnD2GameSpawnMonster OriginalSpawnMonster = NULL;

// Наш перехватчик (Detour) спавна монстров
void* __fastcall DetourD2GameSpawnMonster(DWORD RoomCoordStruct, int MonsterId, int X, int Y, int a5, int a6, int a7) {
    void* pMonsterUnit = OriginalSpawnMonster(RoomCoordStruct, MonsterId, X, Y, a5, a6, a7);

    if (pMonsterUnit && config.enabled) {
        int difficulty = GetGameDifficulty();

        if (RollAffixChance(difficulty)) {
            int maxAffixes = GetMaxAllowedAffixes(difficulty);
            int affixesCount = (rand() % maxAffixes) + 1;

            DWORD* pUnit = (DWORD*)pMonsterUnit;
            if (pUnit) {
                // pUnit[3] в v1.10 указывает на структуру MonsterData внутри UnitAny
                unsigned char* pMonsterData = (unsigned char*)pUnit[3];
                if (pMonsterData) {
                    // Устанавливаем флаг элитности (Champion / Unique), чтобы движок выделил слоты под аффиксы
                    pMonsterData[0] |= 0x02; // Флаг уникального/элитного типа моба

                    // Заполняем слоты аффиксов реальными ID из таблицы модификаторов D2 (от 1 до 35)
                    // В памяти структуры MonsterData массив аффиксов начинается с определенных смещений
                    unsigned char* pAffixesArray = pMonsterData + 0x08; // Смещение массива аффиксов для v1.10
                    
                    for (int i = 0; i < affixesCount; ++i) {
                        // Генерируем случайный ванильный ID аффикса (например, Extra Strong = 1, Cursed = 3 и т.д.)
                        unsigned char affixId = (unsigned char)((rand() % 30) + 1);
                        pAffixesArray[i] = affixId;
                    }
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
            LPVOID pTarget = (LPVOID)((DWORD)hD2Game + 0x51E20);

            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(pTarget, &mbi, sizeof(mbi)) && (mbi.State == MEM_COMMIT)) {
                if (MH_CreateHook(pTarget, &DetourD2GameSpawnMonster, reinterpret_cast<LPVOID*>(&OriginalSpawnMonster)) == MH_OK) {
                    MH_EnableHook(pTarget);
                }
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
        break;
    }
    return TRUE;
}
