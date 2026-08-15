qt_add_executable(RomScannerTest RomScannerTest.cpp)
target_link_libraries(RomScannerTest PRIVATE Qt6::Test bili-core)
add_test(NAME RomScannerTest COMMAND RomScannerTest)
