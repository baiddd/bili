cmake_minimum_required(VERSION 3.21)
project(Bili LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Sql Network Test)
qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(tests)
