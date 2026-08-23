# Premier dossier de tests portant sur app/ et non sur core/ : GameMenuOverlay
# vit dans app/ parce qu'il dépend de Qt6::Quick, que `bili-core` ne lie
# volontairement pas (voir app/GameMenuOverlay.h). Le test compile donc
# directement le .cpp de app/ plutôt que de lier une bibliothèque.
#
# Seule la logique QImage (isUniformlyBlack) est testée ici ; tout le reste de
# la classe manipule deux fenêtres natives réelles et reste vérifié à la main
# (voir le rapport de la Task 3).
qt_add_executable(GameMenuOverlayTest
    GameMenuOverlayTest.cpp
    ${CMAKE_SOURCE_DIR}/app/GameMenuOverlay.cpp
)
target_include_directories(GameMenuOverlayTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(GameMenuOverlayTest PRIVATE Qt6::Test Qt6::Gui Qt6::Quick)
add_test(NAME GameMenuOverlayTest COMMAND GameMenuOverlayTest)
