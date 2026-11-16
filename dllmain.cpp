#include <windows.h>
#include <fstream>

// Путь к нашему конфигу будет рядом с DLL (в папке с игрой или плагинами)
char iniPath[MAX_PATH];

void InitializeConfig() {
    // Получаем путь к папке, где лежит наша DLL
    HMODULE hModule = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)InitializeConfig,
        &hModule
    );
    
    GetModuleFileNameA(hModule, iniPath, MAX_PATH);
    // Обрезаем имя файла, оставляя путь к папке
    char* lastSlash = strrchr(iniPath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
    }
    strcat_s(iniPath, "MobAffixes.ini");

    // Если файла конфигурации еще нет — создаем дефолтный
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
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // Отключаем лишние уведомления потоков для оптимизации
        DisableThreadLibraryCalls(hModule);
        
        // Инициализируем конфиг при старте игры
        InitializeConfig();
        
        // Здесь в будущем мы будем ставить хуки на спавна мобов
        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
