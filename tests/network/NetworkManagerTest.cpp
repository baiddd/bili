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

        const QUrl url(QString("http://127.0.0.1:%1/file").arg(server.serverPort()));
        manager.startDownload(url, destPath);

        QVERIFY(finishedSpy.wait(5000));
        QFile out(destPath);
        QVERIFY(out.open(QIODevice::ReadOnly));
        QCOMPARE(out.readAll(), QByteArray("hello-world"));
    }
};

QTEST_MAIN(NetworkManagerTest)
#include "NetworkManagerTest.moc"
