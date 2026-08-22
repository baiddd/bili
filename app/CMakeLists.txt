# GameMenuOverlay vit ici et non dans core/ (contrairement à son cousin
# GameWindowEmbedder) parce qu'il manipule un QQuickWindow : le mettre dans
# `bili-core` obligerait à ajouter Qt6::Quick aux dépendances de cette
# bibliothèque, qui ne lie volontairement que Core/Sql/Network/Gui/Concurrent.
# C'est la même frontière que celle qui fait déjà récupérer le WId de la
# fenêtre racine par main.cpp plutôt que par core/.
qt_add_executable(Bili WIN32 main.cpp GameMenuOverlay.cpp)

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
    ../ui/screens/ThemeScreen.qml
    ../ui/theme/Theme.qml
)

# Alias every file to its path relative to ui/ so it lands at a flat,
# predictable resource path (:/Bili/<alias>) instead of climbing out with
# `..` (see Task 1 report for the mkdir/qmldir failure this avoids).
foreach(qml_file ${BILI_QML_FILES})
    string(REGEX REPLACE "^\\.\\./ui/" "" qml_alias ${qml_file})
    set_source_files_properties(${qml_file} PROPERTIES
        QT_RESOURCE_ALIAS ${qml_alias}
        QT_QML_SKIP_CACHEGEN TRUE
    )
endforeach()

set_source_files_properties(../ui/theme/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(Bili
    URI Bili
    VERSION 1.0
    QML_FILES ${BILI_QML_FILES}
)

target_link_libraries(Bili PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    bili-core
)

# EmulatorProvider::sevenZipExecutablePath() resolves "7za" via
# QStandardPaths::findExecutable("7za", {QCoreApplication::applicationDirPath()})
# -- passing a non-empty paths list makes Qt search *only* those paths, not
# also PATH, so dev-env.ps1 putting platform/windows/tools on PATH never
# actually helps this specific lookup (found the hard way during Task 8's
# manual verification: every RetroArch install silently failed immediately
# after a real, fully-successful download, because a plain `cmake --build`
# never put 7za.exe next to Bili.exe -- only publish_windows.ps1's packaging
# step did, via its own separate `Copy-Item ... tools\7za.exe`). Mirror that
# copy here so a plain dev build is functionally complete too, not just the
# published one.
add_custom_command(TARGET Bili POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/platform/windows/tools/7za.exe"
        "$<TARGET_FILE_DIR:Bili>/7za.exe"
)
