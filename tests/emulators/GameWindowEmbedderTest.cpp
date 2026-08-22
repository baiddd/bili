// tests/emulators/GameWindowEmbedderTest.cpp
#include <windows.h> // HWND/RECT/GetWindow/GetParent/GetClientRect, pour les assertions
#include <QTest>
#include <QProcess>
#include <QWindow>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QFile>
#include "emulators/GameWindowEmbedder.h"

class GameWindowEmbedderTest : public QObject {
    Q_OBJECT
private slots:
    void embedFindsAndReparentsAndResizesRealWindow();
    void embedFailsWhenNoWindowEverAppears();
    void resizeToHostFollowsHostClientRectChange();
};

// Chemin du stand-in Win32 : compilé dans le même dossier de build que ce
// test (voir tests/emulators/CMakeLists.txt), donc à côté de ce binaire.
static QString standInPath() {
    return QCoreApplication::applicationDirPath() + "/TestGuiWindowStandIn.exe";
}

void GameWindowEmbedderTest::embedFindsAndReparentsAndResizesRealWindow() {
    QVERIFY(QFile::exists(standInPath()));

    // Fenêtre hôte réelle pour ce test (joue le rôle de la fenêtre de Bili).
    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    host.show();

    QProcess process;
    process.start(standInPath(), {});
    QVERIFY(process.waitForStarted(3000));

    GameWindowEmbedder embedder;
    embedder.setPollTimeoutForTesting(3000);
    QVERIFY(embedder.embed(process.processId(), host.winId()));

    // La zone cliente de l'hôte fait 800x600 -- la fenêtre intégrée doit
    // avoir été redimensionnée pour la remplir exactement.
    HWND childHwnd = nullptr;
    for (HWND candidate = GetWindow(reinterpret_cast<HWND>(host.winId()), GW_CHILD);
         candidate; candidate = GetWindow(candidate, GW_HWNDNEXT)) {
        childHwnd = candidate;
    }
    QVERIFY(childHwnd != nullptr);
    QVERIFY(GetParent(childHwnd) == reinterpret_cast<HWND>(host.winId()));

    RECT childRect;
    QVERIFY(GetClientRect(childHwnd, &childRect));
    QCOMPARE(childRect.right - childRect.left, 800);
    QCOMPARE(childRect.bottom - childRect.top, 600);

    process.kill();
    process.waitForFinished(3000);
}

void GameWindowEmbedderTest::embedFailsWhenNoWindowEverAppears() {
    // whoami.exe est un vrai exécutable système qui démarre et se termine
    // quasi immédiatement sans jamais créer de fenêtre -- même stand-in
    // "process réel sans fenêtre" que celui déjà utilisé par
    // EmulatorProviderTest pour un autre scénario.
    const QString systemRoot = qEnvironmentVariable("SystemRoot", "C:/Windows");
    const QString whoami = systemRoot + "/System32/whoami.exe";
    QVERIFY(QFile::exists(whoami));

    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    host.show();

    QProcess process;
    process.start(whoami, {});
    QVERIFY(process.waitForStarted(3000));
    process.waitForFinished(3000); // whoami.exe se termine vite -- PID toujours valide pour embed()

    GameWindowEmbedder embedder;
    embedder.setPollTimeoutForTesting(500); // court, pour ne pas ralentir la suite
    QVERIFY(!embedder.embed(process.processId(), host.winId()));
}

void GameWindowEmbedderTest::resizeToHostFollowsHostClientRectChange() {
    QVERIFY(QFile::exists(standInPath()));

    QWindow host;
    host.setGeometry(0, 0, 800, 600);
    host.create();
    host.show();

    QProcess process;
    process.start(standInPath(), {});
    QVERIFY(process.waitForStarted(3000));

    GameWindowEmbedder embedder;
    embedder.setPollTimeoutForTesting(3000);
    QVERIFY(embedder.embed(process.processId(), host.winId()));

    host.setGeometry(0, 0, 400, 300);
    embedder.resizeToHost(host.winId());

    HWND childHwnd = GetWindow(reinterpret_cast<HWND>(host.winId()), GW_CHILD);
    QVERIFY(childHwnd != nullptr);
    RECT childRect;
    QVERIFY(GetClientRect(childHwnd, &childRect));
    QCOMPARE(childRect.right - childRect.left, 400);
    QCOMPARE(childRect.bottom - childRect.top, 300);

    process.kill();
    process.waitForFinished(3000);
}

QTEST_MAIN(GameWindowEmbedderTest)
#include "GameWindowEmbedderTest.moc"
