#pragma once
#include <QObject>
#include "IEmulatorProvider.h"

class EmulatorProviderQmlBridge : public QObject {
    Q_OBJECT
public:
    explicit EmulatorProviderQmlBridge(IEmulatorProvider *provider, QObject *parent = nullptr)
        : QObject(parent), m_provider(provider) {}

    Q_INVOKABLE QString installRetroArch() { return m_provider->installRetroArch(); }
    Q_INVOKABLE QString installCore(const QString &system) { return m_provider->installCore(system); }
    Q_INVOKABLE QString installStandaloneEmulator(const QString &name) { return m_provider->installStandaloneEmulator(name); }

private:
    IEmulatorProvider *m_provider;
};
