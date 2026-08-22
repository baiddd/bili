qt_add_executable(EmulatorProviderTest EmulatorProviderTest.cpp)
target_link_libraries(EmulatorProviderTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME EmulatorProviderTest COMMAND EmulatorProviderTest)

qt_add_executable(EmulatorCatalogTest EmulatorCatalogTest.cpp)
target_link_libraries(EmulatorCatalogTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME EmulatorCatalogTest COMMAND EmulatorCatalogTest)

qt_add_executable(RetroArchAutoconfigTest RetroArchAutoconfigTest.cpp)
target_link_libraries(RetroArchAutoconfigTest PRIVATE Qt6::Test bili-core)
add_test(NAME RetroArchAutoconfigTest COMMAND RetroArchAutoconfigTest)

if(WIN32)
    add_executable(TestGuiWindowStandIn WIN32 TestGuiWindowStandIn.cpp)
    # Ce binaire n'est pas une cible Qt (qt_add_executable), donc il ne
    # reçoit pas les défines UNICODE/_UNICODE que Qt ajoute d'habitude --
    # sans elles, RegisterClass/CreateWindowEx résolvent vers les
    # surcharges ANSI (...A) alors que le code utilise des littéraux
    # wide-char (L"..."), ce qui échoue à la compilation.
    target_compile_definitions(TestGuiWindowStandIn PRIVATE UNICODE _UNICODE)
endif()

if(WIN32)
    qt_add_executable(GameWindowEmbedderTest GameWindowEmbedderTest.cpp)
    target_link_libraries(GameWindowEmbedderTest PRIVATE Qt6::Test Qt6::Gui bili-core)
    add_test(NAME GameWindowEmbedderTest COMMAND GameWindowEmbedderTest)
    add_dependencies(GameWindowEmbedderTest TestGuiWindowStandIn)
endif()
