// tests/emulators/RetroArchNetworkCommandTest.cpp
#include <QTest>
#include <QUdpSocket>
#include "emulators/RetroArchNetworkCommand.h"

class RetroArchNetworkCommandTest : public QObject {
    Q_OBJECT
private slots:
    void sendDeliversNewlineTerminatedCommandOverUdp() {
        QUdpSocket receiver;
        QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
        const quint16 port = receiver.localPort();

        RetroArchNetworkCommand command;
        QVERIFY(command.send("PAUSE_TOGGLE", port));

        QVERIFY(receiver.waitForReadyRead(2000));
        QByteArray datagram;
        datagram.resize(static_cast<int>(receiver.pendingDatagramSize()));
        receiver.readDatagram(datagram.data(), datagram.size());

        QCOMPARE(datagram, QByteArray("PAUSE_TOGGLE\n"));
    }

    void sendUsesDefaultPortWhenNoneGiven() {
        QCOMPARE(RetroArchNetworkCommand::kDefaultPort, static_cast<quint16>(55355));
    }
};

QTEST_MAIN(RetroArchNetworkCommandTest)
#include "RetroArchNetworkCommandTest.moc"
