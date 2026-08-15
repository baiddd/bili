#pragma once
#include "IEmulatorProvider.h"

class StubEmulatorProvider : public IEmulatorProvider {
public:
    bool isImplemented() const override { return false; }
    QString installRetroArch() override;
    QString installCore(const QString &system) override;
    QString installStandaloneEmulator(const QString &name) override;
};
