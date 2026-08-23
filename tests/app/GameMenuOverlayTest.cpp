// tests/app/GameMenuOverlayTest.cpp
//
// Couvre la détection d'une capture d'image silencieusement ratée
// (GameMenuOverlay::isUniformlyBlack), qui décide si le menu en jeu affiche
// le voile translucide par-dessus l'image du jeu (Fit::FullClient) ou se
// replie sur le panneau opaque centré (Fit::CenteredPanel).
//
// Pourquoi un test au niveau QImage et pas une capture réelle : le cas à
// couvrir est PrintWindow() qui retourne TRUE en rendant une image
// entièrement noire -- ce qui arrive sur une fenêtre accélérée
// matériellement quand PW_RENDERFULLCONTENT n'est pas honoré, donc sur
// Windows 7/8.0. On ne peut pas le provoquer sur cette machine ; en
// revanche la logique de décision, elle, est purement QImage et se teste
// exactement.
#include <QTest>
#include <QImage>
#include <QPainter>
#include "GameMenuOverlay.h"

class GameMenuOverlayTest : public QObject {
    Q_OBJECT
private slots:
    void detectsAnAllBlackCaptureAsFailed();
    void detectsANearBlackCaptureAsFailed();
    void doesNotFlagARealLookingGameFrame();
    void doesNotFlagAFrameThatIsMostlyBlackLetterboxing();
    void doesNotFlagANullImage();
};

void GameMenuOverlayTest::detectsAnAllBlackCaptureAsFailed() {
    QImage frame(800, 600, QImage::Format_RGB32);
    frame.fill(Qt::black);
    QVERIFY(GameMenuOverlay::isUniformlyBlack(frame));
}

void GameMenuOverlayTest::detectsANearBlackCaptureAsFailed() {
    // Robustesse à un arrondi proche du noir plutôt qu'un #000000 exact.
    QImage frame(800, 600, QImage::Format_RGB32);
    frame.fill(QColor(3, 2, 4));
    QVERIFY(GameMenuOverlay::isUniformlyBlack(frame));
}

void GameMenuOverlayTest::doesNotFlagARealLookingGameFrame() {
    // Une vraie capture de jeu : un fond clair avec des éléments dessus,
    // comme celles observées lors de la vérification manuelle contre
    // RetroArch (fond blanc de l'intro de Pokémon).
    QImage frame(800, 600, QImage::Format_RGB32);
    frame.fill(QColor(0xff, 0xfb, 0xff));
    QPainter painter(&frame);
    painter.fillRect(0, 0, 800, 120, Qt::black);   // bandes de letterboxing
    painter.fillRect(0, 480, 800, 120, Qt::black);
    painter.fillRect(360, 280, 80, 60, Qt::black); // un sprite
    painter.end();
    QVERIFY(!GameMenuOverlay::isUniformlyBlack(frame));
}

void GameMenuOverlayTest::doesNotFlagAFrameThatIsMostlyBlackLetterboxing() {
    // Cas limite qui compte vraiment : un jeu très sombre, entouré de larges
    // bandes noires. La zone de contenu est petite mais bien réelle -- elle
    // doit suffire à ne PAS conclure à une capture ratée, sans quoi tout jeu
    // en 4:3 dans une fenêtre large se replierait à tort sur le panneau.
    QImage frame(800, 600, QImage::Format_RGB32);
    frame.fill(Qt::black);
    QPainter painter(&frame);
    painter.fillRect(200, 150, 400, 300, QColor(40, 60, 90));
    painter.end();
    QVERIFY(!GameMenuOverlay::isUniformlyBlack(frame));
}

void GameMenuOverlayTest::doesNotFlagANullImage() {
    // Une image nulle n'est pas « noire » : il n'y a rien à échantillonner,
    // et ce cas est déjà traité en amont par captureWindowContent().
    QVERIFY(!GameMenuOverlay::isUniformlyBlack(QImage()));
}

QTEST_MAIN(GameMenuOverlayTest)
#include "GameMenuOverlayTest.moc"
