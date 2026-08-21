#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "emulators/EmulatorProvider.h"

class EmulatorProviderTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotInstalledWhenNothingOnDisk() {
        QTemporaryDir dir;
        EmulatorProvider provider(dir.path());
        QVERIFY(!provider.isRetroArchInstalled());
        QVERIFY(!provider.isCoreInstalled("nes"));
    }

    void reportsInstalledWhenStateFileAndRealFileBothExist() {
        QTemporaryDir dir;
        EmulatorProvider provider(dir.path());

        QDir().mkpath(provider.retroArchDir());
        QFile(provider.retroArchExecutablePath()).open(QIODevice::WriteOnly);
        QDir().mkpath(provider.coresDir());
        QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);

        QJsonObject cores;
        cores["nes"] = "fceumm";
        QJsonObject state;
        state["retroarch"] = true;
        state["cores"] = cores;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        EmulatorProvider reloaded(dir.path());
        QVERIFY(reloaded.isRetroArchInstalled());
        QVERIFY(reloaded.isCoreInstalled("nes"));
        QVERIFY(!reloaded.isCoreInstalled("snes"));
    }

    void selfHealsWhenStateFileClaimsInstallButFileIsMissing() {
        QTemporaryDir dir;
        EmulatorProvider provider(dir.path());

        QJsonObject state;
        state["retroarch"] = true; // claimed installed, but no real file on disk
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        EmulatorProvider reloaded(dir.path());
        QVERIFY(!reloaded.isRetroArchInstalled());
    }
};

QTEST_MAIN(EmulatorProviderTest)
#include "EmulatorProviderTest.moc"
