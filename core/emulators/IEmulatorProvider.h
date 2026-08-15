#pragma once
#include <QString>

class IEmulatorProvider {
public:
    virtual ~IEmulatorProvider() = default;
    virtual bool isImplemented() const = 0;
    virtual QString installRetroArch() = 0;
    virtual QString installCore(const QString &system) = 0;
    virtual QString installStandaloneEmulator(const QString &name) = 0;
};
