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
    // Runs after every test function (QtTest convention), regardless of
    // pass/fail, so installRetroArchExtractsAndRecordsState()'s fake-7za.exe
    // override never leaks into a later test.
    void cleanup() {
        EmulatorProvider::setSevenZipExecutablePathForTesting(QString());
    }

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

    // Fakes 7za.exe's behavior with a tiny batch-file stand-in that ignores
    // its input archive and just creates retroarch.exe directly in the
    // requested output directory, so this test stays fast and hermetic --
    // exercising EmulatorProvider's orchestration logic (QProcess
    // invocation, argument construction, state recording), not 7-Zip's own
    // extraction correctness (which isn't a real .7z here at all).
    void installRetroArchExtractsAndRecordsState() {
        const QString fake7za = m_tempZipDir.path() + "/fake7za.bat";
        QFile fake(fake7za);
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Text));
        // %1=x %2=<archive> %3=-o<destdir> %4=-y -- strip the "-o" prefix
        // from %3 to recover the destination directory 7za.exe would have
        // been told to extract into.
        fake.write(
            "@echo off\r\n"
            "set \"destdir=%~3\"\r\n"
            "set \"destdir=%destdir:~2%\"\r\n"
            "if not exist \"%destdir%\" mkdir \"%destdir%\"\r\n"
            "type nul > \"%destdir%\\retroarch.exe\"\r\n"
            "exit /b 0\r\n");
        fake.close();
        EmulatorProvider::setSevenZipExecutablePathForTesting(fake7za);

        static const char kArchiveBytes[] = "not-a-real-7z-archive";
        const QByteArray archiveBytes(kArchiveBytes, sizeof(kArchiveBytes));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server, &archiveBytes]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: "
                + QByteArray::number(archiveBytes.size()) + "\r\n\r\n" + archiveBytes;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QSignalSpy finishedSpy(&provider, &EmulatorProvider::installFinished);

        provider.installRetroArchFrom(QUrl(QString("http://127.0.0.1:%1/RetroArch.7z").arg(server.serverPort())));

        QVERIFY(finishedSpy.wait(5000));
        QCOMPARE(finishedSpy.first().at(0).toString(), QString("retroarch"));
        QVERIFY(provider.isRetroArchInstalled());
        QVERIFY(QFile::exists(provider.retroArchDir() + "/retroarch.cfg"));
    }

    // Regression test: a real (or corrupted-but-zero-exit-code) extraction
    // that doesn't end up with exactly one top-level subdirectory should
    // NOT be reported as a successful install. Fakes 7za.exe with a
    // stand-in that creates TWO top-level subdirectories (neither at
    // destDir's root, matching the "flatten a single nested subdirectory"
    // guard's `topLevelDirs.size() == 1` condition being false), so
    // retroarch.exe never ends up at the expected path. Before the fix,
    // extract7zArchive() unconditionally returned true here, so the caller
    // would emit installFinished, persist installed.json, and delete the
    // downloaded archive despite RetroArch not actually being installed.
    void installRetroArchFailsWhenExtractionDoesNotProduceExactlyOneTopLevelDir() {
        const QString fake7za = m_tempZipDir.path() + "/fake7za_multidir.bat";
        QFile fake(fake7za);
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Text));
        // %1=x %2=<archive> %3=-o<destdir> %4=-y -- strip the "-o" prefix
        // from %3 to recover the destination directory, then create two
        // top-level subdirectories instead of retroarch.exe directly.
        fake.write(
            "@echo off\r\n"
            "set \"destdir=%~3\"\r\n"
            "set \"destdir=%destdir:~2%\"\r\n"
            "if not exist \"%destdir%\\dirA\" mkdir \"%destdir%\\dirA\"\r\n"
            "if not exist \"%destdir%\\dirB\" mkdir \"%destdir%\\dirB\"\r\n"
            "type nul > \"%destdir%\\dirA\\retroarch.exe\"\r\n"
            "exit /b 0\r\n");
        fake.close();
        EmulatorProvider::setSevenZipExecutablePathForTesting(fake7za);

        static const char kArchiveBytes[] = "not-a-real-7z-archive";
        const QByteArray archiveBytes(kArchiveBytes, sizeof(kArchiveBytes));

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server, &archiveBytes]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: "
                + QByteArray::number(archiveBytes.size()) + "\r\n\r\n" + archiveBytes;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QSignalSpy finishedSpy(&provider, &EmulatorProvider::installFinished);
        QSignalSpy failedSpy(&provider, &EmulatorProvider::installFailed);

        provider.installRetroArchFrom(QUrl(QString("http://127.0.0.1:%1/RetroArch.7z").arg(server.serverPort())));

        QVERIFY(failedSpy.wait(5000));
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(failedSpy.first().at(0).toString(), QString("retroarch"));
        QVERIFY(!provider.isRetroArchInstalled());
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
