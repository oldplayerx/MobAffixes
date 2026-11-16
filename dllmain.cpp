#include <windows.h>
#include <fstream>

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
bool hasWelcomed = false; // Флаг, исключающий повторный показ при возвращении в город/телепортах

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

// Функция вывода приветствия
void ShowWelcomeMessage() {
    if (hasWelcomed) return;

    HMODULE hD2Client = GetModuleHandleA("D2Client.dll");
    if (hD2Client) {
        // Здесь в следующем шаге мы подключим реальный вызов функции D2 для печати строки в чат/экран.
        // Зеленый цвет в движке задается управляющим байтом "\xff\x63\x2" (где 2 — зеленый цвет предметов).
        
        hasWelcomed = true; // Фиксируем, что приветствие показано, чтобы оно больше не дублировалось
    }
}

// Основной рабочий поток плагина
DWORD WINAPI MainThread(LPVOID lpParam) {
    // Ждем, пока D2Client.dll будет полностью загружена игрой
    while (!GetModuleHandleA("D2Client.dll")) {
        Sleep(100);
    }

    // Загружаем конфиг из .ini файла
    LoadConfig();

    // Небольшая задержка, чтобы игра успела инициализировать интерфейс и мир после старта
    Sleep(1000);

    // Если мод включен в конфиге — показываем приветствие сразу при входе
    if (config.enabled) {
        ShowWelcomeMessage();
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        // Запускаем поток инициализации, чтобы не вешать основной поток игры
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, NULL, 0, NULL);
        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
