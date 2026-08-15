#include "NetworkManager.h"

NetworkManager::NetworkManager(QObject *parent) : QObject(parent) {}

int NetworkManager::startDownload(const QUrl &url, const QString &destPath) {
    const int id = m_nextId++;
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    m_active[id] = reply;

    auto *file = new QFile(destPath, reply);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, id](qint64 received, qint64 total) {
                emit progress(id, received, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, id, reply, destPath, file]() {
        m_active.remove(id);
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(id, reply->errorString());
        } else {
            if (file->open(QIODevice::WriteOnly)) {
                file->write(reply->readAll());
                file->close();
                emit finished(id, destPath);
            } else {
                emit failed(id, "cannot open destination file");
            }
        }
        reply->deleteLater();
    });

    return id;
}

void NetworkManager::cancelDownload(int requestId) {
    if (auto *reply = m_active.value(requestId)) {
        reply->abort();
    }
}
