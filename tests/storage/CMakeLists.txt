qt_add_executable(ConfigStoreTest ConfigStoreTest.cpp)
target_link_libraries(ConfigStoreTest PRIVATE Qt6::Test bili-core)
add_test(NAME ConfigStoreTest COMMAND ConfigStoreTest)

qt_add_executable(LibraryDatabaseTest LibraryDatabaseTest.cpp)
target_link_libraries(LibraryDatabaseTest PRIVATE Qt6::Test Qt6::Sql bili-core)
add_test(NAME LibraryDatabaseTest COMMAND LibraryDatabaseTest)
