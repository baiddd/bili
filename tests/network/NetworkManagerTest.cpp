#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QSignalSpy>
#include "network/NetworkManager.h"

class NetworkManagerTest : public QObject {
    Q_OBJECT
private slots:
    void downloadsFileAndEmitsFinished() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray body = "hello-world";
            const QByteArray response = "HTTP/1.1 200 OK\r\n"
                "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        QTemporaryDir dir;
        const QString destPath = dir.path() + "/out.bin";
        NetworkManager manager;
        QSignalSpy finishedSpy(&manager, &NetworkManager::finished);
        QSignalSpy progressSpy(&manager, &NetworkManager::progress);

        const QUrl url(QString("http://127.0.0.1:%1/file").arg(server.serverPort()));
        manager.startDownload(url, destPath);

        QVERIFY(finishedSpy.wait(5000));
        QFile out(destPath);
        QVERIFY(out.open(QIODevice::ReadOnly));
        QCOMPARE(out.readAll(), QByteArray("hello-world"));
        QVERIFY(!progressSpy.isEmpty());
    }

    void cancelDownloadEmitsFailedNotFinished() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        // Accept the connection but never write a response, simulating a
        // stalled/slow server so the download is still in flight when we
        // cancel it.
        connect(&server, &QTcpServer::newConnection, this, [&server]() {
            QTcpSocket *client = server.nextPendingConnection();
            Q_UNUSED(client);
        });

        QTemporaryDir dir;
        const QString destPath = dir.path() + "/cancelled.bin";
        NetworkManager manager;
        QSignalSpy finishedSpy(&manager, &NetworkManager::finished);
        QSignalSpy failedSpy(&manager, &NetworkManager::failed);

        const QUrl url(QString("http://127.0.0.1:%1/slow").arg(server.serverPort()));
        const int id = manager.startDownload(url, destPath);
        manager.cancelDownload(id);

        // abort() can emit finished()/failed() synchronously (the request
        // may not have progressed past the "connecting" stage yet), so only
        // wait if it hasn't already fired -- QSignalSpy::wait() waits for a
        // *new* occurrence and would time out if the signal already landed.
        if (failedSpy.isEmpty()) {
            QVERIFY(failedSpy.wait(5000));
        }
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(failedSpy.first().at(0).toInt(), id);
        QVERIFY(!failedSpy.first().at(1).toString().isEmpty());
    }

    void downloadFailsForUnreachableHost() {
        quint16 freePort = 0;
        {
            // Bind then immediately close to obtain a port nobody is
            // listening on, so the subsequent connection attempt is
            // refused at the transport level.
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            freePort = probe.serverPort();
        }

        QTemporaryDir dir;
        const QString destPath = dir.path() + "/unreachable.bin";
        NetworkManager manager;
        QSignalSpy finishedSpy(&manager, &NetworkManager::finished);
        QSignalSpy failedSpy(&manager, &NetworkManager::failed);

        const QUrl url(QString("http://127.0.0.1:%1/nope").arg(freePort));
        manager.startDownload(url, destPath);

        QVERIFY(failedSpy.wait(5000));
        QCOMPARE(finishedSpy.count(), 0);
        QVERIFY(!failedSpy.first().at(1).toString().isEmpty());
    }
};

QTEST_MAIN(NetworkManagerTest)
#include "NetworkManagerTest.moc"
