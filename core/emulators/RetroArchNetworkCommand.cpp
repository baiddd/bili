// core/emulators/RetroArchNetworkCommand.cpp
#include "RetroArchNetworkCommand.h"
#include <QUdpSocket>
#include <QHostAddress>

bool RetroArchNetworkCommand::send(const QString &command, quint16 port) {
    QUdpSocket socket;
    const QByteArray payload = (command + "\n").toUtf8();
    const qint64 written = socket.writeDatagram(payload, QHostAddress::LocalHost, port);
    return written == payload.size();
}
