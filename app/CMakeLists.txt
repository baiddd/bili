qt_add_executable(bili-frontend main.cpp)

# ../ui/Main.qml lives outside this module's source dir (app/); without an
# explicit alias, Qt embeds it at :/Bili/ui/Main.qml while the generated
# qmldir entry resolves the ".." relatively and looks for :/ui/Main.qml,
# a mismatch that breaks loadFromModule(). Flatten the resource path so
# both agree on :/Bili/Main.qml.
set_source_files_properties(../ui/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml
)

qt_add_qml_module(bili-frontend
    URI Bili
    VERSION 1.0
    QML_FILES ../ui/Main.qml
    NO_CACHEGEN
)

target_link_libraries(bili-frontend PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    bili-core
)
