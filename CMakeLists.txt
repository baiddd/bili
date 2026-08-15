cmake_minimum_required(VERSION 3.21)
project(BiliFrontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Sql Network Test)

enable_testing()

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(tests)
