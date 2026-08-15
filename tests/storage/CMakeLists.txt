qt_add_executable(ConfigStoreTest ConfigStoreTest.cpp)
target_link_libraries(ConfigStoreTest PRIVATE Qt6::Test bili-core)
add_test(NAME ConfigStoreTest COMMAND ConfigStoreTest)
