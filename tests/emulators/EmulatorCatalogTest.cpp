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
};

QTEST_MAIN(EmulatorCatalogTest)
#include "EmulatorCatalogTest.moc"
