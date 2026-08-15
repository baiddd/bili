#pragma once
#include <QString>

class INetplaySession {
public:
    virtual ~INetplaySession() = default;
    virtual bool isImplemented() const = 0;
    virtual QString host() = 0;
    virtual QString join(const QString &address) = 0;
};
