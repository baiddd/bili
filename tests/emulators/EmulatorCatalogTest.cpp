#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QFile>
#include "emulators/EmulatorCatalog.h"
#include "network/NetworkManager.h"

class EmulatorCatalogTest : public QObject {
    Q_OBJECT
private slots:
    void parsesAValidManifest() {
        // NB: written as an escaped (non-raw) string literal rather than
        // R"(...)" because Qt's moc mis-tokenizes "//" inside a raw string
        // literal as a line comment (verified against this project's Qt
        // 6.8.3/MinGW moc), which desyncs its brace counter and makes it
        // silently skip this Q_OBJECT class ("No relevant classes found").
        const QByteArray body =
            "{\"retroarch\": {\"version\": \"1.22.2\", \"windows_x64_url\": \"https://example.invalid/RetroArch.7z\"},"
            "\"cores\": {"
            "\"nes\": {\"core\": \"fceumm\", \"url\": \"https://example.invalid/fceumm.zip\"},"
            "\"snes\": {\"core\": \"snes9x\", \"url\": \"https://example.invalid/snes9x.zip\"}"
            "}}";

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server, &body]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: "
                + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        QSignalSpy readySpy(&catalog, &EmulatorCatalog::ready);
        QSignalSpy failedSpy(&catalog, &EmulatorCatalog::failed);

        catalog.fetch(QUrl(QString("http://127.0.0.1:%1/manifest.json").arg(server.serverPort())));

        QVERIFY(readySpy.wait(5000));
        QCOMPARE(failedSpy.count(), 0);
        const EmulatorCatalogData data = readySpy.first().at(0).value<EmulatorCatalogData>();
        QCOMPARE(data.retroArchVersion, QString("1.22.2"));
        QCOMPARE(data.retroArchUrl, QUrl("https://example.invalid/RetroArch.7z"));
        QCOMPARE(data.coresBySystem.size(), 2);
        QCOMPARE(data.coresBySystem.value("nes").core, QString("fceumm"));
    }

    void emitsFailedForMalformedJson() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray body = "{ not valid json";
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: "
                + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        QSignalSpy readySpy(&catalog, &EmulatorCatalog::ready);
        QSignalSpy failedSpy(&catalog, &EmulatorCatalog::failed);

        catalog.fetch(QUrl(QString("http://127.0.0.1:%1/manifest.json").arg(server.serverPort())));

        QVERIFY(failedSpy.wait(5000));
        QCOMPARE(readySpy.count(), 0);
        QVERIFY(!failedSpy.first().at(0).toString().isEmpty());
    }

    void emitsFailedWhenDownloadItselfFails() {
        quint16 freePort = 0;
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            freePort = probe.serverPort();
        }

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        QSignalSpy failedSpy(&catalog, &EmulatorCatalog::failed);

        catalog.fetch(QUrl(QString("http://127.0.0.1:%1/nope").arg(freePort)));

        QVERIFY(failedSpy.wait(5000));
        QVERIFY(!failedSpy.first().at(0).toString().isEmpty());
    }

    void removesTempFileOnDownloadFailure() {
        quint16 freePort = 0;
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            freePort = probe.serverPort();
        }

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        QSignalSpy failedSpy(&catalog, &EmulatorCatalog::failed);

        catalog.fetch(QUrl(QString("http://127.0.0.1:%1/nope").arg(freePort)));
        const QString tempPath = catalog.tempPathForTesting();
        QVERIFY(!tempPath.isEmpty());
        QVERIFY(QFile::exists(tempPath));

        QVERIFY(failedSpy.wait(5000));
        QVERIFY(!QFile::exists(tempPath));
    }

    // Regression test (re-review of the final fix wave, sub-project 3):
    // fetch() is now Q_INVOKABLE and called from EmulatorManagerScreen.qml
    // on every screen open, so a second call while the first is still in
    // flight is reachable for the first time. Before the fix, a second
    // fetch() call would overwrite m_tempPath/m_pendingRequestId while the
    // first request was still pending, leaking the first request's own temp
    // file (its own failed-handler branch would `QFile::remove()` only the
    // LATEST m_tempPath, never its own). Uses a QTcpServer that accepts the
    // connection but withholds the response until the test has issued both
    // fetch() calls, so the second call genuinely lands while the first is
    // still in flight -- then asserts only one connection ever reached the
    // server. Verified to fail against the pre-fix code (two connections
    // reach the server) and pass against the fixed code (one connection,
    // second fetch() call is a no-op).
    void secondFetchWhileFirstIsPendingIsIgnored() {
        int connectionCount = 0;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server, &connectionCount]() {
            QTcpSocket *client = server.nextPendingConnection();
            ++connectionCount;
            // Deliberately never respond -- keeps the first request "in
            // flight" for the duration of this test.
            Q_UNUSED(client);
        });

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        const QUrl url(QString("http://127.0.0.1:%1/manifest.json").arg(server.serverPort()));

        catalog.fetch(url);
        // Let the first connection actually reach the server before issuing
        // the second call, so this genuinely exercises "second call while
        // the first request is in flight" rather than a race.
        QTest::qWait(200);
        QCOMPARE(connectionCount, 1);
        const QString firstTempPath = catalog.tempPathForTesting();

        catalog.fetch(url);
        QTest::qWait(200);

        QCOMPARE(connectionCount, 1);
        QCOMPARE(catalog.tempPathForTesting(), firstTempPath);

        // The request is never completed in this test (the server
        // deliberately never responds), so EmulatorCatalog's own
        // finished/failed handlers never run to clean up the first
        // request's temp file -- remove it ourselves rather than leave a
        // scratch artifact behind.
        QFile::remove(firstTempPath);
    }
};

QTEST_MAIN(EmulatorCatalogTest)
#include "EmulatorCatalogTest.moc"
