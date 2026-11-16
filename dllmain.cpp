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
    // Создаем монстра стандартным вызовом движка
    void* pMonsterUnit = OriginalSpawnMonster(RoomCoordStruct, MonsterId, X, Y, a5, a6, a7);

    if (pMonsterUnit && config.enabled) {
        int difficulty = GetGameDifficulty();

        // Проверяем шанс выпадения аффиксов по конфигу для текущей сложности
        if (RollAffixChance(difficulty)) {
            int maxAffixes = GetMaxAllowedAffixes(difficulty);
            int affixesCount = (rand() % maxAffixes) + 1;

            // Структурная обработка UnitAny монстра для назначения аффиксов:
            // В v1.10 по смещению внутри объекта монстра хранятся флаги элитности и массив модификаторов.
            // Присваиваем базовый тип чемпиона/уника, чтобы активировать слоты под аффиксы:
            DWORD* pUnit = (DWORD*)pMonsterUnit;
            if (pUnit) {
                // Пример безопасного доступа к дочерней структуре MonsterData (смещение зависит от сборки, 
                // стандартно pUnit[3] указывает на специфичные данные типа юнита)
                unsigned char* pMonsterData = (unsigned char*)pUnit[3];
                if (pMonsterData) {
                    // Установка флага элитности (например, режим чемпиона с уникальным цветом текста)
                    // pMonsterData[0] |= 0x02; 

                    // Генерация и запись случайных ID аффиксов в структуру моба
                    for (int i = 0; i < affixesCount; ++i) {
                        // ID ванильных аффиксов в Diablo II (например, 1 до 50: Extra Strong, Cursed и т.д.)
                        unsigned char randomAffixId = (rand() % 40) + 1; 
                        
                        // Записываем аффикс в соответствующий слот структуры моба
                        // pMonsterData[4 + i] = randomAffixId;
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
