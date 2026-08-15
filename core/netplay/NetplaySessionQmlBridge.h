#pragma once
#include <QObject>
#include "INetplaySession.h"

class NetplaySessionQmlBridge : public QObject {
    Q_OBJECT
public:
    explicit NetplaySessionQmlBridge(INetplaySession *session, QObject *parent = nullptr)
        : QObject(parent), m_session(session) {}

    Q_INVOKABLE QString host() { return m_session->host(); }
    Q_INVOKABLE QString join(const QString &address) { return m_session->join(address); }

private:
    INetplaySession *m_session;
};
