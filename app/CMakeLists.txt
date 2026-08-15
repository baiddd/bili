qt_add_executable(bili-frontend WIN32 main.cpp)

# ../ui/Main.qml lives outside this module's source dir (app/); without an
# explicit alias, Qt embeds it at :/Bili/ui/Main.qml while the generated
# qmldir entry resolves the ".." relatively and looks for :/ui/Main.qml,
# a mismatch that breaks loadFromModule(). Flatten the resource path so
# both agree on :/Bili/Main.qml. QT_QML_SKIP_CACHEGEN is scoped to this one
# source file (unlike the module-wide NO_CACHEGEN option) so precompilation
# still runs normally for every other QML file later tasks add to this
# module; it avoids the same mkdir bug that motivated the alias above.
set_source_files_properties(../ui/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml
    QT_QML_SKIP_CACHEGEN TRUE
)

qt_add_qml_module(bili-frontend
    URI Bili
    VERSION 1.0
    QML_FILES ../ui/Main.qml
)

target_link_libraries(bili-frontend PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    bili-core
)
