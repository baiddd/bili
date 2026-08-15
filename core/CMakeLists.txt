add_library(bili-core STATIC)
target_link_libraries(bili-core PUBLIC Qt6::Core Qt6::Sql Qt6::Network)
target_include_directories(bili-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(bili-core PRIVATE storage/ConfigStore.cpp storage/LibraryDatabase.cpp network/NetworkManager.cpp)
