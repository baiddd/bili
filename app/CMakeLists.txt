qt_add_executable(Bili WIN32 main.cpp)

# QML files under ../ui live outside this module's source dir (app/);
# without an explicit alias, Qt embeds them at :/Bili/ui/<path> while the
# generated qmldir entry resolves the ".." relatively and looks for
# :/ui/<path>, a mismatch that breaks loadFromModule(). It also produces an
# invalid `<target>_..` intermediate directory name at build time. Flatten
# every entry's resource path relative to ui/ so both agree on
# :/Bili/<alias>, and skip cachegen module-wide (NO_CACHEGEN) to avoid the
# same mkdir bug for every file, not just Main.qml. This list is the single
# place later tasks (this one, Task 6, ...) should append to when adding a
# QML file — never hand-write a new qt_add_qml_module call.
set(BILI_QML_FILES
    ../ui/Main.qml
    ../ui/screens/BootScreen.qml
    ../ui/screens/MainMenuScreen.qml
    ../ui/screens/GameListScreen.qml
    ../ui/screens/GameDetailsScreen.qml
    ../ui/screens/SettingsScreen.qml
    ../ui/screens/DownloadManagerScreen.qml
    ../ui/screens/ScraperManagerScreen.qml
    ../ui/screens/EmulatorManagerScreen.qml
    ../ui/screens/FirstLaunchSetupScreen.qml
    ../ui/screens/AboutScreen.qml
    ../ui/theme/Theme.qml
)

# Alias every file to its path relative to ui/ so it lands at a flat,
# predictable resource path (:/Bili/<alias>) instead of climbing out with
# `..` (see Task 1 report for the mkdir/qmldir failure this avoids).
foreach(qml_file ${BILI_QML_FILES})
    string(REGEX REPLACE "^\\.\\./ui/" "" qml_alias ${qml_file})
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()

qt_add_qml_module(Bili
    URI Bili
    VERSION 1.0
    QML_FILES ${BILI_QML_FILES}
    NO_CACHEGEN
)

target_link_libraries(Bili PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    bili-core
)
