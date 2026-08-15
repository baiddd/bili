#pragma once
#include <QObject>
#include <QUrl>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    int startDownload(const QUrl &url, const QString &destPath);
    void cancelDownload(int requestId);

signals:
    void progress(int requestId, qint64 bytesReceived, qint64 bytesTotal);
    void finished(int requestId, const QString &destPath);
    void failed(int requestId, const QString &errorString);

private:
    QNetworkAccessManager m_nam;
    QMap<int, QNetworkReply *> m_active;
    int m_nextId = 1;
};
