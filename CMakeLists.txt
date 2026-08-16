cmake_minimum_required(VERSION 3.21)
# C is required alongside CXX for the vendored miniz.c (sub-project 2,
# Task 3, .zip archive scanning) - without it, CMake silently drops .c
# sources from any target instead of erroring, since only the CMAKE_CXX
# compiler cache vars (already configured in CMakePresets.json alongside
# CMAKE_C_COMPILER) get wired up otherwise.
project(Bili LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Sql Network Test Concurrent)
qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(tests)
