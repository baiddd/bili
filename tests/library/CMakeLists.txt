qt_add_executable(RomScannerTest RomScannerTest.cpp)
target_link_libraries(RomScannerTest PRIVATE Qt6::Test bili-core)
add_test(NAME RomScannerTest COMMAND RomScannerTest)

qt_add_executable(LibraryScannerTest LibraryScannerTest.cpp)
target_link_libraries(LibraryScannerTest PRIVATE Qt6::Test bili-core)
add_test(NAME LibraryScannerTest COMMAND LibraryScannerTest)
