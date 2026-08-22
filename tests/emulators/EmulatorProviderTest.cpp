#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QWindow>
#include <QCoreApplication>
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
    // whose new top-level entries don't include ANY subdirectory containing
    // retroarch.exe should NOT be reported as a successful install. Fakes
    // 7za.exe with a stand-in that creates TWO top-level subdirectories,
    // neither containing retroarch.exe, so extract7zArchive()'s search among
    // new entries finds no match. Before the original fix, extract7zArchive()
    // unconditionally returned true here, so the caller would emit
    // installFinished, persist installed.json, and delete the downloaded
    // archive despite RetroArch not actually being installed.
    //
    // Note: post-fix-wave, a scenario with multiple new top-level entries
    // where exactly ONE of them contains retroarch.exe is now a legitimate
    // success (see installRetroArchSurvivesAPreExistingCoresDirectory()
    // below and extract7zArchive()'s own comments) -- this test is scoped
    // specifically to the case where NONE of the new entries match, which
    // remains a genuine failure.
    void installRetroArchFailsWhenNoNewEntryContainsRetroarch() {
        const QString fake7za = m_tempZipDir.path() + "/fake7za_multidir.bat";
        QFile fake(fake7za);
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Text));
        // %1=x %2=<archive> %3=-o<destdir> %4=-y -- strip the "-o" prefix
        // from %3 to recover the destination directory, then create two
        // top-level subdirectories, neither containing retroarch.exe.
        fake.write(
            "@echo off\r\n"
            "set \"destdir=%~3\"\r\n"
            "set \"destdir=%destdir:~2%\"\r\n"
            "if not exist \"%destdir%\\dirA\" mkdir \"%destdir%\\dirA\"\r\n"
            "if not exist \"%destdir%\\dirB\" mkdir \"%destdir%\\dirB\"\r\n"
            "type nul > \"%destdir%\\dirA\\somefile.txt\"\r\n"
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

    // Regression test (Critical bug, final review of sub-project 3): a core
    // installed BEFORE RetroArch creates destDir/cores/ (via
    // QDir().mkpath(coresDir()) in the NetworkManager::finished handler).
    // The old extract7zArchive() only flattened a nested "RetroArch-Win64/"
    // folder when destDir held EXACTLY ONE top-level entry -- with cores/
    // already present, that guard silently did nothing, retroarch.exe was
    // never found at the expected path, and the old failure-cleanup path's
    // QDir(destDir).removeRecursively() then wiped out cores/ (and the core
    // inside it) along with the empty extraction attempt, reporting "Échec
    // de l'extraction de RetroArch." This was live-reproduced by the final
    // reviewer with a real network download; this test proves the fix
    // without needing one, via the same fake-7za.exe-stand-in seam the other
    // extraction tests use. Verified to fail against the pre-fix code
    // (finishedSpy never fires -- installFailed fires instead, and the core
    // file gets deleted by the old removeRecursively() cleanup) and pass
    // against the fixed code.
    void installRetroArchSurvivesAPreExistingCoresDirectory() {
        const QString fake7za = m_tempZipDir.path() + "/fake7za_nested.bat";
        QFile fake(fake7za);
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Text));
        // Mimics the real RetroArch.7z's actual shape (everything nested
        // under one subdirectory, "RetroArch-Win64") rather than the other
        // tests' flat stand-in -- a flat stand-in never has a "second
        // top-level entry" problem in the first place, so it wouldn't
        // exercise this bug at all.
        fake.write(
            "@echo off\r\n"
            "set \"destdir=%~3\"\r\n"
            "set \"destdir=%destdir:~2%\"\r\n"
            "if not exist \"%destdir%\\RetroArch-Win64\" mkdir \"%destdir%\\RetroArch-Win64\"\r\n"
            "type nul > \"%destdir%\\RetroArch-Win64\\retroarch.exe\"\r\n"
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

        // Pre-install a core, exactly as if it had been installed before
        // RetroArch (the real-world sequence that triggered the bug).
        QDir().mkpath(provider.coresDir());
        QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
        QJsonObject cores; cores["nes"] = "fceumm";
        QJsonObject state; state["retroarch"] = false; state["cores"] = cores;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();
        QVERIFY(provider.isCoreInstalled("nes"));

        QSignalSpy finishedSpy(&provider, &EmulatorProvider::installFinished);
        QSignalSpy failedSpy(&provider, &EmulatorProvider::installFailed);

        provider.installRetroArchFrom(QUrl(QString("http://127.0.0.1:%1/RetroArch.7z").arg(server.serverPort())));

        QVERIFY(finishedSpy.wait(5000));
        QCOMPARE(failedSpy.count(), 0);
        QVERIFY(provider.isRetroArchInstalled());

        // The pre-existing core must survive RetroArch's install untouched.
        QVERIFY(QFile::exists(provider.coresDir() + "/fceumm_libretro.dll"));
        QVERIFY(provider.isCoreInstalled("nes"));
    }

    // Regression test (re-review of the final fix wave, sub-project 3): the
    // merge step that flattens matchedSubdir's contents into destDir
    // overwrites-by-name any top-level entry with no distinction between
    // "the archive's own content" and content Bili itself owns --
    // specifically coresDir()'s basename ("cores"). Not currently triggered
    // by the real RetroArch.7z (RetroArch.7z and RetroArch_cores.7z are
    // confirmed-separate archives on the real buildbot), but this proves the
    // defensive guard added for a hypothetical future archive that ships a
    // top-level cores/ entry of its own. Fakes 7za.exe to extract a nested
    // "RetroArch-Win64/" subfolder containing BOTH retroarch.exe AND a
    // "cores" sub-subfolder -- simulating exactly that hypothetical future
    // archive shape. Asserts the install fails (installFailed fires, not
    // installFinished) and the real, pre-existing core file in coresDir() is
    // still present with unchanged content afterward. Verified to fail
    // against the pre-fix merge logic (the fake archive's own "cores"
    // subfolder gets merged into destDir, overwriting/hiding the
    // pre-existing core file) and pass against the fixed code.
    void installRetroArchRefusesToOverwriteExistingCoresDirectory() {
        const QString fake7za = m_tempZipDir.path() + "/fake7za_cores_collision.bat";
        QFile fake(fake7za);
        QVERIFY(fake.open(QIODevice::WriteOnly | QIODevice::Text));
        // %1=x %2=<archive> %3=-o<destdir> %4=-y -- strip the "-o" prefix
        // from %3, then create a nested RetroArch-Win64/ subfolder holding
        // both retroarch.exe and its own "cores" sub-subfolder (simulating a
        // hypothetical future RetroArch.7z that bundles cores directly).
        fake.write(
            "@echo off\r\n"
            "set \"destdir=%~3\"\r\n"
            "set \"destdir=%destdir:~2%\"\r\n"
            "if not exist \"%destdir%\\RetroArch-Win64\" mkdir \"%destdir%\\RetroArch-Win64\"\r\n"
            "if not exist \"%destdir%\\RetroArch-Win64\\cores\" mkdir \"%destdir%\\RetroArch-Win64\\cores\"\r\n"
            "type nul > \"%destdir%\\RetroArch-Win64\\retroarch.exe\"\r\n"
            "type nul > \"%destdir%\\RetroArch-Win64\\cores\\some_future_core.dll\"\r\n"
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

        // Pre-install a real core, exactly as if it had been installed
        // before RetroArch -- the interesting content that must survive.
        static const char kCoreBytes[] = "dummy-core-bytes";
        QDir().mkpath(provider.coresDir());
        QFile coreFile(provider.coresDir() + "/fceumm_libretro.dll");
        QVERIFY(coreFile.open(QIODevice::WriteOnly));
        coreFile.write(kCoreBytes, sizeof(kCoreBytes));
        coreFile.close();

        QJsonObject cores; cores["nes"] = "fceumm";
        QJsonObject state; state["retroarch"] = false; state["cores"] = cores;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();
        QVERIFY(provider.isCoreInstalled("nes"));

        QSignalSpy finishedSpy(&provider, &EmulatorProvider::installFinished);
        QSignalSpy failedSpy(&provider, &EmulatorProvider::installFailed);

        provider.installRetroArchFrom(QUrl(QString("http://127.0.0.1:%1/RetroArch.7z").arg(server.serverPort())));

        QVERIFY(failedSpy.wait(5000));
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(failedSpy.first().at(0).toString(), QString("retroarch"));
        QVERIFY(!provider.isRetroArchInstalled());

        // The pre-existing core must survive completely untouched, with its
        // content unchanged, not just merely present.
        QFile checkFile(provider.coresDir() + "/fceumm_libretro.dll");
        QVERIFY(checkFile.exists());
        QVERIFY(checkFile.open(QIODevice::ReadOnly));
        const QByteArray survivingContent = checkFile.readAll();
        checkFile.close();
        QCOMPARE(survivingContent, QByteArray(kCoreBytes, sizeof(kCoreBytes)));
        QVERIFY(provider.isCoreInstalled("nes"));
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

    void uninstallCoreRemovesFileAndClearsState() {
        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QDir().mkpath(provider.coresDir());
        QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
        // Seed installed.json directly (same JSON shape as Task 1's test) to
        // simulate a prior successful install without re-running the whole
        // download flow.
        QJsonObject cores; cores["nes"] = "fceumm";
        QJsonObject state; state["retroarch"] = false; state["cores"] = cores;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        stateFile.open(QIODevice::WriteOnly);
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        QVERIFY(provider.isCoreInstalled("nes"));

        QSignalSpy finishedSpy(&provider, &EmulatorProvider::uninstallFinished);
        provider.uninstallCore("nes");

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toString(), QString("core:nes"));
        QVERIFY(!provider.isCoreInstalled("nes"));
        QVERIFY(!QFile::exists(provider.coresDir() + "/fceumm_libretro.dll"));
    }

    void uninstallRetroArchRemovesDirectoryAndClearsState() {
        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QDir().mkpath(provider.retroArchDir());
        QFile(provider.retroArchExecutablePath()).open(QIODevice::WriteOnly);
        QJsonObject state; state["retroarch"] = true;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        stateFile.open(QIODevice::WriteOnly);
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        QVERIFY(provider.isRetroArchInstalled());

        QSignalSpy finishedSpy(&provider, &EmulatorProvider::uninstallFinished);
        provider.uninstallRetroArch();

        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY(!provider.isRetroArchInstalled());
        QVERIFY(!QDir(provider.retroArchDir()).exists());
    }

    void launchArgsPointsAtTheGivenCoreAndRom() {
        const QStringList args = EmulatorProvider::launchArgs("C:/cores/fceumm_libretro.dll", "C:/roms/Zelda.nes");
        QCOMPARE(args, QStringList({"-L", "C:/cores/fceumm_libretro.dll", "C:/roms/Zelda.nes"}));
    }

    void launchGameFailsCleanlyWhenNoCoreIsInstalled() {
        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QSignalSpy failedSpy(&provider, &EmulatorProvider::launchFailed);
        provider.launchGame("C:/roms/Zelda.nes", "nes");

        QCOMPARE(failedSpy.count(), 1);
        QVERIFY(!failedSpy.first().at(0).toString().isEmpty());
    }

    // Regression test (Task 6 review, bug 1): before the fix, the temp dir
    // created to hold a ROM extracted from a "<archive>::<entry>" rom_path
    // was only ever cleaned up at the top of the *next* launchGame() call
    // (or in the destructor) -- never when the game that used it actually
    // exited, leaking it on disk for the rest of the app session.
    //
    // Exercises a real end-to-end launch/exit cycle: "retroarch.exe" is a
    // copy of TestGuiWindowStandIn.exe (Task 1's real Win32 GUI stand-in
    // process), which creates one visible window (so EmulatorProvider's
    // GameWindowEmbedder can find and embed it) then self-closes after
    // ~2s -- a genuine QProcess::NormalExit, not a crash, so this stays
    // hermetic without needing RetroArch itself.
    void launchGameCleansUpTempDirWhenTheGameExits() {
        const QString standIn = QCoreApplication::applicationDirPath() + "/TestGuiWindowStandIn.exe";
        QVERIFY(QFile::exists(standIn));

        const QString zipPath = m_tempZipDir.path() + "/roms.zip";
        mz_zip_archive zipArchive;
        memset(&zipArchive, 0, sizeof(zipArchive));
        QVERIFY(mz_zip_writer_init_file(&zipArchive, zipPath.toLocal8Bit().constData(), 0));
        static const char kRomBytes[] = "fake-rom-bytes";
        QVERIFY(mz_zip_writer_add_mem(&zipArchive, "game.nes", kRomBytes, sizeof(kRomBytes), MZ_DEFAULT_COMPRESSION));
        QVERIFY(mz_zip_writer_finalize_archive(&zipArchive));
        QVERIFY(mz_zip_writer_end(&zipArchive));

        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QDir().mkpath(provider.coresDir());
        QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
        QDir().mkpath(provider.retroArchDir());
        QVERIFY(QFile::copy(standIn, provider.retroArchExecutablePath()));

        QJsonObject cores; cores["nes"] = "fceumm";
        QJsonObject state; state["retroarch"] = true; state["cores"] = cores;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        QWindow host;
        host.setGeometry(0, 0, 800, 600);
        host.create();
        provider.setHostWindowId(host.winId());

        QSignalSpy exitedSpy(&provider, &EmulatorProvider::gameExited);
        QSignalSpy failedSpy(&provider, &EmulatorProvider::launchFailed);

        provider.launchGame(zipPath + "::game.nes", "nes");

        const QString tempDirPath = provider.gameTempDirPathForTesting();
        QVERIFY(!tempDirPath.isEmpty());
        QVERIFY(QDir(tempDirPath).exists());

        QVERIFY(exitedSpy.wait(5000));
        QCOMPARE(failedSpy.count(), 0);
        QVERIFY(provider.gameTempDirPathForTesting().isEmpty());
        QVERIFY(!QDir(tempDirPath).exists());
    }

    void launchGameFailsWhenWindowEmbeddingFails() {
        // whoami.exe démarre et se termine quasi immédiatement sans jamais
        // créer de fenêtre -- exactement le scénario "embed() échoue" (même
        // stand-in que GameWindowEmbedderTest::embedFailsWhenNoWindowEverAppears,
        // Task 1).
        const QString systemRoot = qEnvironmentVariable("SystemRoot", "C:/Windows");
        const QString whoami = systemRoot + "/System32/whoami.exe";
        QVERIFY(QFile::exists(whoami));

        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);
        provider.setEmbedPollTimeoutForTesting(500); // court, pour ne pas ralentir la suite

        QDir().mkpath(provider.coresDir());
        QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
        QDir().mkpath(provider.retroArchDir());
        QVERIFY(QFile::copy(whoami, provider.retroArchExecutablePath()));

        QJsonObject cores; cores["nes"] = "fceumm";
        QJsonObject state; state["retroarch"] = true; state["cores"] = cores;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        QWindow host;
        host.setGeometry(0, 0, 800, 600);
        host.create();
        provider.setHostWindowId(host.winId());

        QSignalSpy launchFailedSpy(&provider, &EmulatorProvider::launchFailed);
        QSignalSpy gameLaunchedSpy(&provider, &EmulatorProvider::gameLaunched);

        provider.launchGame("dummy.nes", "nes");

        // As with launchGameEmitsLaunchFailedNotGameExitedWhenRetroArchFailsToStart()
        // below: on Windows, QProcess::started() fires synchronously once
        // CreateProcess() succeeds, and GameWindowEmbedder::embed()'s poll
        // loop is a plain blocking Sleep()-based loop (never yields to the
        // Qt event loop) -- so by the time launchGame() returns, whoami.exe
        // has already started, embed() has already exhausted its 500ms
        // testing budget without ever finding a window, and launchFailed()
        // has already been emitted. A QSignalSpy::wait() here would time out:
        // it only reports emissions that happen *after* wait() starts
        // watching, and this one already happened.
        if (launchFailedSpy.isEmpty()) {
            QVERIFY(launchFailedSpy.wait(3000));
        }
        QCOMPARE(launchFailedSpy.count(), 1);
        QCOMPARE(gameLaunchedSpy.count(), 0);
    }

    // Regression test (Task 6 review, bug 2): the errorOccurred handler used
    // to emit launchFailed for ANY QProcess::ProcessError, not just a
    // genuine failure to start. This proves the still-needed case -- a
    // process that truly never starts (an empty file is not a valid Win32
    // executable, so QProcess::start() fails with FailedToStart) -- still
    // correctly reports launchFailed, and neither gameLaunched nor
    // gameExited fire for it. (The other half of the fix -- that a mid-game
    // crash does NOT also fire launchFailed -- isn't covered by an automated
    // test here: simulating "starts successfully, then crashes" needs either
    // a custom crash-inducing test binary or a way to reach into the live
    // QProcess/PID from outside EmulatorProvider, and the fix itself is a
    // one-line "only act on FailedToStart" gate that's easily verified by
    // inspection once this positive case is proven to still work.)
    void launchGameEmitsLaunchFailedNotGameExitedWhenRetroArchFailsToStart() {
        QTemporaryDir dir;
        NetworkManager networkManager;
        EmulatorProvider provider(dir.path(), &networkManager);

        QDir().mkpath(provider.coresDir());
        QFile(provider.coresDir() + "/fceumm_libretro.dll").open(QIODevice::WriteOnly);
        QDir().mkpath(provider.retroArchDir());
        QFile(provider.retroArchExecutablePath()).open(QIODevice::WriteOnly); // empty: not a valid .exe

        QJsonObject cores; cores["nes"] = "fceumm";
        QJsonObject state; state["retroarch"] = true; state["cores"] = cores;
        QDir().mkpath(QFileInfo(provider.installedStatePath()).absolutePath());
        QFile stateFile(provider.installedStatePath());
        QVERIFY(stateFile.open(QIODevice::WriteOnly));
        stateFile.write(QJsonDocument(state).toJson());
        stateFile.close();

        QSignalSpy failedSpy(&provider, &EmulatorProvider::launchFailed);
        QSignalSpy exitedSpy(&provider, &EmulatorProvider::gameExited);
        QSignalSpy launchedSpy(&provider, &EmulatorProvider::gameLaunched);

        provider.launchGame("C:/roms/Zelda.nes", "nes");

        // On Windows, QProcess::start() calls CreateProcess() synchronously,
        // so a FailedToStart error (an empty file isn't a valid Win32
        // executable) is already reflected in the spy by the time
        // launchGame() returns -- no event-loop wait() needed (and calling
        // it here would wrongly wait for a *second*, never-coming emission).
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(exitedSpy.count(), 0);
        QCOMPARE(launchedSpy.count(), 0);
    }

private:
    QTemporaryDir m_tempZipDir;
};

QTEST_MAIN(EmulatorProviderTest)
#include "EmulatorProviderTest.moc"
