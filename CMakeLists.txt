cmake_minimum_required(VERSION 3.15)
project(MobAffixes CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_GENERATOR_PLATFORM "Win32")

# Автоматически скачиваем и подключаем MinHook
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(minhook)

# Создаем DLL библиотеку
add_library(MobAffixes SHARED
    dllmain.cpp
)

# Исправлено имя цели на libMinHook
target_link_libraries(MobAffixes PRIVATE libMinHook)
target_include_directories(MobAffixes PRIVATE ${minhook_SOURCE_DIR}/include)

set_target_properties(MobAffixes PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
