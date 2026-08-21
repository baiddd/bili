qt_add_executable(EmulatorProviderTest EmulatorProviderTest.cpp)
target_link_libraries(EmulatorProviderTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME EmulatorProviderTest COMMAND EmulatorProviderTest)

qt_add_executable(EmulatorCatalogTest EmulatorCatalogTest.cpp)
target_link_libraries(EmulatorCatalogTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME EmulatorCatalogTest COMMAND EmulatorCatalogTest)
