#include <QtTest>
#include "library/RomScanner.h"

class RomScannerTest : public QObject {
    Q_OBJECT
private slots:
    void detectsKnownSystemsByExtension() {
        QCOMPARE(RomScanner::detectSystem("Super Mario World (USA).sfc"), QString("snes"));
        QCOMPARE(RomScanner::detectSystem("Super Mario World (USA).smc"), QString("snes"));
        QCOMPARE(RomScanner::detectSystem("Zelda.nes"), QString("nes"));
        QCOMPARE(RomScanner::detectSystem("Pokemon.gba"), QString("gba"));
        QCOMPARE(RomScanner::detectSystem("Pokemon (Rev 1).gb"), QString("gb"));
        QCOMPARE(RomScanner::detectSystem("Pokemon.gbc"), QString("gb"));
        QCOMPARE(RomScanner::detectSystem("Mario64.n64"), QString("n64"));
        QCOMPARE(RomScanner::detectSystem("Mario64.z64"), QString("n64"));
        QCOMPARE(RomScanner::detectSystem("Sonic.md"), QString("genesis"));
        QCOMPARE(RomScanner::detectSystem("Sonic.gen"), QString("genesis"));
    }

    void detectSystemIsCaseInsensitiveOnExtension() {
        QCOMPARE(RomScanner::detectSystem("Zelda.NES"), QString("nes"));
    }

    void detectSystemReturnsEmptyForUnknownExtension() {
        QCOMPARE(RomScanner::detectSystem("readme.txt"), QString(""));
        QCOMPARE(RomScanner::detectSystem("noextension"), QString(""));
    }

    void cleanTitleStripsTagsAndExtension() {
        QCOMPARE(RomScanner::cleanTitle("Super Mario World (USA).sfc"), QString("Super Mario World"));
        QCOMPARE(RomScanner::cleanTitle("Sonic the Hedgehog (Europe) (Rev 1).md"), QString("Sonic the Hedgehog"));
        QCOMPARE(RomScanner::cleanTitle("Contra [!].nes"), QString("Contra"));
        QCOMPARE(RomScanner::cleanTitle("Kirby's Dream Land (USA, Europe).gb"), QString("Kirby's Dream Land"));
    }

    void cleanTitleHandlesNoTagsGracefully() {
        QCOMPARE(RomScanner::cleanTitle("Tetris.gb"), QString("Tetris"));
    }
};

QTEST_MAIN(RomScannerTest)
#include "RomScannerTest.moc"
