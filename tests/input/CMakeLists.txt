qt_add_executable(InputManagerTest InputManagerTest.cpp)
target_link_libraries(InputManagerTest PRIVATE Qt6::Test Qt6::Gui bili-core)
add_test(NAME InputManagerTest COMMAND InputManagerTest)
