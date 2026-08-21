#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <cstring>
#include "emulators/EmulatorProvider.h"
#include "emulators/EmulatorCatalog.h"
#include "network/NetworkManager.h"
#include "miniz.h"

class EmulatorProviderTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotInstalledWhenNothingOnDisk() {
        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);
        QVERIFY(!provider.isRetroArchInstalled());
        QVERIFY(!provider.isCoreInstalled("nes"));
    }

    void reportsInstalledWhenStateFileAndRealFileBothExist() {
        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

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

        EmulatorProvider reloaded(dir.path(), &networkManager);
        QVERIFY(reloaded.isRetroArchInstalled());
        QVERIFY(reloaded.isCoreInstalled("nes"));
        QVERIFY(!reloaded.isCoreInstalled("snes"));
    }

    void selfHealsWhenStateFileClaimsInstallButFileIsMissing() {
        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QJsonObject state;
        state["retroarch"] = true; // claimed installed, but no real file on disk
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        EmulatorProvider reloaded(dir.path(), &networkManager);
        QVERIFY(!reloaded.isRetroArchInstalled());
    }

    void installCoreDownloadsExtractsAndRecordsState() {
        const QString zipPath = m_tempZipDir.path() + "/core.zip";
        mz_zip_archive zipArchive;
        memset(&zipArchive, 0, sizeof(zipArchive));
        QVERIFY(mz_zip_writer_init_file(&zipArchive, zipPath.toLocal8Bit().constData(), 0));
        static const char kCoreBytes[] = "fake-core-bytes";
        QVERIFY(mz_zip_writer_add_mem(&zipArchive, "fceumm_libretro.dll", kCoreBytes, sizeof(kCoreBytes), MZ_DEFAULT_COMPRESSION));
        QVERIFY(mz_zip_writer_finalize_archive(&zipArchive));
        QVERIFY(mz_zip_writer_end(&zipArchive));

        QFile zipFile(zipPath);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        const QByteArray zipBytes = zipFile.readAll();
        zipFile.close();

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server, &zipBytes]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: "
                + QByteArray::number(zipBytes.size()) + "\r\n\r\n" + zipBytes;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);
        QSignalSpy finishedSpy(&provider, &EmulatorProvider::installFinished);
        QSignalSpy failedSpy(&provider, &EmulatorProvider::installFailed);
        QSignalSpy progressSpy(&provider, &EmulatorProvider::installProgress);

        provider.installCoreFrom("nes", CoreCatalogEntry{
            "fceumm", QUrl(QString("http://127.0.0.1:%1/core.zip").arg(server.serverPort()))});

        QVERIFY(finishedSpy.wait(5000));
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(finishedSpy.first().at(0).toString(), QString("core:nes"));
        QVERIFY(!progressSpy.isEmpty());
        QVERIFY(provider.isCoreInstalled("nes"));
        QVERIFY(QFile::exists(provider.coresDir() + "/fceumm_libretro.dll"));
    }

    void installCoreFailsCleanlyForAnUnreachableUrl() {
        quint16 freePort = 0;
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            freePort = probe.serverPort();
        }

        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);
        QSignalSpy failedSpy(&provider, &EmulatorProvider::installFailed);

        provider.installCoreFrom("nes", CoreCatalogEntry{
            "fceumm", QUrl(QString("http://127.0.0.1:%1/nope").arg(freePort))});

        QVERIFY(failedSpy.wait(5000));
        QVERIFY(!provider.isCoreInstalled("nes"));
    }

private:
    QTemporaryDir m_tempZipDir;
};

QTEST_MAIN(EmulatorProviderTest)
#include "EmulatorProviderTest.moc"
