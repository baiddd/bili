#pragma once
#include <QObject>
#include <QString>
#include <QMap>

class EmulatorProvider : public QObject {
    Q_OBJECT
public:
    explicit EmulatorProvider(QString dataDir, QObject *parent = nullptr);

    Q_INVOKABLE bool isRetroArchInstalled() const;
    Q_INVOKABLE bool isCoreInstalled(const QString &system) const;

    // Exposed for testing: the exact paths this class checks/writes to,
    // without touching the filesystem.
    QString retroArchDir() const;         // "<dataDir>/emulators/retroarch"
    QString retroArchExecutablePath() const; // "<retroArchDir>/retroarch.exe"
    QString coresDir() const;             // "<retroArchDir>/cores"
    QString installedStatePath() const;   // "<dataDir>/emulators/installed.json"

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
};
