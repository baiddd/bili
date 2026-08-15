qt_add_executable(ScreenManagerTest ScreenManagerTest.cpp)
target_link_libraries(ScreenManagerTest PRIVATE Qt6::Test bili-core)
add_test(NAME ScreenManagerTest COMMAND ScreenManagerTest)
