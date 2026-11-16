cmake_minimum_required(VERSION 3.15)
project(MobAffixes CXX)

# Устанавливаем стандарт C++ (например, C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Принудительно задаем 32-битную сборку (x86), так как Diablo II v1.10 — x86 игра
set(CMAKE_GENERATOR_PLATFORM "Win32")

# Создаем DLL библиотеку
add_library(MobAffixes SHARED
    dllmain.cpp
)

# Настройка экспорта символов и имен
set_target_properties(MobAffixes PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
