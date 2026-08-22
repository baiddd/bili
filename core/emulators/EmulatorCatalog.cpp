#include "EmulatorCatalog.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

EmulatorCatalog::EmulatorCatalog(NetworkManager *networkManager, QObject *parent)
    : QObject(parent), m_networkManager(networkManager) {
    connect(m_networkManager, &NetworkManager::finished, this,
            [this](int requestId, const QString &destPath) {
        if (requestId != m_pendingRequestId) return;
        m_pendingRequestId = -1;

        QFile file(destPath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit failed("Impossible de lire le manifeste téléchargé.");
            return;
        }
        const QByteArray raw = file.readAll();
        file.close();
        QFile::remove(destPath);

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            emit failed("Manifeste invalide : " + parseError.errorString());
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonObject retroArch = root.value("retroarch").toObject();
        EmulatorCatalogData data;
        data.retroArchVersion = retroArch.value("version").toString();
        data.retroArchUrl = QUrl(retroArch.value("windows_x64_url").toString());

        const QJsonObject cores = root.value("cores").toObject();
        for (auto it = cores.begin(); it != cores.end(); ++it) {
            const QJsonObject entry = it.value().toObject();
            CoreCatalogEntry coreEntry;
            coreEntry.core = entry.value("core").toString();
            coreEntry.url = QUrl(entry.value("url").toString());
            data.coresBySystem.insert(it.key(), coreEntry);
        }

        emit ready(data);
    });

    connect(m_networkManager, &NetworkManager::failed, this,
            [this](int requestId, const QString &errorString) {
        if (requestId != m_pendingRequestId) return;
        m_pendingRequestId = -1;
        QFile::remove(m_tempPath);
        emit failed(errorString);
    });
}

QUrl EmulatorCatalog::manifestUrl() {
    // Canonical single source for this literal (fix wave, sub-project 3 final
    // review) - main.cpp's boot-time fetch() and
    // EmulatorManagerScreen.qml's on-screen-open fetch() both call this
    // instead of each hardcoding their own copy of the URL, which would
    // otherwise be free to silently drift apart.
    return QUrl("https://raw.githubusercontent.com/baiddd/bili/master/catalog/emulators.json");
}

void EmulatorCatalog::fetch(const QUrl &manifestUrl) {
    // Re-entrancy guard (re-review of the final fix wave, sub-project 3):
    // fetch() is now Q_INVOKABLE and called from EmulatorManagerScreen.qml
    // on every screen open (Fix 3), so a second call can now land while the
    // first is still in flight -- unreachable before that change, since the
    // only caller was main.cpp's one-shot boot fetch. Without this guard, a
    // second call overwrites m_tempPath and m_pendingRequestId before the
    // first request's own finished/failed handler runs; those handlers
    // already ignore a superseded requestId correctly, but the failed
    // handler's QFile::remove(m_tempPath) only ever refers to the LATEST
    // m_tempPath, so the superseded request's own temp file is silently
    // never removed -- the exact same leak class as EmulatorProvider's
    // m_activeTargets guard (Fix 5c) exists to prevent. m_pendingRequestId
    // resets to -1 in both the finished and failed handlers, so this only
    // blocks a second call while the first is genuinely still pending -- it
    // does not affect the legitimate retry-on-screen-reopen story Fix 3
    // exists for, since that call always happens after the previous fetch
    // has already completed, never while one is in flight.
    if (m_pendingRequestId != -1) return;

    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    m_tempPath = temp.fileName();
    temp.close();

    m_pendingRequestId = m_networkManager->startDownload(manifestUrl, m_tempPath);
}
