cmake_minimum_required(VERSION 3.15)
project(MobAffixes CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_GENERATOR_PLATFORM "Win32")

# Скачиваем архив MinHook
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(minhook)

# Собираем нашу DLL, включая исходники MinHook напрямую в проект
add_library(MobAffixes SHARED
    dllmain.cpp
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
    ${minhook_SOURCE_DIR}/src/hook.c
)

# Указываем путь к заголовочным файлам MinHook
target_include_directories(MobAffixes PRIVATE ${minhook_SOURCE_DIR}/include)

set_target_properties(MobAffixes PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
