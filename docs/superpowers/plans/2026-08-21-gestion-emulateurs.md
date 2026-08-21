# Gestion des Émulateurs (RetroArch + cores libretro) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the emulator-management stub with a real implementation:
download/install/uninstall RetroArch and libretro cores, and launch a game
from `GameDetailsScreen`/`GameListScreen`.

**Architecture:** `EmulatorCatalog` fetches and parses a JSON manifest
hosted in the Bili GitHub repo (which system needs which core, and where to
download RetroArch itself). `EmulatorProvider` (a `QObject`, exposed
directly to QML as a context property — no interface/bridge layer, matching
sub-project 2's `LibraryScanner`/`LibraryModel` pattern) owns install state
(a small local JSON file, independent of the remote catalog so "is X
installed" never depends on network access), downloads (`NetworkManager`,
already built), extraction (`miniz` for `.zip` cores, a vendored `7za.exe`
run via `QProcess` for RetroArch's `.7z`), uninstall, and launching a game
(`QProcess`, not detached, so Bili can react when the game exits).
`RetroArchAutoconfig` translates the SDL2 controller mapping `GamepadBridge`
already uses into a RetroArch autoconfig profile.

**Tech Stack:** C++17, Qt6 (Core, Network, Concurrent — already linked),
`miniz` (already vendored), a newly-vendored `7za.exe`, SDL2 (already
linked, for querying the current controller's mapping).

**Spec:** `docs/superpowers/specs/2026-08-21-gestion-emulateurs-design.md`

## Global Constraints

- Scope: RetroArch + libretro cores only. No standalone emulators
  (Dolphin, PCSX2, etc.), no in-game screenshot capture, no netplay — all
  explicitly deferred per the spec.
- Catalog is a JSON manifest hosted by the Bili project itself (in the
  `baiddd/bili` GitHub repo, fetched via `raw.githubusercontent.com`), not
  a hardcoded catalog and not a direct dependency on libretro's buildbot
  directory structure.
- "Is RetroArch/a core installed" must be answerable **without any network
  access** — it is a local, on-disk state check, never gated on the
  catalog having been fetched successfully.
- RetroArch runs in **portable mode**: its own config/saves/save-states
  stay under `data/emulators/retroarch/`, never in the Windows user
  profile.
- Cores are `.zip` (extract with `miniz`, already vendored — see
  `docs/index.md`). RetroArch itself is only available as `.7z` on the
  official buildbot (verified directly against
  `buildbot.libretro.com/stable/` during brainstorming — no portable
  `.zip` option exists) — extracted by shelling out to a vendored
  `7za.exe` via `QProcess`, not a new C++ library.
- Launching a game uses `QProcess` (not detached) so Bili can react to the
  game process exiting — never fire-and-forget.
- Never guess a library/API/URL/format you're not sure about — research it
  (web search, official docs, `gh api`) before writing code that depends
  on it, and record what you chose and why in `docs/index.md` alongside
  the existing `miniz` entry.
- Toolchain is MinGW-w64/Qt-mingw on `D:\Qt`, SDL2 at
  `D:\SDL2\x86_64-w64-mingw32`. Every `cmake`/`ctest`/`ninja` command
  assumes `platform/windows/dev-env.ps1` has been dot-sourced first.
- Follow the existing project convention: build-file touches
  (`core/CMakeLists.txt`, `tests/*/CMakeLists.txt`) are described in each
  task's Steps even where not spelled out in a task's Files header — the
  Steps are the source of truth.
- The remote repo is `git@github.com:baiddd/bili.git` (already pushed,
  `origin`/`master`). Any task that needs the manifest to be actually
  fetchable must commit **and push** it, not just commit locally.

## Architecture note: retiring `IEmulatorProvider`/`StubEmulatorProvider`/`EmulatorProviderQmlBridge`

Sub-project 1 built `IEmulatorProvider` (a plain, non-`QObject` interface),
`StubEmulatorProvider` (its only implementation, returning "non
implémenté" strings), and `EmulatorProviderQmlBridge` (a `QObject` wrapper
that adapts the plain interface's synchronous `QString`-returning methods
to something QML can call). That design made sense when the real
implementation didn't exist yet and the exact shape of a future one was
unknown.

This plan needs real async behavior — download progress signals, install/
uninstall completion, game-launch lifecycle — which a synchronous
`QString installRetroArch()` return value can't carry. Rather than bolt
signals onto the bridge while the interface stays synchronous
underneath, this plan retires all three files and introduces a single
concrete `EmulatorProvider : public QObject`, exposed directly to QML as a
context property — exactly the pattern sub-project 2 already established
for `LibraryScanner`/`LibraryModel` (no interface/bridge layer for a
single, real implementation). `IScraperProvider`/`StubScraperProvider` and
`INetplaySession`/`StubNetplaySession` are untouched — they still have no
real implementation, so their stub pattern still earns its keep for now.

## Task 1: Retire the emulator stub; `EmulatorProvider` skeleton with local install-state detection

**Files:**
- Delete: `core/emulators/IEmulatorProvider.h`
- Delete: `core/emulators/StubEmulatorProvider.h`
- Delete: `core/emulators/StubEmulatorProvider.cpp`
- Delete: `core/emulators/EmulatorProviderQmlBridge.h`
- Delete: `tests/emulators/StubEmulatorProviderTest.cpp`
- Create: `core/emulators/EmulatorProvider.h`
- Create: `core/emulators/EmulatorProvider.cpp`
- Modify: `core/library/RomScanner.h`
- Modify: `core/library/RomScanner.cpp`
- Modify: `core/CMakeLists.txt`
- Modify: `tests/emulators/CMakeLists.txt`
- Test: `tests/emulators/EmulatorProviderTest.cpp`
- Test: `tests/library/RomScannerTest.cpp` (append)

**Interfaces:**
- Consumes: nothing new yet (later tasks add `NetworkManager`/`EmulatorCatalog` dependencies to this same class).
- Produces:
  ```cpp
  // RomScanner addition
  static QStringList RomScanner::knownSystems();
  // returns {"nes", "snes", "gba", "gb", "n64", "genesis"} — the distinct
  // systems detectSystem()'s extension table maps to, for UI code (Task 8)
  // that needs to list "one row per known system" without hardcoding it
  // a second time.

  // EmulatorProvider (constructor + state-query surface other tasks build on)
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
  };
  ```
  Task 3/4 add `installCore`/`installRetroArch` (which write to
  `installedStatePath()` on success). Task 5 adds uninstall (which removes
  entries from it). Task 6 adds `launchGame`. Task 8 wires this class into
  `app/main.cpp` as the `EmulatorProvider` QML context property, replacing
  `EmulatorProviderQmlBridge`.

**Design of the local install-state file** (`installed.json`): a small
JSON file, independent of the remote catalog, so "is X installed" is
always a fast, synchronous, offline-safe check:

```json
{
  "retroarch": true,
  "cores": {
    "nes": "fceumm",
    "snes": "snes9x"
  }
}
```

`isRetroArchInstalled()`/`isCoreInstalled(system)` check **both** that the
state file records an install **and** that the expected file still
exists on disk — if the file's gone (user deleted it manually), treat it
as not-installed (self-healing, never trusts a stale record over reality).

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/library/RomScannerTest.cpp (append)
void knownSystemsReturnsEverySupportedSystem() {
    const QStringList systems = RomScanner::knownSystems();
    QCOMPARE(systems.size(), 6);
    for (const QString &expected : {"nes", "snes", "gba", "gb", "n64", "genesis"}) {
        QVERIFY(systems.contains(expected));
    }
}
```

```cpp
// tests/emulators/EmulatorProviderTest.cpp
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
```

- [ ] **Step 2: Wire the test target, run, verify it fails**

```cmake
# tests/emulators/CMakeLists.txt (replace the StubEmulatorProviderTest block)
qt_add_executable(EmulatorProviderTest EmulatorProviderTest.cpp)
target_link_libraries(EmulatorProviderTest PRIVATE Qt6::Test bili-core)
add_test(NAME EmulatorProviderTest COMMAND EmulatorProviderTest)
```

Delete `tests/emulators/StubEmulatorProviderTest.cpp` in this same step
(the class it tests no longer exists after this task).

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R "EmulatorProviderTest|RomScannerTest"`
Expected: FAIL (compile error — `EmulatorProvider`/`knownSystems` don't exist yet).

- [ ] **Step 3: Delete the retired stub files**

```bash
git rm core/emulators/IEmulatorProvider.h core/emulators/StubEmulatorProvider.h core/emulators/StubEmulatorProvider.cpp core/emulators/EmulatorProviderQmlBridge.h
```

- [ ] **Step 4: Add `RomScanner::knownSystems()`**

```cpp
// core/library/RomScanner.h (add to the public section)
static QStringList knownSystems();
```

```cpp
// core/library/RomScanner.cpp (add near detectSystem, reusing its table)
QStringList RomScanner::knownSystems() {
    static const QMap<QString, QString> kExtensionToSystem = {
        {"nes", "nes"},
        {"sfc", "snes"}, {"smc", "snes"},
        {"gba", "gba"},
        {"gb", "gb"}, {"gbc", "gb"},
        {"n64", "n64"}, {"z64", "n64"},
        {"md", "genesis"}, {"gen", "genesis"},
    };
    QStringList systems = kExtensionToSystem.values();
    systems.removeDuplicates();
    return systems;
}
```

(This duplicates the extension table already in `detectSystem` rather than
refactoring both to share one static table — a small, deliberate
duplication favored over restructuring `detectSystem`'s already-reviewed,
working implementation for a one-line addition. If a future task ever
needs to *change* the table, do it in both places or refactor then, not
speculatively now.)

- [ ] **Step 5: Write `EmulatorProvider.h`/`.cpp`**

```cpp
// core/emulators/EmulatorProvider.h
#pragma once
#include <QObject>
#include <QString>

class EmulatorProvider : public QObject {
    Q_OBJECT
public:
    explicit EmulatorProvider(QString dataDir, QObject *parent = nullptr);

    Q_INVOKABLE bool isRetroArchInstalled() const;
    Q_INVOKABLE bool isCoreInstalled(const QString &system) const;

    QString retroArchDir() const;
    QString retroArchExecutablePath() const;
    QString coresDir() const;
    QString installedStatePath() const;

protected:
    // Reads installed.json into memory; returns an empty/default state if
    // the file doesn't exist or fails to parse (never treated as an
    // error — "no state file yet" is the normal state on first run).
    struct InstalledState {
        bool retroArch = false;
        QMap<QString, QString> coresBySystem; // system -> core name
    };
    InstalledState readInstalledState() const;

    QString m_dataDir;
};
```

```cpp
// core/emulators/EmulatorProvider.cpp
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
```

Add `emulators/EmulatorProvider.cpp` to `bili-core`'s sources and remove
`emulators/StubEmulatorProvider.cpp` from the list in `core/CMakeLists.txt`.

- [ ] **Step 6: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: PASS (all tests, including the new `EmulatorProviderTest` and
`RomScannerTest`'s new case; `StubEmulatorProviderTest` no longer exists so
the total test-binary count drops by one from sub-project 2's final count
before rising again in later tasks).

- [ ] **Step 7: Commit**

```bash
git add -A core/emulators tests/emulators core/library/RomScanner.h core/library/RomScanner.cpp tests/library/RomScannerTest.cpp core/CMakeLists.txt
git commit -m "feat: retire emulator stub, add EmulatorProvider with local install-state detection"
```

---

## Task 2: `EmulatorCatalog` — remote manifest fetch and parse

**Files:**
- Create: `core/emulators/EmulatorCatalog.h`
- Create: `core/emulators/EmulatorCatalog.cpp`
- Create: `catalog/emulators.json` (in the repo root — this is the file the app fetches at runtime via its raw GitHub URL)
- Modify: `core/CMakeLists.txt`
- Modify: `tests/emulators/CMakeLists.txt`
- Modify: `docs/index.md`
- Test: `tests/emulators/EmulatorCatalogTest.cpp`

**Interfaces:**
- Consumes: `NetworkManager::startDownload(QUrl, QString)` / `progress(int, qint64, qint64)` / `finished(int, QString)` / `failed(int, QString)` (sub-project 1, unchanged).
- Produces:
  ```cpp
  struct CoreCatalogEntry {
      QString core; // e.g. "fceumm" — matches the *_libretro.dll basename
      QUrl url;     // .zip download URL
  };

  struct EmulatorCatalogData {
      QString retroArchVersion;
      QUrl retroArchUrl; // .7z download URL
      QMap<QString, CoreCatalogEntry> coresBySystem; // system -> entry
  };

  class EmulatorCatalog : public QObject {
      Q_OBJECT
  public:
      explicit EmulatorCatalog(NetworkManager *networkManager, QObject *parent = nullptr);
      void fetch(const QUrl &manifestUrl);

  signals:
      void ready(const EmulatorCatalogData &data);
      void failed(const QString &errorString);
  };
  ```
  Task 3/4 call `fetch()` (via `EmulatorProvider`, which owns an
  `EmulatorCatalog` instance) and use `EmulatorCatalogData` to know what to
  download and from where.

**Before writing any code:** research the current, real RetroArch stable
release and libretro core download URLs — do not guess or reuse the
illustrative names from the spec without verifying them still resolve.
Confirm via `WebFetch`/`gh api` (this project's established convention,
matching how SDL2/miniz were sourced):
1. The latest stable RetroArch version directory under
   `https://buildbot.libretro.com/stable/` and the exact filename of its
   Windows x86_64 `.7z` (confirmed during brainstorming to be named
   `RetroArch.7z` as of version `1.22.2` — verify this is still current,
   or use whatever the latest stable version actually is at
   implementation time).
2. The exact filenames of the `fceumm`, `snes9x`, `mgba`,
   `mupen64plus_next`, and `genesis_plus_gx` cores under
   `https://buildbot.libretro.com/nightly/windows/x86_64/latest/` (all
   confirmed to exist with `_libretro.dll.zip` names during
   brainstorming — verify current filenames, and prefer a numbered stable
   core build over "latest"/nightly if the buildbot offers one, for
   reproducibility; if only nightly builds exist for cores, that's an
   acceptable, documented choice — record which you picked and why in
   `docs/index.md`).

- [ ] **Step 1: Write the failing test**

```cpp
// tests/emulators/EmulatorCatalogTest.cpp
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include "emulators/EmulatorCatalog.h"
#include "network/NetworkManager.h"

class EmulatorCatalogTest : public QObject {
    Q_OBJECT
private slots:
    void parsesAValidManifest() {
        const QByteArray body = R"({
            "retroarch": {"version": "1.22.2", "windows_x64_url": "https://example.invalid/RetroArch.7z"},
            "cores": {
                "nes": {"core": "fceumm", "url": "https://example.invalid/fceumm.zip"},
                "snes": {"core": "snes9x", "url": "https://example.invalid/snes9x.zip"}
            }
        })";

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server, &body]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: "
                + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        QSignalSpy readySpy(&catalog, &EmulatorCatalog::ready);
        QSignalSpy failedSpy(&catalog, &EmulatorCatalog::failed);

        catalog.fetch(QUrl(QString("http://127.0.0.1:%1/manifest.json").arg(server.serverPort())));

        QVERIFY(readySpy.wait(5000));
        QCOMPARE(failedSpy.count(), 0);
        const EmulatorCatalogData data = readySpy.first().at(0).value<EmulatorCatalogData>();
        QCOMPARE(data.retroArchVersion, QString("1.22.2"));
        QCOMPARE(data.retroArchUrl, QUrl("https://example.invalid/RetroArch.7z"));
        QCOMPARE(data.coresBySystem.size(), 2);
        QCOMPARE(data.coresBySystem.value("nes").core, QString("fceumm"));
    }

    void emitsFailedForMalformedJson() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray body = "{ not valid json";
            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: "
                + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        QSignalSpy readySpy(&catalog, &EmulatorCatalog::ready);
        QSignalSpy failedSpy(&catalog, &EmulatorCatalog::failed);

        catalog.fetch(QUrl(QString("http://127.0.0.1:%1/manifest.json").arg(server.serverPort())));

        QVERIFY(failedSpy.wait(5000));
        QCOMPARE(readySpy.count(), 0);
        QVERIFY(!failedSpy.first().at(0).toString().isEmpty());
    }

    void emitsFailedWhenDownloadItselfFails() {
        quint16 freePort = 0;
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            freePort = probe.serverPort();
        }

        NetworkManager networkManager;
        EmulatorCatalog catalog(&networkManager);
        QSignalSpy failedSpy(&catalog, &EmulatorCatalog::failed);

        catalog.fetch(QUrl(QString("http://127.0.0.1:%1/nope").arg(freePort)));

        QVERIFY(failedSpy.wait(5000));
        QVERIFY(!failedSpy.first().at(0).toString().isEmpty());
    }
};

QTEST_MAIN(EmulatorCatalogTest)
#include "EmulatorCatalogTest.moc"
```

- [ ] **Step 2: Wire the test target, run, verify it fails**

```cmake
# tests/emulators/CMakeLists.txt (append)
qt_add_executable(EmulatorCatalogTest EmulatorCatalogTest.cpp)
target_link_libraries(EmulatorCatalogTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME EmulatorCatalogTest COMMAND EmulatorCatalogTest)
```

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R EmulatorCatalogTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write `EmulatorCatalog.h`/`.cpp`**

```cpp
// core/emulators/EmulatorCatalog.h
#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include <QMap>
#include <QMetaType>
#include "network/NetworkManager.h"

struct CoreCatalogEntry {
    QString core;
    QUrl url;
};

struct EmulatorCatalogData {
    QString retroArchVersion;
    QUrl retroArchUrl;
    QMap<QString, CoreCatalogEntry> coresBySystem;
};
Q_DECLARE_METATYPE(EmulatorCatalogData)

class EmulatorCatalog : public QObject {
    Q_OBJECT
public:
    explicit EmulatorCatalog(NetworkManager *networkManager, QObject *parent = nullptr);
    void fetch(const QUrl &manifestUrl);

signals:
    void ready(const EmulatorCatalogData &data);
    void failed(const QString &errorString);

private:
    NetworkManager *m_networkManager;
    int m_pendingRequestId = -1;
    QString m_tempPath;
};
```

```cpp
// core/emulators/EmulatorCatalog.cpp
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
        emit failed(errorString);
    });
}

void EmulatorCatalog::fetch(const QUrl &manifestUrl) {
    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    m_tempPath = temp.fileName();
    temp.close();

    m_pendingRequestId = m_networkManager->startDownload(manifestUrl, m_tempPath);
}
```

Add `emulators/EmulatorCatalog.cpp` to `bili-core`'s sources in
`core/CMakeLists.txt`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R EmulatorCatalogTest`
Expected: PASS.

- [ ] **Step 5: Create and publish the real manifest**

Using the URLs/filenames verified in the research step above, create
`catalog/emulators.json` at the repo root with the real, current values
(not the illustrative ones from this plan text):

```json
{
  "retroarch": {
    "version": "<verified stable version>",
    "windows_x64_url": "<verified .7z URL>"
  },
  "cores": {
    "nes": { "core": "fceumm", "url": "<verified .zip URL>" },
    "snes": { "core": "snes9x", "url": "<verified .zip URL>" },
    "gba": { "core": "mgba", "url": "<verified .zip URL>" },
    "gb": { "core": "mgba", "url": "<verified .zip URL>" },
    "n64": { "core": "mupen64plus_next", "url": "<verified .zip URL>" },
    "genesis": { "core": "genesis_plus_gx", "url": "<verified .zip URL>" }
  }
}
```

Commit **and push** it (this file must actually be fetchable at
`https://raw.githubusercontent.com/baiddd/bili/master/catalog/emulators.json`
for the app to work):

```bash
git add catalog/emulators.json
git commit -m "chore: add the RetroArch/cores download catalog manifest"
git push origin master
```

After pushing, verify the raw URL actually serves the file (e.g.
`curl -s https://raw.githubusercontent.com/baiddd/bili/master/catalog/emulators.json`)
before considering this step done — GitHub's raw CDN can take a short
moment to reflect a fresh push; if the first check fails, wait briefly and
retry rather than assuming something is broken.

- [ ] **Step 6: Update `docs/index.md`**

Add a row documenting the manifest URL constant and, separately, a row (or
an addition to the existing miniz row's table) recording which
RetroArch version / core build channel (stable vs. nightly) was chosen and
why.

- [ ] **Step 7: Commit**

```bash
git add core/emulators/EmulatorCatalog.h core/emulators/EmulatorCatalog.cpp core/CMakeLists.txt tests/emulators/EmulatorCatalogTest.cpp tests/emulators/CMakeLists.txt docs/index.md
git commit -m "feat: fetch and parse the emulator download catalog manifest"
```

---

## Task 3: Install a core (`.zip` via `miniz`) with progress

**Files:**
- Modify: `core/emulators/EmulatorProvider.h`
- Modify: `core/emulators/EmulatorProvider.cpp`
- Modify: `tests/emulators/CMakeLists.txt`
- Test: `tests/emulators/EmulatorProviderTest.cpp` (append)

**Interfaces:**
- Consumes: `EmulatorCatalog`/`EmulatorCatalogData` (Task 2), `NetworkManager` (sub-project 1), `miniz.h`'s `mz_zip_reader_*` functions (already vendored, used read-only in `RomScanner`; this task additionally uses its file-extraction functions to actually write files to disk).
- Produces:
  ```cpp
  // EmulatorProvider additions
  explicit EmulatorProvider(QString dataDir, NetworkManager *networkManager, QObject *parent = nullptr);
  // (constructor signature changes to take NetworkManager — Task 8 updates main.cpp's construction call accordingly)

  Q_INVOKABLE void installCore(const QString &system);

  signals:
      void installProgress(const QString &target, qint64 bytesReceived, qint64 bytesTotal);
      void installFinished(const QString &target);
      void installFailed(const QString &target, const QString &errorString);
  ```
  `target` is `"retroarch"` or `"core:<system>"` (e.g. `"core:nes"`) — a
  simple string tag so QML (Task 8) can route progress/completion to the
  right row in `EmulatorManagerScreen`'s list without a second lookup
  table. Task 4 reuses the same three signals for RetroArch's own install
  (with `target == "retroarch"`), Task 5 reuses `target` for uninstall
  signals too.

**Before writing any code:** confirm `miniz.h`'s actual extraction API by
reading `core/third_party/miniz/miniz.h` directly (it's already vendored —
don't guess function names). `RomScanner.cpp`'s `scanZipArchive` only uses
`mz_zip_reader_init_file`/`get_num_files`/`get_filename`/`is_file_a_directory`/`reader_end`
to read entries virtually; this task additionally needs a function that
extracts an entry's actual bytes to a real file on disk (miniz exposes
`mz_zip_reader_extract_to_file`, taking the archive, entry index, and a
destination path — verify the exact signature/flags parameter in the
vendored header before using it).

**This task also changes an existing constructor signature** —
`EmulatorProvider`'s constructor gains a required `NetworkManager *`
parameter. Task 1's three existing tests in `EmulatorProviderTest.cpp`
(`reportsNotInstalledWhenNothingOnDisk`,
`reportsInstalledWhenStateFileAndRealFileBothExist`,
`selfHealsWhenStateFileClaimsInstallButFileIsMissing`) all construct
`EmulatorProvider` with the old single-argument form and will fail to
compile once this task's header change lands — update each of their
`EmulatorProvider provider(dir.path());` (and the `reloaded` variant)
call sites to `EmulatorProvider provider(dir.path(), &networkManager);`,
adding a local `NetworkManager networkManager;` to each of those three
test functions (they don't otherwise exercise networking, so it's
constructed but unused beyond satisfying the constructor). Do this as
part of Step 1 below, alongside adding the new tests, so the whole file
compiles and passes together.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/emulators/EmulatorProviderTest.cpp (append)
// Uses a local HTTP server serving a real, tiny, valid .zip fixture
// (build it with the same miniz writer API RomScanner's own Task 3 test
// used to construct its fixture .zip — check tests/library/RomScannerTest.cpp
// for that exact pattern and reuse it here, so this test doesn't need a
// second zip-construction mechanism).
void installCoreDownloadsExtractsAndRecordsState() {
    QByteArray zipBytes = /* built via mz_zip_writer_* the same way
                              RomScannerTest's zip-archive fixture is built,
                              containing one entry "fceumm_libretro.dll"
                              with arbitrary content */;

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

    // installCore() needs a catalog entry to know the URL/core name for
    // "nes" -- inject one directly for this unit test rather than going
    // through a real EmulatorCatalog fetch (that's EmulatorCatalogTest's
    // job): expose a testing-only setter/overload for this, e.g.
    // provider.installCoreFrom("nes", CoreCatalogEntry{"fceumm", QUrl(...)});
    // with installCore(system) (the QML-facing entry point) looking the
    // entry up from its own already-fetched EmulatorCatalogData and
    // delegating to this same internal method.
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
```

- [ ] **Step 2: Add the `Qt6::Network` link and run to verify it fails**

The new tests use `QTcpServer`/`QTcpSocket` (from `Qt6::Network`), which
Task 1's `tests/emulators/CMakeLists.txt` entry doesn't link:

```cmake
# tests/emulators/CMakeLists.txt — update the EmulatorProviderTest block
qt_add_executable(EmulatorProviderTest EmulatorProviderTest.cpp)
target_link_libraries(EmulatorProviderTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME EmulatorProviderTest COMMAND EmulatorProviderTest)
```

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R EmulatorProviderTest`
Expected: FAIL (compile error — `installCoreFrom`/`NetworkManager`
constructor param/signals don't exist yet).

- [ ] **Step 3: Implement**

```cpp
// core/emulators/EmulatorProvider.h (additions)
#include "EmulatorCatalog.h"
#include "network/NetworkManager.h"

// ... inside the class, replace the old single-arg constructor:
explicit EmulatorProvider(QString dataDir, NetworkManager *networkManager, QObject *parent = nullptr);

Q_INVOKABLE void installCore(const QString &system);
// Testing-only entry point that skips the catalog lookup (EmulatorCatalogTest
// already covers catalog parsing; this lets EmulatorProviderTest exercise
// download+extract+state-recording in isolation).
void installCoreFrom(const QString &system, const CoreCatalogEntry &entry);

signals:
    void installProgress(const QString &target, qint64 bytesReceived, qint64 bytesTotal);
    void installFinished(const QString &target);
    void installFailed(const QString &target, const QString &errorString);

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
```

```cpp
// core/emulators/EmulatorProvider.cpp (additions)
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
```

Every install/uninstall path in this plan (Task 3's core install above,
Task 4's RetroArch install, Task 5's uninstall paths) follows the same
read-modify-write shape: `InstalledState state = readInstalledState();`,
mutate the relevant field, `persistInstalledState(state);` — no other
task re-derives the JSON-building logic.

Note the extraction fixture's expected `.zip`-entry name in the test
above must match the entry name pattern `installCoreFrom` looks for
(`"<core>_libretro.dll"`) — build the test fixture zip with an entry
literally named `fceumm_libretro.dll` to match.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: PASS (all tests).

- [ ] **Step 5: Commit**

```bash
git add core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp tests/emulators/EmulatorProviderTest.cpp tests/emulators/CMakeLists.txt
git commit -m "feat: install libretro cores from the catalog (.zip via miniz)"
```

---

## Task 4: Install RetroArch (`.7z` via a vendored `7za.exe`)

**Files:**
- Create: `platform/windows/tools/7za.exe` (vendored binary)
- Modify: `core/emulators/EmulatorProvider.h`
- Modify: `core/emulators/EmulatorProvider.cpp`
- Modify: `platform/windows/dev-env.ps1`
- Modify: `platform/windows/publish_windows.ps1`
- Modify: `docs/index.md`
- Test: `tests/emulators/EmulatorProviderTest.cpp` (append)

**Interfaces:**
- Consumes: `EmulatorCatalogData::retroArchUrl` (Task 2), the same
  `NetworkManager`/download-tracking machinery Task 3 built.
- Produces:
  ```cpp
  Q_INVOKABLE void installRetroArch();
  void installRetroArchFrom(const QUrl &url); // testing-only entry point, mirrors installCoreFrom
  ```
  Reuses `installProgress`/`installFinished`/`installFailed` from Task 3
  with `target == "retroarch"`. Task 5/6 depend on RetroArch actually
  being extracted into `retroArchExecutablePath()` afterward.

**Before writing any code — two things to research, do not guess:**
1. **Source and vendor `7za.exe`**: find 7-Zip's official current download
   page (e.g. 7-zip.org's downloads section) and identify the "7-Zip
   Extra" package (the standalone console-only `7za.exe`, distinct from
   the full GUI installer) — verify the current version and download URL,
   download it, and vendor `7za.exe` under `platform/windows/tools/`.
   Confirm its license (7-Zip's core, including `7za.exe`, is public
   domain — verify this against the actual license file the download
   provides rather than assuming) and record it in `docs/index.md`.
2. **RetroArch's portable-mode requirements**: check RetroArch's own
   documentation (or the extracted `.7z`'s own contents/README) for how
   its Windows build behaves when extracted standalone — confirm whether
   it's portable by default (writes its config/saves next to the exe
   automatically) or needs an explicit `retroarch.cfg` with specific keys
   (e.g. `system_directory`, `savefile_directory`) to stay fully contained
   under `data/emulators/retroarch/` rather than touching
   `%APPDATA%`/the user profile. Implement whichever this research shows
   is actually required — do not assume portable-by-default without
   checking.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/emulators/EmulatorProviderTest.cpp (append)
// This test fakes 7za.exe's behavior (a tiny script/batch file that just
// creates the expected output file) rather than depending on the real
// vendored binary or a real .7z archive, so it stays fast and hermetic —
// exercising EmulatorProvider's orchestration logic (QProcess invocation,
// argument construction, state recording), not 7-Zip's own extraction
// correctness. Use QTemporaryDir to point EmulatorProvider's expected
// "7za.exe location" at a fake stand-in executable for this test only via
// whatever seam Step 3's implementation exposes for it (e.g. a
// testing-only setter for the resolved sevenZipPath, mirroring how other
// tasks in this project exposed static/testable seams for otherwise
// hard-to-test system interactions, e.g. SystemController::programName()).
void installRetroArchExtractsAndRecordsState() {
    QTemporaryDir dir;
    NetworkManager networkManager;
    EmulatorProvider provider(dir.path(), &networkManager);

    // Fake archive content doesn't need to be a real .7z for this test,
    // since the fake 7za.exe stand-in ignores its input and just creates
    // retroarch.exe directly -- see the stand-in script referenced above.

    QSignalSpy finishedSpy(&provider, &EmulatorProvider::installFinished);
    provider.installRetroArchFrom(QUrl("http://127.0.0.1:1/unused")); // exact URL irrelevant once download itself is faked the same way Task 3's tests faked it -- reuse that same local-HTTP-server pattern here, serving arbitrary bytes as the "archive"

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(finishedSpy.first().at(0).toString(), QString("retroarch"));
    QVERIFY(provider.isRetroArchInstalled());
}
```

(This test skeleton intentionally leaves the exact fake-`7za.exe`
seam/mechanism to Step 3's implementation — write the concrete version
once you've decided, in Step 3, exactly how `EmulatorProvider` resolves
the path to `7za.exe`, since the test needs to override that same seam.)

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R EmulatorProviderTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Implement**

```cpp
// core/emulators/EmulatorProvider.h (additions)
#include <QString>

Q_INVOKABLE void installRetroArch();
void installRetroArchFrom(const QUrl &url);

// Exposed for testing: where this class looks for 7za.exe. Searches PATH
// plus the running application's own directory (so the shipped, portable
// app finds it next to Bili.exe without relying on the launching shell's
// PATH, while a dev build finds it via dev-env.ps1's PATH addition).
static QString sevenZipExecutablePath();

private:
bool extract7zArchive(const QString &archivePath, const QString &destDir);
```

```cpp
// core/emulators/EmulatorProvider.cpp (additions)
#include <QStandardPaths>
#include <QCoreApplication>
#include <QProcess>

QString EmulatorProvider::sevenZipExecutablePath() {
    return QStandardPaths::findExecutable("7za", {QCoreApplication::applicationDirPath()});
}

void EmulatorProvider::installRetroArch() {
    if (m_catalogData.retroArchUrl.isEmpty()) {
        emit installFailed("retroarch", "Catalogue non chargé — réessaie.");
        return;
    }
    installRetroArchFrom(m_catalogData.retroArchUrl);
}

void EmulatorProvider::installRetroArchFrom(const QUrl &url) {
    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    const QString tempPath = temp.fileName();
    temp.close();

    const int requestId = m_networkManager->startDownload(url, tempPath);
    m_activeDownloadTargets.insert(requestId, "retroarch");
}

bool EmulatorProvider::extract7zArchive(const QString &archivePath, const QString &destDir) {
    const QString sevenZip = sevenZipExecutablePath();
    if (sevenZip.isEmpty()) return false;

    QDir().mkpath(destDir);
    QProcess process;
    process.start(sevenZip, {"x", archivePath, "-o" + destDir, "-y"});
    if (!process.waitForFinished(60000)) return false;
    return process.exitCode() == 0;
}
```

Extend the `NetworkManager::finished` lambda from Task 3 with the
`"retroarch"` branch:

```cpp
// Inside the existing connect(m_networkManager, &NetworkManager::finished, ...) lambda from Task 3:
if (target == "retroarch") {
    if (!extract7zArchive(destPath, retroArchDir())) {
        QFile::remove(destPath);
        emit installFailed(target, "Échec de l'extraction de RetroArch.");
        return;
    }
    QFile::remove(destPath);
    InstalledState state = readInstalledState();
    state.retroArch = true;
    persistInstalledState(state);
    emit installFinished(target);
    return;
}
```

Apply whatever portable-mode configuration Step 0's research determined is
needed (either nothing further, if extraction alone is portable by
default, or writing a `retroarch.cfg` with the specific keys found) as
part of this same `installRetroArch` flow, right after extraction
succeeds.

- [ ] **Step 4: Vendor `7za.exe` and wire the build/dist scripts**

```powershell
# platform/windows/dev-env.ps1 (add to the existing $env:PATH line)
$env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.8.3\mingw_64\bin;D:\SDL2\x86_64-w64-mingw32\bin;$PSScriptRoot\tools;$env:PATH"
```

```powershell
# platform/windows/publish_windows.ps1 (add alongside the existing SDL2.dll copy)
Copy-Item "$PSScriptRoot\tools\7za.exe" -Destination $AppDir -Force
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Update `docs/index.md`**

Add a row for the vendored `7za.exe` (version, source URL, license — from
the research in Step 0) and a note updating the existing `.7z` gap entry:
ROM archive scanning (`RomScanner`) is still `.zip`-only and unaffected;
RetroArch's own installation now uses `7za.exe` via `QProcess`, a
completely separate mechanism from ROM scanning.

- [ ] **Step 7: Commit**

```bash
git add platform/windows/tools/7za.exe core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp platform/windows/dev-env.ps1 platform/windows/publish_windows.ps1 docs/index.md tests/emulators/EmulatorProviderTest.cpp
git commit -m "feat: install RetroArch itself (.7z via a vendored 7za.exe)"
```

---

## Task 5: Uninstall RetroArch and cores

**Files:**
- Modify: `core/emulators/EmulatorProvider.h`
- Modify: `core/emulators/EmulatorProvider.cpp`
- Test: `tests/emulators/EmulatorProviderTest.cpp` (append)

**Interfaces:**
- Consumes: `installedStatePath()`/`readInstalledState()`/`persistInstalledState()` (Task 1/3).
- Produces:
  ```cpp
  Q_INVOKABLE void uninstallRetroArch();
  Q_INVOKABLE void uninstallCore(const QString &system);

  signals:
      void uninstallFinished(const QString &target); // reuses the same "retroarch" / "core:<system>" tags
      void uninstallFailed(const QString &target, const QString &errorString);
  ```
  Task 8 wires these to "Désinstaller" buttons on `EmulatorManagerScreen`.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/emulators/EmulatorProviderTest.cpp (append)
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
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R EmulatorProviderTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Implement**

```cpp
// core/emulators/EmulatorProvider.h (additions)
Q_INVOKABLE void uninstallRetroArch();
Q_INVOKABLE void uninstallCore(const QString &system);

signals:
    void uninstallFinished(const QString &target);
    void uninstallFailed(const QString &target, const QString &errorString);
```

```cpp
// core/emulators/EmulatorProvider.cpp (additions)
void EmulatorProvider::uninstallCore(const QString &system) {
    const InstalledState state = readInstalledState();
    const QString core = state.coresBySystem.value(system);
    const QString target = "core:" + system;
    if (core.isEmpty()) {
        emit uninstallFailed(target, "Ce core n'est pas installé.");
        return;
    }

    const QString path = coresDir() + "/" + core + "_libretro.dll";
    if (QFile::exists(path) && !QFile::remove(path)) {
        emit uninstallFailed(target, "Impossible de supprimer le fichier du core.");
        return;
    }

    InstalledState newState = state;
    newState.coresBySystem.remove(system);
    persistInstalledState(newState);

    emit uninstallFinished(target);
}

void EmulatorProvider::uninstallRetroArch() {
    if (!QDir(retroArchDir()).removeRecursively()) {
        // removeRecursively() also returns true if the directory simply
        // doesn't exist, so a false result here is a genuine failure
        // (e.g. a file still open/locked), not "already uninstalled".
        emit uninstallFailed("retroarch", "Impossible de supprimer RetroArch (fichier verrouillé ?).");
        return;
    }

    InstalledState state = readInstalledState();
    state.retroArch = false;
    persistInstalledState(state);

    emit uninstallFinished("retroarch");
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp tests/emulators/EmulatorProviderTest.cpp
git commit -m "feat: uninstall RetroArch and individual cores"
```

---

## Task 6: Launch a game

**Files:**
- Modify: `core/emulators/EmulatorProvider.h`
- Modify: `core/emulators/EmulatorProvider.cpp`
- Test: `tests/emulators/EmulatorProviderTest.cpp` (append)

**Interfaces:**
- Consumes: `coresDir()`/`retroArchExecutablePath()`/`readInstalledState()` (Task 1), a `rom_path` string possibly containing the `"<archive>::<entry>"` convention (sub-project 2's `RomScanner`).
- Produces:
  ```cpp
  Q_INVOKABLE void launchGame(const QString &romPath, const QString &system);

  signals:
      void gameLaunched();
      void gameExited(int exitCode);
      void launchFailed(const QString &errorString);

  // Exposed for testing: builds the retroarch.exe argument list without
  // ever actually starting a process, same pattern as
  // SystemController::restartArgs()/shutdownArgs() from the socle.
  static QStringList launchArgs(const QString &corePath, const QString &resolvedRomPath);
  ```

**Before writing any code:** check RetroArch's own documentation for how
it loads content directly from inside a `.zip`/other archive (a `rom_path`
of the form `"<archive-path>::<entry-name>"`, per sub-project 2's
convention, needs translating into whatever RetroArch actually expects —
do not guess the argument format). If no clean built-in option exists,
the fallback is: extract the specific entry to a temporary file (reusing
`miniz`'s extraction, same function `extractZipEntry` from Task 3 —
generalize it slightly if needed to extract by entry name into an
arbitrary temp dir rather than assuming the `<core>_libretro.dll` naming
convention) and pass that temporary path to RetroArch instead.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/emulators/EmulatorProviderTest.cpp (append)
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
```

(A test that actually launches a real `retroarch.exe` process and waits
for `gameExited` is deliberately not written here — it would need a real
RetroArch install and a real ROM, which is exactly the kind of thing this
project's established convention verifies manually rather than in the
automated suite, matching `SystemController`'s restart/shutdown methods
never being exercised for real in `SystemControllerTest`.)

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R EmulatorProviderTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Implement**

```cpp
// core/emulators/EmulatorProvider.h (additions)
#include <QProcess>

Q_INVOKABLE void launchGame(const QString &romPath, const QString &system);
static QStringList launchArgs(const QString &corePath, const QString &resolvedRomPath);

signals:
    void gameLaunched();
    void gameExited(int exitCode);
    void launchFailed(const QString &errorString);

private:
    QProcess *m_gameProcess = nullptr;
```

```cpp
// core/emulators/EmulatorProvider.cpp (additions)
QStringList EmulatorProvider::launchArgs(const QString &corePath, const QString &resolvedRomPath) {
    return {"-L", corePath, resolvedRomPath};
}

void EmulatorProvider::launchGame(const QString &romPath, const QString &system) {
    const InstalledState state = readInstalledState();
    const QString core = state.coresBySystem.value(system);
    if (core.isEmpty() || !isCoreInstalled(system)) {
        emit launchFailed("Aucun core installé pour ce système.");
        return;
    }
    if (!isRetroArchInstalled()) {
        emit launchFailed("RetroArch n'est pas installé.");
        return;
    }

    // Apply whatever archive-content-loading mechanism the research step
    // above determined RetroArch actually supports; resolvedRomPath is
    // romPath unchanged for a plain on-disk file, or the result of that
    // mechanism (a native archive argument form, or an extracted temp
    // file path) for a "<archive>::<entry>" rom_path.
    const QString resolvedRomPath = romPath; // placeholder for the researched resolution logic

    const QString corePath = coresDir() + "/" + core + "_libretro.dll";

    if (m_gameProcess) {
        m_gameProcess->deleteLater();
    }
    m_gameProcess = new QProcess(this);
    connect(m_gameProcess, &QProcess::started, this, [this]() { emit gameLaunched(); });
    connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus) { emit gameExited(exitCode); });
    connect(m_gameProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError) { emit launchFailed("Impossible de lancer RetroArch."); });

    m_gameProcess->start(retroArchExecutablePath(), launchArgs(corePath, resolvedRomPath));
}
```

Replace the `resolvedRomPath` placeholder line with the actual archive-path
resolution logic once the research step's findings are in hand — this is
one of two spots in this plan intentionally left for a mandated research
step (the other is Task 7's autoconfig key set), consistent with how
sub-project 2's Task 3 handled its own single research-gated line.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp tests/emulators/EmulatorProviderTest.cpp
git commit -m "feat: launch a game through RetroArch with process lifecycle tracking"
```

---

## Task 7: `RetroArchAutoconfig` — generate a default gamepad profile

**Files:**
- Create: `core/emulators/RetroArchAutoconfig.h`
- Create: `core/emulators/RetroArchAutoconfig.cpp`
- Modify: `core/emulators/EmulatorProvider.h`
- Modify: `core/emulators/EmulatorProvider.cpp`
- Modify: `core/CMakeLists.txt`
- Modify: `tests/emulators/CMakeLists.txt`
- Test: `tests/emulators/RetroArchAutoconfigTest.cpp`

**Interfaces:**
- Consumes: SDL2's `SDL_GameControllerMapping`/`SDL_GameControllerName` (already linked via `SDL2::SDL2`, used today only by `GamepadBridge`).
- Produces:
  ```cpp
  class RetroArchAutoconfig {
  public:
      // Builds a RetroArch autoconfig .cfg file's *contents* (not written
      // to disk here, so this stays trivially testable) for the given
      // controller name/GUID-derived mapping string, in RetroArch's
      // autoconfig key=value format.
      static QString buildProfile(const QString &controllerName, const QString &sdlMapping);
  };
  ```
  `EmulatorProvider::launchGame` (Task 6) calls this once, writing the
  result into `data/emulators/retroarch/autoconfig/`, before the first
  launch if no autoconfig files exist yet for any currently-connected
  controller.

**Before writing any code:** research RetroArch's actual autoconfig `.cfg`
key names (e.g. `input_a_btn`, `input_b_btn`, `input_up_btn`, etc. — verify
the exact, complete key set and file format from RetroArch's own
documentation, not guessed) and how it identifies which profile applies to
which physical controller (typically device name + button/axis count, or
a `input_vendor_id`/`input_product_id` pair — confirm which RetroArch
actually uses). Separately, confirm the exact string format
`SDL_GameControllerMapping()` returns (it's a comma-separated
`GUID,name,platform:...,a:b0,b:b1,...` string — verify field meanings
against SDL2's own documentation) so `buildProfile` can correctly
translate from one to the other.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/emulators/RetroArchAutoconfigTest.cpp
#include <QtTest>
#include "emulators/RetroArchAutoconfig.h"

class RetroArchAutoconfigTest : public QObject {
    Q_OBJECT
private slots:
    void buildsAProfileContainingTheControllerName() {
        // Use a real SDL2 GameController mapping string as a fixture (copy
        // one verbatim from SDL2's own bundled gamecontrollerdb.txt for a
        // common pad, e.g. an Xbox 360 controller entry) rather than
        // inventing one, so the test exercises real-world input.
        const QString sdlMapping = "<a real gamecontrollerdb.txt line, verified during implementation>";
        const QString profile = RetroArchAutoconfig::buildProfile("Xbox 360 Controller", sdlMapping);

        QVERIFY(profile.contains("input_device = \"Xbox 360 Controller\""));
        // Additional assertions on specific input_*_btn keys once the
        // exact RetroArch key format is confirmed by the research step —
        // write the concrete expected key=value pairs then, not guessed
        // here.
    }
};

QTEST_MAIN(RetroArchAutoconfigTest)
#include "RetroArchAutoconfigTest.moc"
```

(This is the second and last place in this plan where exact content is
intentionally deferred to a mandated research step, for the same reason
as Task 6's archive-path resolution — RetroArch's precise autoconfig key
set isn't something to invent.)

- [ ] **Step 2: Wire the test target, run, verify it fails**

```cmake
# tests/emulators/CMakeLists.txt (append)
qt_add_executable(RetroArchAutoconfigTest RetroArchAutoconfigTest.cpp)
target_link_libraries(RetroArchAutoconfigTest PRIVATE Qt6::Test bili-core)
add_test(NAME RetroArchAutoconfigTest COMMAND RetroArchAutoconfigTest)
```

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R RetroArchAutoconfigTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Implement**

```cpp
// core/emulators/RetroArchAutoconfig.h
#pragma once
#include <QString>

class RetroArchAutoconfig {
public:
    static QString buildProfile(const QString &controllerName, const QString &sdlMapping);
};
```

```cpp
// core/emulators/RetroArchAutoconfig.cpp
#include "RetroArchAutoconfig.h"
#include <QStringList>

QString RetroArchAutoconfig::buildProfile(const QString &controllerName, const QString &sdlMapping) {
    // Parse sdlMapping's comma-separated "key:value" fields (skipping the
    // leading GUID/name/platform fields) into a lookup, then emit the
    // corresponding RetroArch input_*_btn/axis lines using the exact key
    // names confirmed by this task's research step. Implement the real
    // translation here once that research is done — do not ship a
    // guessed subset of keys.
    QString profile;
    profile += "input_device = \"" + controllerName + "\"\n";
    profile += "input_driver = \"sdl2\"\n";
    // ... remaining input_*_btn / input_*_axis lines, per the confirmed
    // RetroArch autoconfig format.
    return profile;
}
```

**A real thread-affinity concern, not to be guessed past:** `GamepadBridge`
opens each connected controller (`SDL_GameControllerOpen`) on its own
dedicated `QThread` (`m_thread` in `GamepadBridge.cpp`), and doesn't expose
any accessor for the `SDL_GameController*` handles it holds. `EmulatorProvider::launchGame`
runs on the GUI thread. Querying an `SDL_GameController*` handle that was
opened on a different thread is exactly the same class of hazard already
found and fixed once in this project (sub-project 2's cross-thread
`QSqlDatabase` issue) — verify SDL2's actual thread-safety guarantees
before writing this code, don't assume they're the same as Qt's.

Avoid the question entirely by not touching `GamepadBridge`'s opened
handles at all: SDL2 exposes device-index-based query functions —
`SDL_NumJoysticks()`, `SDL_IsGameController(index)`,
`SDL_GameControllerNameForIndex(index)`,
`SDL_GameControllerMappingForDeviceIndex(index)` — that query a connected
device by its enumeration index, without requiring it to already be open.
Confirm in SDL2's own documentation that these specific `*ForIndex`/
`*ForDeviceIndex` functions are safe to call from a thread other than the
one that called `SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK)`
(`GamepadBridge`'s poll thread, in this codebase) — if they are, use them
directly from `EmulatorProvider` on the GUI thread and this task needs no
`GamepadBridge` changes at all; if SDL2's documentation says otherwise,
stop and report back rather than shipping an unverified cross-thread call
— this would need a different design (e.g. `GamepadBridge` computing the
mapping itself on its own thread and handing it to `EmulatorProvider` via
a queued Qt signal), which is a bigger change than this task's stated
scope and should be escalated rather than guessed around.

Add a small private helper and call it from `launchGame`, right after the
existing `isRetroArchInstalled()` check and before `corePath` is used
(the two checks above it already guarantee RetroArch is present, so this
is the right point to prepare its input config before actually starting
it):

```cpp
// core/emulators/EmulatorProvider.h (addition)
private:
    void ensureGamepadAutoconfig();
```

```cpp
// core/emulators/EmulatorProvider.cpp (addition)
#include "RetroArchAutoconfig.h"
#include <SDL.h>

void EmulatorProvider::ensureGamepadAutoconfig() {
    const int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; ++i) {
        if (!SDL_IsGameController(i)) continue;

        const QString name = QString::fromUtf8(SDL_GameControllerNameForIndex(i));
        const QString mapping = QString::fromUtf8(SDL_GameControllerMappingForDeviceIndex(i));
        if (name.isEmpty() || mapping.isEmpty()) continue;

        // Sanitize the controller name into a safe filename (RetroArch
        // matches autoconfig files by their contents, not their filename,
        // but the filename still needs to be filesystem-safe).
        QString safeName = name;
        safeName.replace(QRegularExpression("[^A-Za-z0-9 _-]"), "_");
        const QString path = retroArchDir() + "/autoconfig/" + safeName + ".cfg";
        if (QFile::exists(path)) continue;

        QDir().mkpath(retroArchDir() + "/autoconfig");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(RetroArchAutoconfig::buildProfile(name, mapping).toUtf8());
        }
    }
}
```

Insert `ensureGamepadAutoconfig();` into `launchGame` (Task 6's
implementation) immediately after the `if (!isRetroArchInstalled()) { ... }`
block and before `const QString corePath = ...`:

```cpp
// core/emulators/EmulatorProvider.cpp — launchGame, insert after the
// isRetroArchInstalled() check from Task 6, before "const QString corePath ="
ensureGamepadAutoconfig();
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add core/emulators/RetroArchAutoconfig.h core/emulators/RetroArchAutoconfig.cpp core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp core/CMakeLists.txt tests/emulators/RetroArchAutoconfigTest.cpp tests/emulators/CMakeLists.txt
git commit -m "feat: generate a default RetroArch gamepad autoconfig profile"
```

---

## Task 8: Wire `EmulatorProvider` into `main.cpp`; rebuild `EmulatorManagerScreen.qml`

**Files:**
- Modify: `app/main.cpp`
- Modify: `ui/screens/EmulatorManagerScreen.qml`

**Interfaces:**
- Consumes: `EmulatorProvider`'s full public surface (Tasks 1, 3-7), `RomScanner::knownSystems()` (Task 1), `EmulatorCatalog`/`EmulatorCatalogData` (Task 2).
- Produces: nothing new for later tasks — `EmulatorManagerScreen` is a leaf UI screen. Task 9/10 depend on `EmulatorProvider` already being the `"EmulatorProvider"` QML context property this task establishes.

- [ ] **Step 1: Wire `main.cpp`**

```cpp
// app/main.cpp
// Remove: #include "emulators/StubEmulatorProvider.h" and
//         #include "emulators/EmulatorProviderQmlBridge.h"
// Add:
#include "emulators/EmulatorProvider.h"
#include "emulators/EmulatorCatalog.h"

// Remove the existing StubEmulatorProvider/EmulatorProviderQmlBridge block:
//   StubEmulatorProvider emulatorProvider;
//   EmulatorProviderQmlBridge emulatorBridge(&emulatorProvider);
//   engine.rootContext()->setContextProperty("EmulatorProvider", &emulatorBridge);
// Replace with (placed after libraryScanner's declaration, since
// EmulatorProvider also outlives no one it doesn't own — it takes
// networkManager by pointer, which is declared earlier in main() and
// outlives it just fine as a sibling, not a nested dependency):
EmulatorProvider emulatorProvider(dataDir, &networkManager);
engine.rootContext()->setContextProperty("EmulatorProvider", &emulatorProvider);

EmulatorCatalog emulatorCatalog(&networkManager);
QObject::connect(&emulatorCatalog, &EmulatorCatalog::ready,
                  [&emulatorProvider](const EmulatorCatalogData &data) {
    emulatorProvider.setCatalogData(data); // small setter to add alongside m_catalogData in EmulatorProvider.h/.cpp
});
emulatorCatalog.fetch(QUrl("https://raw.githubusercontent.com/baiddd/bili/master/catalog/emulators.json"));
```

Add the small `setCatalogData(const EmulatorCatalogData &data) { m_catalogData = data; }`
setter to `EmulatorProvider` (public method) as part of this step, since
no earlier task needed `EmulatorProvider` and `EmulatorCatalog` wired
together outside of tests.

- [ ] **Step 2: Rebuild `EmulatorManagerScreen.qml`**

```qml
// ui/screens/EmulatorManagerScreen.qml
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Component.onCompleted: refreshList()

    function refreshList() {
        var rows = [{ rowId: "retroarch", label: "RetroArch" }]
        var knownSystems = EmulatorProvider.knownSystems()
        for (var i = 0; i < knownSystems.length; i++) {
            rows.push({ rowId: "core:" + knownSystems[i], label: "Core " + knownSystems[i] })
        }
        listModel.clear()
        for (var j = 0; j < rows.length; j++) {
            listModel.append({
                rowId: rows[j].rowId,
                label: rows[j].label,
                progressFraction: 0,
                progressVisible: false
            })
        }
    }

    // Finds the row matching a "retroarch"/"core:<system>" target tag
    // (the same tags EmulatorProvider's install/uninstall signals use) and
    // returns its index, or -1 if not found — used by the Connections
    // handlers below to update that one row's own progress state.
    function rowIndexForTarget(target) {
        for (var i = 0; i < listModel.count; i++) {
            if (listModel.get(i).rowId === target) return i
        }
        return -1
    }

    ListModel { id: listModel }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingUnit * 2
        spacing: Theme.spacingUnit

        Text { text: "Émulateurs"; color: Theme.colorText; font.pixelSize: Theme.fontSizeTitle }

        ListView {
            id: emulatorList
            width: parent.width
            height: parent.height - Theme.spacingUnit * 6
            focus: true
            keyNavigationEnabled: true
            highlightFollowsCurrentItem: true
            highlight: Rectangle { color: Theme.focusBorderColor; opacity: 0.5; border.color: Theme.focusBorderColor; border.width: Theme.focusBorderWidth }
            Component.onCompleted: forceActiveFocus()
            model: listModel
            delegate: Row {
                spacing: Theme.spacingUnit
                property bool installed: model.rowId === "retroarch" ? EmulatorProvider.isRetroArchInstalled() : EmulatorProvider.isCoreInstalled(model.rowId.substring(5))
                Text {
                    text: model.label + (installed ? " (installé)" : "")
                    color: Theme.colorText
                    font.pixelSize: Theme.fontSizeBody
                }
                ProgressBar {
                    width: 150
                    visible: model.progressVisible
                    from: 0; to: 1
                    value: model.progressFraction
                }
                Button {
                    text: installed ? "Désinstaller" : "Installer"
                    onClicked: {
                        if (model.rowId === "retroarch") {
                            installed ? EmulatorProvider.uninstallRetroArch() : EmulatorProvider.installRetroArch()
                        } else {
                            var system = model.rowId.substring(5)
                            installed ? EmulatorProvider.uninstallCore(system) : EmulatorProvider.installCore(system)
                        }
                    }
                }
            }
        }

        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
    }

    Connections {
        target: EmulatorProvider
        function onInstallProgress(target, bytesReceived, bytesTotal) {
            var index = rowIndexForTarget(target)
            if (index === -1) return
            listModel.setProperty(index, "progressVisible", true)
            listModel.setProperty(index, "progressFraction", bytesReceived / Math.max(bytesTotal, 1))
        }
        function onInstallFinished(target) {
            statusText.text = target + " installé."
            refreshList()
        }
        function onInstallFailed(target, errorString) {
            var index = rowIndexForTarget(target)
            if (index !== -1) listModel.setProperty(index, "progressVisible", false)
            statusText.text = "Échec (" + target + ") : " + errorString
        }
        function onUninstallFinished(target) {
            statusText.text = target + " désinstallé."
            refreshList()
        }
        function onUninstallFailed(target, errorString) {
            statusText.text = "Échec (" + target + ") : " + errorString
        }
    }
}
```

`refreshList()` calls `EmulatorProvider.knownSystems()` rather than the
C++-only `RomScanner::knownSystems()` directly, because `RomScanner` is a
plain static-method class (not a `QObject`) and isn't itself exposed to
QML. Add this small forwarding method to `EmulatorProvider` as part of
this task:

```cpp
// core/emulators/EmulatorProvider.h (addition)
Q_INVOKABLE QStringList knownSystems() const;
```

```cpp
// core/emulators/EmulatorProvider.cpp (addition)
#include "library/RomScanner.h"

QStringList EmulatorProvider::knownSystems() const {
    return RomScanner::knownSystems();
}
```

Each `ListModel` row carries its own `progressFraction`/`progressVisible`
properties (initialized in `refreshList()`), so the `onInstallProgress`
handler updates only the one row matching the event's `target` tag via
`rowIndexForTarget()` — every other row's `ProgressBar` stays untouched,
giving each install its own independently-filling bar rather than a
single shared status line.

- [ ] **Step 3: Build and manually verify**

Run: `cmake --build build\windows-portable`, then launch `Bili.exe` (dot-source
`dev-env.ps1` first), navigate to `EmulatorManagerScreen`. Confirm the
list shows RetroArch + one row per known system, click "Installer" on a
core, confirm a progress indicator appears and the row updates to
"installé"/"Désinstaller" once done. Repeat for RetroArch itself (larger
download, more time to observe progress). Click "Désinstaller" on
something just installed, confirm it reverts.

- [ ] **Step 4: Commit**

```bash
git add app/main.cpp ui/screens/EmulatorManagerScreen.qml core/emulators/EmulatorProvider.h core/emulators/EmulatorProvider.cpp
git commit -m "feat: wire EmulatorProvider into the app; rebuild EmulatorManagerScreen as a real catalog list"
```

---

## Task 9: `GameDetailsScreen.qml` — real content, launch/install-suggestion

**Files:**
- Modify: `core/ui/ScreenManager.h`
- Modify: `core/ui/ScreenManager.cpp`
- Modify: `ui/screens/GameDetailsScreen.qml`
- Modify: `ui/screens/GameListScreen.qml`
- Modify: `ui/Main.qml`
- Test: `tests/ui/ScreenManagerTest.cpp` (append)

**Interfaces:**
- Consumes: `LibraryModel`'s `TitleRole`/`SystemRole`/`RomPathRole` (sub-project 2), `EmulatorProvider.isCoreInstalled`/`launchGame` (Tasks 1, 6), `ScreenManager` (sub-project 1).
- Produces: nothing new for later tasks — leaf UI screen.

**Context:** `ScreenManager.push("GameDetails")` currently gets called
from `Main.qml`'s `onAccept()` (added in sub-project 2's final fix wave)
with no data about *which* game was selected. `GameDetailsScreen` needs
the selected game's `romPath`/`system`/`title`. `ScreenManager`
(`core/ui/ScreenManager.h`, reproduced in full below for reference) is a
stack-based singleton with no existing mechanism for passing a payload
alongside a push, and `GameListScreen.qml`'s `GridView` delegate
(`ui/screens/GameListScreen.qml`, also reproduced below) doesn't expose
its model roles as properties on the delegate root — only inline, inside
its own child `Text { text: model.title }` binding — so there's currently
no way for `Main.qml` to read "the currently-focused game's data" from
outside the delegate. Both gaps are fixed together in this task.

Current `core/ui/ScreenManager.h`:
```cpp
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class ScreenManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentScreen READ currentScreen NOTIFY currentScreenChanged)
public:
    explicit ScreenManager(QObject *parent = nullptr);
    Q_INVOKABLE void push(const QString &screenName);
    Q_INVOKABLE void pop();
    QString currentScreen() const;

signals:
    void currentScreenChanged();

private:
    QStringList m_stack{"Boot"};
};
```

- [ ] **Step 1: Write the failing test**

```cpp
// tests/ui/ScreenManagerTest.cpp (append)
void selectedGamePropertiesRoundTripAndEmitOneSignal() {
    ScreenManager mgr;
    QSignalSpy spy(&mgr, &ScreenManager::selectedGameChanged);

    mgr.setSelectedGameRomPath("C:/roms/Zelda.nes");
    mgr.setSelectedGameSystem("nes");
    mgr.setSelectedGameTitle("Zelda");

    QCOMPARE(mgr.selectedGameRomPath(), QString("C:/roms/Zelda.nes"));
    QCOMPARE(mgr.selectedGameSystem(), QString("nes"));
    QCOMPARE(mgr.selectedGameTitle(), QString("Zelda"));
    QCOMPARE(spy.count(), 3);

    // Setting the same value again must not re-emit (standard Qt
    // property-setter hygiene: avoids redundant QML re-bindings).
    mgr.setSelectedGameTitle("Zelda");
    QCOMPARE(spy.count(), 3);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R ScreenManagerTest`
Expected: FAIL (compile error — the new properties/methods don't exist yet).

- [ ] **Step 3: Add a selected-game carrier to `ScreenManager`**

```cpp
// core/ui/ScreenManager.h — add alongside the existing currentScreen property
Q_PROPERTY(QString selectedGameRomPath READ selectedGameRomPath WRITE setSelectedGameRomPath NOTIFY selectedGameChanged)
Q_PROPERTY(QString selectedGameSystem READ selectedGameSystem WRITE setSelectedGameSystem NOTIFY selectedGameChanged)
Q_PROPERTY(QString selectedGameTitle READ selectedGameTitle WRITE setSelectedGameTitle NOTIFY selectedGameChanged)
public:
    // ... existing members unchanged ...
    QString selectedGameRomPath() const;
    void setSelectedGameRomPath(const QString &romPath);
    QString selectedGameSystem() const;
    void setSelectedGameSystem(const QString &system);
    QString selectedGameTitle() const;
    void setSelectedGameTitle(const QString &title);

signals:
    void currentScreenChanged();
    void selectedGameChanged();

private:
    QStringList m_stack{"Boot"};
    QString m_selectedGameRomPath;
    QString m_selectedGameSystem;
    QString m_selectedGameTitle;
```

```cpp
// core/ui/ScreenManager.cpp (additions)
QString ScreenManager::selectedGameRomPath() const { return m_selectedGameRomPath; }
void ScreenManager::setSelectedGameRomPath(const QString &romPath) {
    if (m_selectedGameRomPath == romPath) return;
    m_selectedGameRomPath = romPath;
    emit selectedGameChanged();
}
QString ScreenManager::selectedGameSystem() const { return m_selectedGameSystem; }
void ScreenManager::setSelectedGameSystem(const QString &system) {
    if (m_selectedGameSystem == system) return;
    m_selectedGameSystem = system;
    emit selectedGameChanged();
}
QString ScreenManager::selectedGameTitle() const { return m_selectedGameTitle; }
void ScreenManager::setSelectedGameTitle(const QString &title) {
    if (m_selectedGameTitle == title) return;
    m_selectedGameTitle = title;
    emit selectedGameChanged();
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R ScreenManagerTest`
Expected: PASS.

- [ ] **Step 5: Expose the delegate's model data so it's readable from outside**

Current `ui/screens/GameListScreen.qml`'s delegate only uses `model.title`
inline in a child `Text`; add explicit properties on the delegate root so
`Main.qml` can read them via `GridView.currentItem`:

```qml
// ui/screens/GameListScreen.qml — inside the existing `delegate: Rectangle { id: gameDelegate ... }` block, add:
property string romPath: model.romPath
property string system: model.system
property string gameTitle: model.title
```

- [ ] **Step 6: Set it from `Main.qml`'s `onAccept()`**

```qml
// ui/Main.qml — inside the existing onAccept() GridView branch (added in
// sub-project 2's final fix wave), before ScreenManager.push("GameDetails"):
ScreenManager.selectedGameRomPath = root.activeFocusItem.currentItem.romPath
ScreenManager.selectedGameSystem = root.activeFocusItem.currentItem.system
ScreenManager.selectedGameTitle = root.activeFocusItem.currentItem.gameTitle
```

(`root.activeFocusItem` at that point is the `GridView` itself — `GridView`
keeps `activeFocus` on the view, never on individual delegates, per Task
6/sub-project 2's own established finding. `currentItem` is `GridView`'s
own built-in property giving the currently-instantiated delegate for
`currentIndex`, which now exposes `romPath`/`system`/`gameTitle` thanks to
Step 5 above.)

- [ ] **Step 7: Rebuild `GameDetailsScreen.qml`**

```qml
// ui/screens/GameDetailsScreen.qml
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    property bool coreInstalled: EmulatorProvider.isCoreInstalled(ScreenManager.selectedGameSystem)

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit * 2

        Text {
            text: ScreenManager.selectedGameTitle
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }
        Text {
            text: "Système : " + ScreenManager.selectedGameSystem
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
        }

        Button {
            id: actionButton
            text: coreInstalled ? "Lancer" : "Installer un core pour ce système"
            focus: true
            Component.onCompleted: forceActiveFocus()
            background: Rectangle {
                color: actionButton.activeFocus ? Theme.focusBorderColor : "#22222a"
                border.color: Theme.focusBorderColor
                border.width: actionButton.activeFocus ? Theme.focusBorderWidth : 1
                radius: Theme.focusRadius
            }
            onClicked: {
                if (coreInstalled) {
                    EmulatorProvider.launchGame(ScreenManager.selectedGameRomPath, ScreenManager.selectedGameSystem)
                } else {
                    ScreenManager.push("EmulatorManager")
                }
            }
        }

        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
    }

    Connections {
        target: EmulatorProvider
        function onGameLaunched() { statusText.text = "En cours de jeu..." }
        function onGameExited(exitCode) { statusText.text = "" }
        function onLaunchFailed(errorString) { statusText.text = "Erreur : " + errorString }
    }
}
```

- [ ] **Step 8: Build and manually verify**

Launch, navigate to a game with an installed core, confirm "Lancer"
appears and actually starts RetroArch with that game; navigate to a game
without one, confirm "Installer un core pour ce système" appears and
pushes to `EmulatorManagerScreen`.

- [ ] **Step 9: Commit**

```bash
git add core/ui/ScreenManager.h core/ui/ScreenManager.cpp ui/Main.qml ui/screens/GameListScreen.qml ui/screens/GameDetailsScreen.qml tests/ui/ScreenManagerTest.cpp
git commit -m "feat: show real game details with launch/install-suggestion"
```

---

## Task 10: Direct launch from `GameListScreen`

**Files:**
- Modify: `ui/Main.qml`

**Interfaces:**
- Consumes: `EmulatorProvider.isCoreInstalled`/`launchGame` (Task 6), the same `onAccept()` GridView branch Task 9 already extended.

- [ ] **Step 1: Extend `onAccept()`'s GameList branch**

```qml
// ui/Main.qml — replace the GameList-specific branch inside onAccept()
// (the one added in sub-project 2's final fix wave, then extended by
// Task 9 above) with:
else if (typeof root.activeFocusItem.moveCurrentIndexUp === "function" && ScreenManager.currentScreen === "GameList") {
    var romPath = root.activeFocusItem.currentItem.romPath
    var system = root.activeFocusItem.currentItem.system
    var title = root.activeFocusItem.currentItem.gameTitle

    if (EmulatorProvider.isCoreInstalled(system)) {
        ScreenManager.selectedGameRomPath = romPath
        ScreenManager.selectedGameSystem = system
        ScreenManager.selectedGameTitle = title
        EmulatorProvider.launchGame(romPath, system)
    } else {
        ScreenManager.selectedGameRomPath = romPath
        ScreenManager.selectedGameSystem = system
        ScreenManager.selectedGameTitle = title
        ScreenManager.push("GameDetails")
    }
}
```

(Both branches set the same three `ScreenManager.selectedGame*`
properties before acting — a direct launch still wants them set, in case
`gameExited`/`launchFailed` feedback needs to be shown somewhere that
reads them, and so `GameDetailsScreen` shows correct data if the user
backs out to it later in the same session.)

- [ ] **Step 2: Build and manually verify**

With a core already installed for some system, focus a game of that
system on `GameListScreen` and press Enter/A: confirm RetroArch launches
directly, without a `GameDetails` detour. With no core installed for a
different game's system, confirm Accept still opens `GameDetails` as
before.

- [ ] **Step 3: Commit**

```bash
git add ui/Main.qml
git commit -m "feat: launch a game directly from GameListScreen when a core is already installed"
```

---

## Vérification (end-to-end, for the final whole-branch review)

- Fresh install (no RetroArch, no cores): `EmulatorManagerScreen` shows
  everything as not-installed; installing a core and RetroArch both work,
  with visible progress, and both end up marked installed.
- A ROM whose system has an installed core launches directly from
  `GameListScreen` on Accept; a ROM whose system has no core opens
  `GameDetailsScreen` with an "Installer un core" prompt instead.
- A ROM inside a `.zip` archive (`rom_path` using the `"<archive>::<entry>"`
  convention) launches correctly through whichever mechanism Task 6's
  research determined.
- Uninstalling a core/RetroArch removes the files and the app correctly
  reports it as not-installed afterward.
- Disconnecting network access before opening `EmulatorManagerScreen`:
  clear error message, no crash, rest of the app (including the offline
  ROM library) keeps working.
- A connected gamepad gets a working RetroArch autoconfig profile on
  first launch, usable without manual configuration; the user can still
  remap individual buttons afterward via RetroArch's own Input settings.
- All `ctest` suites pass, including every new test target this plan
  adds (`EmulatorProviderTest`, `EmulatorCatalogTest`,
  `RetroArchAutoconfigTest`).
- `docs/index.md` documents `7za.exe`'s source/license and the RetroArch/
  core catalog's version choice, alongside the existing `miniz` entry.
