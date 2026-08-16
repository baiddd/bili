find_package(SDL2 REQUIRED)

# miniz (vendored, MIT-licensed, single .c/.h pair from
# https://github.com/richgel999/miniz release 3.1.2) reads .zip archive
# contents for RomScanner's archive-scanning support (sub-project 2,
# Task 3). It's a drop-in amalgamated source pair, not a system-installed
# library, so it's vendored directly under third_party/ and compiled as a
# small static lib rather than found via find_package/CMAKE_PREFIX_PATH -
# no DLL, no extra toolchain dependency.
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)

add_library(bili-core STATIC)
target_link_libraries(bili-core PUBLIC Qt6::Core Qt6::Sql Qt6::Network Qt6::Gui SDL2::SDL2 miniz)
target_include_directories(bili-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(bili-core PRIVATE library/RomScanner.cpp storage/ConfigStore.cpp storage/LibraryDatabase.cpp storage/RomSourcesStore.cpp network/NetworkManager.cpp ui/ScreenManager.cpp input/InputManager.cpp input/GamepadBridge.cpp system/SystemController.cpp emulators/StubEmulatorProvider.cpp scraper/StubScraperProvider.cpp netplay/StubNetplaySession.cpp)

# Header-only QML bridge wrappers (no companion .cpp): AUTOMOC only moc's
# headers that are either paired by basename with a .cpp or explicitly
# listed as sources, so list them here to get their moc_*.cpp generated.
target_sources(bili-core PRIVATE emulators/EmulatorProviderQmlBridge.h scraper/ScraperProviderQmlBridge.h netplay/NetplaySessionQmlBridge.h)
