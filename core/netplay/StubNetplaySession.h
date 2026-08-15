#pragma once
#include "INetplaySession.h"

class StubNetplaySession : public INetplaySession {
public:
    bool isImplemented() const override { return false; }
    QString host() override;
    QString join(const QString &address) override;
};
