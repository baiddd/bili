#include "EmulatorProvider.h"
#include "miniz.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QCoreApplication>

EmulatorProvider::EmulatorProvider(QString dataDir, NetworkManager *networkManager, QObject *parent)
    : QObject(parent), m_dataDir(std::move(dataDir)), m_networkManager(networkManager) {
    connect(m_networkManager, &NetworkManager::progress, this,
            [this](int requestId, qint64 received, qint64 total) {
        const QString target = m_activeDownloadTargets.value(requestId);
        if (target.isEmpty()) return;
        emit installProgress(target, received, total);
    });

    connect(m_networkManager, &NetworkManager::finished, this,
            [this](int requestId, const QString &destPath) {
        if (!m_activeDownloadTargets.contains(requestId)) return;
        const QString target = m_activeDownloadTargets.take(requestId);

        if (target.startsWith("core:")) {
            const QString system = target.mid(QString("core:").size());
            const QString coreName = m_pendingCoreFilenames.take(target);
            QDir().mkpath(coresDir());
            if (!extractZipEntry(destPath, coreName + "_libretro.dll", coresDir())) {
                QFile::remove(destPath);
                emit installFailed(target, "Échec de l'extraction du core.");
                return;
            }
            QFile::remove(destPath);
            InstalledState state = readInstalledState();
            state.coresBySystem.insert(system, coreName);
            persistInstalledState(state);
            emit installFinished(target);
        }
        // "retroarch" target handled by Task 4's addition to this same lambda.
    });

    connect(m_networkManager, &NetworkManager::failed, this,
            [this](int requestId, const QString &errorString) {
        const QString target = m_activeDownloadTargets.take(requestId);
        if (target.isEmpty()) return;
        m_pendingCoreFilenames.remove(target);
        emit installFailed(target, errorString);
    });
}

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

void EmulatorProvider::installCore(const QString &system) {
    const CoreCatalogEntry entry = m_catalogData.coresBySystem.value(system);
    if (entry.core.isEmpty()) {
        emit installFailed("core:" + system, "Catalogue non chargé — réessaie.");
        return;
    }
    installCoreFrom(system, entry);
}

void EmulatorProvider::installCoreFrom(const QString &system, const CoreCatalogEntry &entry) {
    const QString target = "core:" + system;
    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    const QString tempPath = temp.fileName();
    temp.close();

    const int requestId = m_networkManager->startDownload(entry.url, tempPath);
    m_activeDownloadTargets.insert(requestId, target);
    m_pendingCoreFilenames.insert(target, entry.core);
}

bool EmulatorProvider::extractZipEntry(const QString &zipPath, const QString &entryFileName, const QString &destDir) {
    mz_zip_archive zipArchive;
    mz_zip_zero_struct(&zipArchive);
    if (!mz_zip_reader_init_file(&zipArchive, zipPath.toUtf8().constData(), 0)) {
        return false;
    }

    bool extracted = false;
    const mz_uint numFiles = mz_zip_reader_get_num_files(&zipArchive);
    for (mz_uint i = 0; i < numFiles; ++i) {
        if (mz_zip_reader_is_file_a_directory(&zipArchive, i)) continue;
        char nameBuf[1024];
        mz_zip_reader_get_filename(&zipArchive, i, nameBuf, sizeof(nameBuf));
        if (QString::fromUtf8(nameBuf) != entryFileName) continue;

        const QString destPath = destDir + "/" + entryFileName;
        extracted = mz_zip_reader_extract_to_file(&zipArchive, i, destPath.toUtf8().constData(), 0);
        break;
    }

    mz_zip_reader_end(&zipArchive);
    return extracted;
}

void EmulatorProvider::persistInstalledState(const InstalledState &state) {
    QJsonObject cores;
    for (auto it = state.coresBySystem.begin(); it != state.coresBySystem.end(); ++it) {
        cores[it.key()] = it.value();
    }
    QJsonObject root;
    root["retroarch"] = state.retroArch;
    root["cores"] = cores;

    QDir().mkpath(QFileInfo(installedStatePath()).absolutePath());
    QFile file(installedStatePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
    }
}
