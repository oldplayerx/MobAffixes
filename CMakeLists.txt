cmake_minimum_required(VERSION 3.15)
project(MobAffixes C CXX)

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

# Создаем DLL и компилируем все необходимые файлы MinHook напрямую
add_library(MobAffixes SHARED
    dllmain.cpp
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)

# Указываем пути к заголовкам MinHook
target_include_directories(MobAffixes PRIVATE
    ${minhook_SOURCE_DIR}/include
    ${minhook_SOURCE_DIR}/src
    ${minhook_SOURCE_DIR}/src/hde
)

set_target_properties(MobAffixes PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
