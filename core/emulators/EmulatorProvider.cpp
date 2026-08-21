#include "EmulatorProvider.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

EmulatorProvider::EmulatorProvider(QString dataDir, QObject *parent)
    : QObject(parent), m_dataDir(std::move(dataDir)) {}

QString EmulatorProvider::retroArchDir() const {
    return m_dataDir + "/emulators/retroarch";
}

QString EmulatorProvider::retroArchExecutablePath() const {
    return retroArchDir() + "/retroarch.exe";
}

QString EmulatorProvider::coresDir() const {
    return retroArchDir() + "/cores";
}

QString EmulatorProvider::installedStatePath() const {
    return m_dataDir + "/emulators/installed.json";
}

EmulatorProvider::InstalledState EmulatorProvider::readInstalledState() const {
    InstalledState state;
    QFile file(installedStatePath());
    if (!file.open(QIODevice::ReadOnly)) return state;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return state;

    const QJsonObject obj = doc.object();
    state.retroArch = obj.value("retroarch").toBool(false);
    const QJsonObject cores = obj.value("cores").toObject();
    for (auto it = cores.begin(); it != cores.end(); ++it) {
        state.coresBySystem.insert(it.key(), it.value().toString());
    }
    return state;
}

bool EmulatorProvider::isRetroArchInstalled() const {
    const InstalledState state = readInstalledState();
    return state.retroArch && QFile::exists(retroArchExecutablePath());
}

bool EmulatorProvider::isCoreInstalled(const QString &system) const {
    const InstalledState state = readInstalledState();
    const QString core = state.coresBySystem.value(system);
    if (core.isEmpty()) return false;
    return QFile::exists(coresDir() + "/" + core + "_libretro.dll");
}
