add_library(bili-core STATIC)
target_link_libraries(bili-core PUBLIC Qt6::Core)
target_include_directories(bili-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
