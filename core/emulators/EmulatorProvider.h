#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include "EmulatorCatalog.h"
#include "network/NetworkManager.h"

class EmulatorProvider : public QObject {
    Q_OBJECT
public:
    explicit EmulatorProvider(QString dataDir, NetworkManager *networkManager, QObject *parent = nullptr);

    Q_INVOKABLE bool isRetroArchInstalled() const;
    Q_INVOKABLE bool isCoreInstalled(const QString &system) const;

    Q_INVOKABLE void installCore(const QString &system);
    // Testing-only entry point that skips the catalog lookup (EmulatorCatalogTest
    // already covers catalog parsing; this lets EmulatorProviderTest exercise
    // download+extract+state-recording in isolation).
    void installCoreFrom(const QString &system, const CoreCatalogEntry &entry);

    // Exposed for testing: the exact paths this class checks/writes to,
    // without touching the filesystem.
    QString retroArchDir() const;         // "<dataDir>/emulators/retroarch"
    QString retroArchExecutablePath() const; // "<retroArchDir>/retroarch.exe"
    QString coresDir() const;             // "<retroArchDir>/cores"
    QString installedStatePath() const;   // "<dataDir>/emulators/installed.json"

signals:
    void installProgress(const QString &target, qint64 bytesReceived, qint64 bytesTotal);
    void installFinished(const QString &target);
    void installFailed(const QString &target, const QString &errorString);

protected:
    // Reads installed.json into memory; returns an empty/default state if
    // the file doesn't exist or fails to parse (never treated as an
    // error - "no state file yet" is the normal state on first run).
    struct InstalledState {
        bool retroArch = false;
        QMap<QString, QString> coresBySystem; // system -> core name
    };
    InstalledState readInstalledState() const;

    QString m_dataDir;

private:
    // Read-modify-write helper used by every install/uninstall path (Tasks
    // 3-5): callers read the current state, mutate the in-memory copy, and
    // call this once to persist it — a single place that builds/writes the
    // JSON, so no call site duplicates that logic.
    void persistInstalledState(const InstalledState &state);
    bool extractZipEntry(const QString &zipPath, const QString &entryFileName, const QString &destDir);

    NetworkManager *m_networkManager;
    EmulatorCatalogData m_catalogData; // populated via setCatalogData() once main.cpp's EmulatorCatalog::ready fires (Task 8)
    QMap<int, QString> m_activeDownloadTargets; // requestId -> "core:<system>" / "retroarch"
    QMap<QString, QString> m_pendingCoreFilenames; // target -> expected "<core>_libretro.dll" once known
};
