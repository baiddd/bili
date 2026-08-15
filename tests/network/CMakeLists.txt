qt_add_executable(NetworkManagerTest NetworkManagerTest.cpp)
target_link_libraries(NetworkManagerTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME NetworkManagerTest COMMAND NetworkManagerTest)
