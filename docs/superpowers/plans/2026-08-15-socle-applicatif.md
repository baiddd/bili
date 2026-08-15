# Socle Applicatif Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the cross-platform application shell (Qt6/QML + SDL2 gamepad
bridge, screen navigation, config/library storage, network layer, and stub
extension points) that every later sub-project (library management, emulator
management, scraping, final UI/UX, netplay, packaging) builds on top of.

**Architecture:** A CMake-built Qt6 application with a pure-C++ `/core`
(storage, network, input bridge, stub provider interfaces) exposed to a QML
`/ui` layer via singletons and context properties. Gamepad input comes from
SDL2 running in a dedicated thread that never touches video/rendering — Qt
owns the window exclusively. Everything lives in a self-contained, portable
`<app>/data/` folder; no OS-level install step, no registry/QSettings use.

**Tech Stack:** C++17, Qt 6 (Core, Gui, Qml, Quick, Sql, Network), SDL2
(gamepad/joystick subsystem only), CMake 3.21+, Qt Test, CTest.

**Spec:** `C:\Users\Steam\.claude\plans\tranquil-weaving-karp.md` (approved
plan for sub-project 1, "socle applicatif" — this plan implements it in full).

## Global Constraints

- Portable app: no installer, no admin rights, no registry writes, no
  `QSettings` default backend — all persistent state lives under
  `<app_root>/data/`.
- `emulators`, `scraper`, `netplay` modules expose interfaces + stubs only in
  this plan — real implementations are out of scope (sub-projects 3/4/6).
- Every screen must be reachable and navigable (keyboard, mouse, gamepad)
  even though most screens are visual placeholders until sub-project 5.
- SDL2 must be initialized with `SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK`
  only — never `SDL_INIT_VIDEO`.
- No network calls are placeholders except netplay (P2P) — emulator/scraper
  downloads use the real `NetworkManager` against real HTTP endpoints (test
  endpoints acceptable for this plan's automated tests).
- Toolchain is MinGW-w64/Qt-mingw on `D:\Qt`, not MSVC (see Task 1 Step 2 for
  why). Every `cmake`/`ctest`/`ninja` command in every task's Steps assumes
  `platform/windows/dev-env.ps1` (created in Task 1) has been dot-sourced
  first in the current PowerShell session: `. .\platform\windows\dev-env.ps1`.
  Do this once per new terminal session before running any Run: command in
  this plan.

---

## Task 1: Project scaffolding — CMake + blank Qt6 window boots

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `app/main.cpp`
- Create: `app/CMakeLists.txt`
- Create: `ui/Main.qml`
- Create: `ui/qml.qrc` (or `qt_add_qml_module` resource, see step 3)
- Create: `platform/windows/dev-env.ps1`

**Interfaces:**
- Produces: an executable target `bili-frontend` that opens a window titled
  "Bili" and exits cleanly on close. Later tasks add C++ singletons to this
  same target via `qt_add_qml_module(... SOURCES ...)`.

- [ ] **Step 1: Write root `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.21)
project(BiliFrontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Sql Network Test)

enable_testing()

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(tests)
```

- [ ] **Step 2: Write `CMakePresets.json` with a `windows-portable` preset**

This machine's toolchain was installed to `D:\Qt` (verified working: CMake
3.30.5, Ninja 1.12.1, MinGW-w64 GCC 13.1.0, Qt 6.8.3 mingw_64 — chosen over
MSVC because the C: system drive had only ~10GB free and MSVC's installer
cannot be fully redirected off C:, while this whole toolchain lives entirely
on D:). Hardcode these paths in the preset so the build works regardless of
PATH:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "windows-portable",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/windows-portable",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_PREFIX_PATH": "D:/Qt/6.8.3/mingw_64",
        "CMAKE_C_COMPILER": "D:/Qt/Tools/mingw1310_64/bin/gcc.exe",
        "CMAKE_CXX_COMPILER": "D:/Qt/Tools/mingw1310_64/bin/g++.exe",
        "CMAKE_MAKE_PROGRAM": "D:/Qt/Tools/Ninja/ninja.exe"
      }
    }
  ]
}
```

`cmake` itself is not on PATH either — invoke it as
`D:\Qt\Tools\CMake_64\bin\cmake.exe` (or add
`D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\mingw1310_64\bin` to
PATH for the session first, e.g. via
`$env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\mingw1310_64\bin;$env:PATH"`
in PowerShell). At runtime, the built `.exe` also needs the MinGW runtime
DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) —
either keep `D:\Qt\Tools\mingw1310_64\bin` on PATH while testing locally, or
copy those three DLLs next to the exe (Task 13's `windeployqt` packaging
step handles this for the distributed portable build; for local dev/test
runs in this task, PATH is simpler).

- [ ] **Step 3: Write `platform/windows/dev-env.ps1`**

```powershell
# platform/windows/dev-env.ps1
# Dot-source this once per new PowerShell session before running any
# cmake/ctest/ninja command in this repo: . .\platform\windows\dev-env.ps1
$env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\mingw1310_64\bin;$env:PATH"
Write-Host "Dev environment ready: cmake $(cmake --version | Select-Object -First 1), ninja $(ninja --version), mingw g++ $(g++ --version | Select-Object -First 1)"
```

Run: `. .\platform\windows\dev-env.ps1`
Expected: prints the three tool versions (cmake 3.30.5, ninja 1.12.1, g++ 13.1.0) without error.

- [ ] **Step 4: Write `app/CMakeLists.txt` with a QML module**

`WIN32` on `qt_add_executable` marks the target as a GUI subsystem
executable on Windows (no-op on other platforms) — without it, the built
`.exe` opens a console window behind the Qt window, which is wrong for a
portable, double-click-to-run app.

```cmake
qt_add_executable(bili-frontend WIN32 main.cpp)

qt_add_qml_module(bili-frontend
    URI Bili
    VERSION 1.0
    QML_FILES ../ui/Main.qml
)

target_link_libraries(bili-frontend PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    bili-core
)
```

- [ ] **Step 5: Write `app/main.cpp`**

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Bili", "Main");

    return app.exec();
}
```

- [ ] **Step 6: Write `ui/Main.qml`**

```qml
import QtQuick
import QtQuick.Window

ApplicationWindow {
    visible: true
    width: 1280
    height: 720
    title: "Bili"
}
```

(Note: `ApplicationWindow` requires `import QtQuick.Controls` — add that
import; this file is superseded by `ScreenManager`-driven navigation in
Task 5, so keep it minimal here.)

- [ ] **Step 7: Create placeholder `core/CMakeLists.txt` and `tests/CMakeLists.txt`**

```cmake
# core/CMakeLists.txt
add_library(bili-core STATIC)
target_link_libraries(bili-core PUBLIC Qt6::Core)
target_include_directories(bili-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

```cmake
# tests/CMakeLists.txt
# Individual test targets are added by later tasks via add_test().
```

- [ ] **Step 8: Configure and build**

Run: `cmake --preset windows-portable && cmake --build build/windows-portable`
Expected: build succeeds, `bili-frontend.exe` produced.

- [ ] **Step 9: Run the executable**

Run: `./build/windows-portable/app/bili-frontend.exe`
Expected: a window titled "Bili" opens at 1280x720 and closes cleanly on
window-close. If it fails to launch with a missing-DLL error, put
`D:\Qt\Tools\mingw1310_64\bin` on PATH (already done if `dev-env.ps1` is
still dot-sourced in this session) — see Step 3's note on MinGW runtime DLLs.

- [ ] **Step 10: Commit**

Git is already initialized at the repo root (done by the controller before
this task started; do not run `git init` again).

```bash
git add CMakeLists.txt CMakePresets.json app core tests ui platform
git commit -m "chore: scaffold CMake + Qt6 project, blank window boots"
```

---

## Task 2: Portable config storage (JSON)

**Files:**
- Create: `core/storage/ConfigStore.h`
- Create: `core/storage/ConfigStore.cpp`
- Test: `tests/storage/ConfigStoreTest.cpp`

**Interfaces:**
- Consumes: nothing (foundational).
- Produces:
  ```cpp
  class ConfigStore : public QObject {
      Q_OBJECT
  public:
      explicit ConfigStore(QString dataDir, QObject *parent = nullptr);
      QJsonObject data() const;
      void setData(const QJsonObject &obj);
      bool save() const;   // writes data() to <dataDir>/config.json
      bool load();          // reads <dataDir>/config.json into data()
  };
  ```
  Later tasks (romSources, FirstLaunchSetup, Settings) read/write through
  `data()`/`setData()`/`save()`.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/storage/ConfigStoreTest.cpp
#include <QtTest>
#include <QTemporaryDir>
#include "storage/ConfigStore.h"

class ConfigStoreTest : public QObject {
    Q_OBJECT
private slots:
    void savesAndReloadsJson() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        ConfigStore store(dir.path());
        QJsonObject obj;
        obj["theme"] = "dark";
        store.setData(obj);
        QVERIFY(store.save());

        ConfigStore reloaded(dir.path());
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.data()["theme"].toString(), QString("dark"));
    }

    void loadOnMissingFileReturnsFalseWithoutCrashing() {
        QTemporaryDir dir;
        ConfigStore store(dir.path());
        QVERIFY(!store.load());
        QVERIFY(store.data().isEmpty());
    }
};

QTEST_MAIN(ConfigStoreTest)
#include "ConfigStoreTest.moc"
```

- [ ] **Step 2: Wire the test target and run to verify it fails**

```cmake
# tests/storage/CMakeLists.txt
qt_add_executable(ConfigStoreTest ConfigStoreTest.cpp)
target_link_libraries(ConfigStoreTest PRIVATE Qt6::Test bili-core)
add_test(NAME ConfigStoreTest COMMAND ConfigStoreTest)
```
Add `add_subdirectory(storage)` to `tests/CMakeLists.txt`.

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R ConfigStoreTest`
Expected: FAIL (compile error — `ConfigStore` does not exist yet).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/storage/ConfigStore.h
#pragma once
#include <QObject>
#include <QJsonObject>
#include <QString>

class ConfigStore : public QObject {
    Q_OBJECT
public:
    explicit ConfigStore(QString dataDir, QObject *parent = nullptr);
    QJsonObject data() const { return m_data; }
    void setData(const QJsonObject &obj) { m_data = obj; }
    bool save() const;
    bool load();

private:
    QString m_dataDir;
    QJsonObject m_data;
};
```

```cpp
// core/storage/ConfigStore.cpp
#include "ConfigStore.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>

ConfigStore::ConfigStore(QString dataDir, QObject *parent)
    : QObject(parent), m_dataDir(std::move(dataDir)) {}

bool ConfigStore::save() const {
    QDir().mkpath(m_dataDir);
    QFile file(m_dataDir + "/config.json");
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(m_data).toJson());
    return true;
}

bool ConfigStore::load() {
    QFile file(m_dataDir + "/config.json");
    if (!file.open(QIODevice::ReadOnly)) return false;
    m_data = QJsonDocument::fromJson(file.readAll()).object();
    return true;
}
```

Add `core/storage/ConfigStore.cpp` to `bili-core`'s sources in
`core/CMakeLists.txt` via `target_sources(bili-core PRIVATE storage/ConfigStore.cpp)`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R ConfigStoreTest`
Expected: PASS (2/2 tests).

- [ ] **Step 5: Commit**

```bash
git add core/storage tests/storage tests/CMakeLists.txt
git commit -m "feat: add portable JSON ConfigStore"
```

---

## Task 3: SQLite library index (schema + open)

**Files:**
- Create: `core/storage/LibraryDatabase.h`
- Create: `core/storage/LibraryDatabase.cpp`
- Test: `tests/storage/LibraryDatabaseTest.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  ```cpp
  class LibraryDatabase {
  public:
      explicit LibraryDatabase(QString dbPath);
      bool open();   // creates schema if missing
      bool isOpen() const;
      int gameCount() const;
      qint64 insertGame(const QString &romPath, const QString &system,
                         const QString &title);
  };
  ```
  Sub-project 2 (bibliothèque locale) extends this with the real scan/
  organize logic; this task only guarantees the schema and basic CRUD exist.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/storage/LibraryDatabaseTest.cpp
#include <QtTest>
#include <QTemporaryDir>
#include "storage/LibraryDatabase.h"

class LibraryDatabaseTest : public QObject {
    Q_OBJECT
private slots:
    void createsSchemaAndInsertsGame() {
        QTemporaryDir dir;
        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());
        QCOMPARE(db.gameCount(), 0);

        qint64 id = db.insertGame("/roms/nes/mario.nes", "nes", "Super Mario Bros.");
        QVERIFY(id > 0);
        QCOMPARE(db.gameCount(), 1);
    }
};

QTEST_MAIN(LibraryDatabaseTest)
#include "LibraryDatabaseTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/storage/CMakeLists.txt (append)
qt_add_executable(LibraryDatabaseTest LibraryDatabaseTest.cpp)
target_link_libraries(LibraryDatabaseTest PRIVATE Qt6::Test Qt6::Sql bili-core)
add_test(NAME LibraryDatabaseTest COMMAND LibraryDatabaseTest)
```

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R LibraryDatabaseTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/storage/LibraryDatabase.h
#pragma once
#include <QString>
#include <QSqlDatabase>

class LibraryDatabase {
public:
    explicit LibraryDatabase(QString dbPath);
    bool open();
    bool isOpen() const { return m_db.isOpen(); }
    int gameCount() const;
    qint64 insertGame(const QString &romPath, const QString &system,
                       const QString &title);

private:
    QString m_dbPath;
    QSqlDatabase m_db;
};
```

```cpp
// core/storage/LibraryDatabase.cpp
#include "LibraryDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>

LibraryDatabase::LibraryDatabase(QString dbPath) : m_dbPath(std::move(dbPath)) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    m_db.setDatabaseName(m_dbPath);
}

bool LibraryDatabase::open() {
    if (!m_db.open()) return false;
    QSqlQuery q(m_db);
    return q.exec(
        "CREATE TABLE IF NOT EXISTS games ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  rom_path TEXT NOT NULL UNIQUE,"
        "  system TEXT NOT NULL,"
        "  title TEXT NOT NULL,"
        "  boxart_path TEXT"
        ")");
}

int LibraryDatabase::gameCount() const {
    QSqlQuery q("SELECT COUNT(*) FROM games", m_db);
    q.next();
    return q.value(0).toInt();
}

qint64 LibraryDatabase::insertGame(const QString &romPath, const QString &system,
                                    const QString &title) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO games (rom_path, system, title) VALUES (?, ?, ?)");
    q.addBindValue(romPath);
    q.addBindValue(system);
    q.addBindValue(title);
    if (!q.exec()) return -1;
    return q.lastInsertId().toLongLong();
}
```

Add `core/storage/LibraryDatabase.cpp` to `bili-core` sources; link
`Qt6::Sql` to `bili-core` in `core/CMakeLists.txt`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R LibraryDatabaseTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add core/storage tests/storage
git commit -m "feat: add SQLite LibraryDatabase with games schema"
```

---

## Task 4: NetworkManager (async download, progress/retry/cancel)

**Files:**
- Create: `core/network/NetworkManager.h`
- Create: `core/network/NetworkManager.cpp`
- Test: `tests/network/NetworkManagerTest.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  ```cpp
  class NetworkManager : public QObject {
      Q_OBJECT
  public:
      explicit NetworkManager(QObject *parent = nullptr);
      // returns a request id; emits progress/finished/failed with that id
      int startDownload(const QUrl &url, const QString &destPath);
      void cancelDownload(int requestId);
  signals:
      void progress(int requestId, qint64 bytesReceived, qint64 bytesTotal);
      void finished(int requestId, const QString &destPath);
      void failed(int requestId, const QString &errorString);
  };
  ```
  Sub-projects 3/4 (emulator/scraper) and Task 11 (stub wiring) call
  `startDownload`/`cancelDownload` and connect to these three signals.

- [ ] **Step 1: Write the failing test**

Uses a local `QTcpServer`-backed HTTP stub so the test has no external
network dependency.

```cpp
// tests/network/NetworkManagerTest.cpp
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QSignalSpy>
#include "network/NetworkManager.h"

class NetworkManagerTest : public QObject {
    Q_OBJECT
private slots:
    void downloadsFileAndEmitsFinished() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&server]() {
            QTcpSocket *client = server.nextPendingConnection();
            const QByteArray body = "hello-world";
            const QByteArray response = "HTTP/1.1 200 OK\r\n"
                "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            client->write(response);
            client->flush();
            connect(client, &QTcpSocket::bytesWritten, client, &QTcpSocket::deleteLater);
        });

        QTemporaryDir dir;
        const QString destPath = dir.path() + "/out.bin";
        NetworkManager manager;
        QSignalSpy finishedSpy(&manager, &NetworkManager::finished);

        const QUrl url(QString("http://127.0.0.1:%1/file").arg(server.serverPort()));
        manager.startDownload(url, destPath);

        QVERIFY(finishedSpy.wait(5000));
        QFile out(destPath);
        QVERIFY(out.open(QIODevice::ReadOnly));
        QCOMPARE(out.readAll(), QByteArray("hello-world"));
    }
};

QTEST_MAIN(NetworkManagerTest)
#include "NetworkManagerTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/network/CMakeLists.txt
qt_add_executable(NetworkManagerTest NetworkManagerTest.cpp)
target_link_libraries(NetworkManagerTest PRIVATE Qt6::Test Qt6::Network bili-core)
add_test(NAME NetworkManagerTest COMMAND NetworkManagerTest)
```
Add `add_subdirectory(network)` to `tests/CMakeLists.txt`.

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R NetworkManagerTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/network/NetworkManager.h
#pragma once
#include <QObject>
#include <QUrl>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    int startDownload(const QUrl &url, const QString &destPath);
    void cancelDownload(int requestId);

signals:
    void progress(int requestId, qint64 bytesReceived, qint64 bytesTotal);
    void finished(int requestId, const QString &destPath);
    void failed(int requestId, const QString &errorString);

private:
    QNetworkAccessManager m_nam;
    QMap<int, QNetworkReply *> m_active;
    int m_nextId = 1;
};
```

```cpp
// core/network/NetworkManager.cpp
#include "NetworkManager.h"

NetworkManager::NetworkManager(QObject *parent) : QObject(parent) {}

int NetworkManager::startDownload(const QUrl &url, const QString &destPath) {
    const int id = m_nextId++;
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    m_active[id] = reply;

    auto *file = new QFile(destPath, reply);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, id](qint64 received, qint64 total) {
                emit progress(id, received, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, id, reply, destPath, file]() {
        m_active.remove(id);
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(id, reply->errorString());
        } else {
            if (file->open(QIODevice::WriteOnly)) {
                file->write(reply->readAll());
                file->close();
                emit finished(id, destPath);
            } else {
                emit failed(id, "cannot open destination file");
            }
        }
        reply->deleteLater();
    });

    return id;
}

void NetworkManager::cancelDownload(int requestId) {
    if (auto *reply = m_active.value(requestId)) {
        reply->abort();
    }
}
```

Add `core/network/NetworkManager.cpp` to `bili-core` sources; link
`Qt6::Network` to `bili-core`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R NetworkManagerTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add core/network tests/network
git commit -m "feat: add NetworkManager with async download and progress signal"
```

---

## Task 5: ScreenManager + QML screen stack skeleton

**Files:**
- Create: `core/ui/ScreenManager.h`
- Create: `core/ui/ScreenManager.cpp`
- Create: `ui/screens/BootScreen.qml`
- Create: `ui/screens/MainMenuScreen.qml`
- Create: `ui/screens/GameListScreen.qml`
- Create: `ui/screens/GameDetailsScreen.qml`
- Create: `ui/screens/SettingsScreen.qml`
- Create: `ui/screens/DownloadManagerScreen.qml`
- Create: `ui/screens/ScraperManagerScreen.qml`
- Create: `ui/screens/EmulatorManagerScreen.qml`
- Create: `ui/screens/FirstLaunchSetupScreen.qml`
- Create: `ui/screens/AboutScreen.qml`
- Modify: `ui/Main.qml`
- Modify: `app/CMakeLists.txt`
- Test: `tests/ui/ScreenManagerTest.cpp`

**Interfaces:**
- Consumes: nothing new (this is the navigation backbone other screens plug into).
- Produces:
  ```cpp
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
  };
  ```
  Registered to QML as a singleton `ScreenManager`. `Main.qml` binds a
  `Loader`/`StackView` to `ScreenManager.currentScreen`. Screen names map
  1:1 to the QML files above (e.g. `"MainMenu"` → `MainMenuScreen.qml`).

- [ ] **Step 1: Write the failing test**

```cpp
// tests/ui/ScreenManagerTest.cpp
#include <QtTest>
#include "ui/ScreenManager.h"

class ScreenManagerTest : public QObject {
    Q_OBJECT
private slots:
    void startsOnBootAndNavigates() {
        ScreenManager mgr;
        QCOMPARE(mgr.currentScreen(), QString("Boot"));

        QSignalSpy spy(&mgr, &ScreenManager::currentScreenChanged);
        mgr.push("MainMenu");
        QCOMPARE(mgr.currentScreen(), QString("MainMenu"));
        QCOMPARE(spy.count(), 1);
    }

    void popReturnsToPreviousScreen() {
        ScreenManager mgr;
        mgr.push("MainMenu");
        mgr.push("Settings");
        mgr.pop();
        QCOMPARE(mgr.currentScreen(), QString("MainMenu"));
    }
};

QTEST_MAIN(ScreenManagerTest)
#include "ScreenManagerTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/ui/CMakeLists.txt
qt_add_executable(ScreenManagerTest ScreenManagerTest.cpp)
target_link_libraries(ScreenManagerTest PRIVATE Qt6::Test bili-core)
add_test(NAME ScreenManagerTest COMMAND ScreenManagerTest)
```
Add `add_subdirectory(ui)` to `tests/CMakeLists.txt`.

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R ScreenManagerTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/ui/ScreenManager.h
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

```cpp
// core/ui/ScreenManager.cpp
#include "ScreenManager.h"

ScreenManager::ScreenManager(QObject *parent) : QObject(parent) {}

QString ScreenManager::currentScreen() const { return m_stack.last(); }

void ScreenManager::push(const QString &screenName) {
    m_stack.append(screenName);
    emit currentScreenChanged();
}

void ScreenManager::pop() {
    if (m_stack.size() > 1) {
        m_stack.removeLast();
        emit currentScreenChanged();
    }
}
```

Add `core/ui/ScreenManager.cpp` to `bili-core` sources.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R ScreenManagerTest`
Expected: PASS.

- [ ] **Step 5: Register `ScreenManager` as a QML singleton and build the screen stack**

Task 1 discovered (see its report, and the `app/CMakeLists.txt` it
committed) that Qt 6.8.3's `qmlcachegen`/resource embedding cannot cleanly
handle `QML_FILES` entries that climb out of `app/`'s own source dir with
`..` on this Windows/MinGW/Ninja toolchain — it produces an invalid
`<target>_..` intermediate directory name at build time, and separately the
generated `qmldir` resolves the entry to the wrong resource path at
runtime. Task 1 fixed this for the single `../ui/Main.qml` entry with a
`QT_RESOURCE_ALIAS` + `NO_CACHEGEN`. This task adds ten more files the same
way (`../ui/screens/*.qml`), so replace `app/CMakeLists.txt`'s
`qt_add_qml_module` call entirely with this list-driven version, which
computes the alias for every entry automatically instead of repeating the
fix ten times — this is the version every later task that adds a QML file
(this one, and Task 6) should extend by appending to `BILI_QML_FILES`,
never by hand-writing a new `qt_add_qml_module` call:

```cmake
# app/CMakeLists.txt (replace the qt_add_qml_module call from Task 1 with this)
set(BILI_QML_FILES
    ../ui/Main.qml
    ../ui/screens/BootScreen.qml
    ../ui/screens/MainMenuScreen.qml
    ../ui/screens/GameListScreen.qml
    ../ui/screens/GameDetailsScreen.qml
    ../ui/screens/SettingsScreen.qml
    ../ui/screens/DownloadManagerScreen.qml
    ../ui/screens/ScraperManagerScreen.qml
    ../ui/screens/EmulatorManagerScreen.qml
    ../ui/screens/FirstLaunchSetupScreen.qml
    ../ui/screens/AboutScreen.qml
)

# Alias every file to its path relative to ui/ so it lands at a flat,
# predictable resource path (:/Bili/<alias>) instead of climbing out with
# `..` (see Task 1 report for the mkdir/qmldir failure this avoids).
foreach(qml_file ${BILI_QML_FILES})
    string(REGEX REPLACE "^\\.\\./ui/" "" qml_alias ${qml_file})
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()

qt_add_qml_module(bili-frontend
    URI Bili
    VERSION 1.0
    QML_FILES ${BILI_QML_FILES}
    NO_CACHEGEN
)
```

Register the C++ type in `app/main.cpp` before `loadFromModule`:

```cpp
#include "ui/ScreenManager.h"
#include <QQmlContext>
// ...
ScreenManager screenManager;
engine.rootContext()->setContextProperty("ScreenManager", &screenManager);
```

Each screen file (all identical skeleton except title, shown here for
`MainMenuScreen.qml` — repeat the pattern verbatim, substituting the title,
for `GameListScreen`, `GameDetailsScreen`, `SettingsScreen`,
`DownloadManagerScreen`, `ScraperManagerScreen`, `EmulatorManagerScreen`,
`FirstLaunchSetupScreen`, `AboutScreen`; `BootScreen.qml` additionally calls
`ScreenManager.push("MainMenu")` after a short `Timer`):

```qml
// ui/screens/MainMenuScreen.qml
import QtQuick

Rectangle {
    anchors.fill: parent
    color: "#101014"
    Text {
        anchors.centerIn: parent
        text: "MainMenu"
        color: "white"
        font.pixelSize: 32
    }
}
```

```qml
// ui/screens/BootScreen.qml
import QtQuick

Rectangle {
    anchors.fill: parent
    color: "black"
    Text {
        anchors.centerIn: parent
        text: "Bili"
        color: "white"
        font.pixelSize: 40
    }
    Timer {
        interval: 500
        running: true
        onTriggered: ScreenManager.push("MainMenu")
    }
}
```

```qml
// ui/Main.qml
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 1280
    height: 720
    title: "Bili"

    Loader {
        anchors.fill: parent
        source: "screens/" + ScreenManager.currentScreen + "Screen.qml"
    }
}
```

- [ ] **Step 6: Build and run manually**

Run: `cmake --build build/windows-portable && ./build/windows-portable/app/bili-frontend.exe`
Expected: window shows "Bili" boot text for ~0.5s, then transitions to a
screen showing "MainMenu" text.

- [ ] **Step 7: Commit**

```bash
git add core/ui ui/screens ui/Main.qml app/CMakeLists.txt app/main.cpp tests/ui
git commit -m "feat: add ScreenManager and placeholder screen stack"
```

---

## Task 6: Theme singleton (design tokens + breakpoints)

**Files:**
- Create: `ui/theme/Theme.qml`
- Modify: `app/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: a QML singleton `Theme` with properties `colorBackground`,
  `colorAccent`, `colorText`, `spacingUnit`, `fontSizeBody`,
  `fontSizeTitle`, `animationDurationMs`, `animationEasing`, and a function
  `isCompact(width)` returning `bool` for the responsive breakpoint. All
  screens built from Task 5 onward reference `Theme.*` instead of hardcoded
  values — sub-project 5 replaces the values (not the property names) with
  Figma-derived tokens.

- [ ] **Step 1: Write `ui/theme/Theme.qml`**

```qml
pragma Singleton
import QtQuick

QtObject {
    readonly property color colorBackground: "#101014"
    readonly property color colorAccent: "#4f8cff"
    readonly property color colorText: "#ffffff"

    readonly property int spacingUnit: 8
    readonly property int fontSizeBody: 16
    readonly property int fontSizeTitle: 32

    readonly property int animationDurationMs: 200
    readonly property int animationEasing: Easing.InOutQuad

    readonly property int compactBreakpointWidth: 900

    function isCompact(width) {
        return width < compactBreakpointWidth;
    }
}
```

- [ ] **Step 2: Register the singleton**

```cmake
# app/CMakeLists.txt (add to QML_FILES list)
../ui/theme/Theme.qml
```

Add `qt_target_qml_sources` isn't needed for a singleton beyond listing it
in `QML_FILES` with a `QML_SINGLETON` — use the file-level pragma (already
present as `pragma Singleton` above), which `qt_add_qml_module` picks up
automatically for `.qml` files starting with an uppercase letter.

- [ ] **Step 3: Use it in one screen to prove it resolves**

Edit `ui/screens/MainMenuScreen.qml`:

```qml
import QtQuick
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Text {
        anchors.centerIn: parent
        text: "MainMenu"
        color: Theme.colorText
        font.pixelSize: Theme.fontSizeTitle
    }
}
```

- [ ] **Step 4: Build and run manually**

Run: `cmake --build build/windows-portable && ./build/windows-portable/app/bili-frontend.exe`
Expected: identical visual result to Task 5 (colors now come from `Theme`
instead of literals) — confirms the singleton resolves without QML errors
in the console.

- [ ] **Step 5: Commit**

```bash
git add ui/theme ui/screens/MainMenuScreen.qml app/CMakeLists.txt
git commit -m "feat: add Theme singleton with placeholder design tokens"
```

---

## Task 7: InputManager — unified keyboard/mouse/touch navigation signals

**Files:**
- Create: `core/input/InputManager.h`
- Create: `core/input/InputManager.cpp`
- Modify: `app/main.cpp`
- Modify: `ui/Main.qml`
- Test: `tests/input/InputManagerTest.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  ```cpp
  class InputManager : public QObject {
      Q_OBJECT
  public:
      explicit InputManager(QObject *parent = nullptr);
      Q_INVOKABLE void handleKeyPress(int key);
  signals:
      void navigateUp();
      void navigateDown();
      void navigateLeft();
      void navigateRight();
      void accept();
      void cancel();
      void menu();
      void capture();  // reserved for in-game screenshot, wired in sub-project 3
  };
  ```
  Task 8 (gamepad) emits the same six navigation signals plus `capture`
  from a different source, so QML never special-cases the device.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/input/InputManagerTest.cpp
#include <QtTest>
#include <Qt>
#include "input/InputManager.h"

class InputManagerTest : public QObject {
    Q_OBJECT
private slots:
    void arrowUpEmitsNavigateUp() {
        InputManager mgr;
        QSignalSpy spy(&mgr, &InputManager::navigateUp);
        mgr.handleKeyPress(Qt::Key_Up);
        QCOMPARE(spy.count(), 1);
    }

    void enterEmitsAccept() {
        InputManager mgr;
        QSignalSpy spy(&mgr, &InputManager::accept);
        mgr.handleKeyPress(Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
    }

    void escapeEmitsCancel() {
        InputManager mgr;
        QSignalSpy spy(&mgr, &InputManager::cancel);
        mgr.handleKeyPress(Qt::Key_Escape);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(InputManagerTest)
#include "InputManagerTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/input/CMakeLists.txt
qt_add_executable(InputManagerTest InputManagerTest.cpp)
target_link_libraries(InputManagerTest PRIVATE Qt6::Test Qt6::Gui bili-core)
add_test(NAME InputManagerTest COMMAND InputManagerTest)
```
Add `add_subdirectory(input)` to `tests/CMakeLists.txt`.

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R InputManagerTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/input/InputManager.h
#pragma once
#include <QObject>

class InputManager : public QObject {
    Q_OBJECT
public:
    explicit InputManager(QObject *parent = nullptr);
    Q_INVOKABLE void handleKeyPress(int key);

signals:
    void navigateUp();
    void navigateDown();
    void navigateLeft();
    void navigateRight();
    void accept();
    void cancel();
    void menu();
    void capture();
};
```

```cpp
// core/input/InputManager.cpp
#include "InputManager.h"
#include <Qt>

InputManager::InputManager(QObject *parent) : QObject(parent) {}

void InputManager::handleKeyPress(int key) {
    switch (key) {
        case Qt::Key_Up: emit navigateUp(); break;
        case Qt::Key_Down: emit navigateDown(); break;
        case Qt::Key_Left: emit navigateLeft(); break;
        case Qt::Key_Right: emit navigateRight(); break;
        case Qt::Key_Return:
        case Qt::Key_Enter: emit accept(); break;
        case Qt::Key_Escape: emit cancel(); break;
        case Qt::Key_M: emit menu(); break;
        case Qt::Key_F12: emit capture(); break;
        default: break;
    }
}
```

Add `core/input/InputManager.cpp` to `bili-core` sources; link `Qt6::Gui`
to `bili-core`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R InputManagerTest`
Expected: PASS.

- [ ] **Step 5: Wire into the app (keyboard + touch/mouse already native via QML)**

```cpp
// app/main.cpp (add before loadFromModule)
#include "input/InputManager.h"
// ...
InputManager inputManager;
engine.rootContext()->setContextProperty("InputManager", &inputManager);
```

```qml
// ui/Main.qml
ApplicationWindow {
    // ...existing properties...
    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => InputManager.handleKeyPress(event.key)

        Loader {
            anchors.fill: parent
            source: "screens/" + ScreenManager.currentScreen + "Screen.qml"
        }
    }
}
```

- [ ] **Step 6: Build and run manually**

Run: build and launch the app, press Up/Down/Enter/Escape.
Expected: no crash, no QML console errors (there's no visible reaction yet
since Task 5's screens don't consume these signals — that wiring happens
per-screen when GameList/GameDetails get real content in later
sub-projects; this task only proves the signal path exists end-to-end).

- [ ] **Step 7: Commit**

```bash
git add core/input tests/input app/main.cpp ui/Main.qml
git commit -m "feat: add InputManager with unified keyboard navigation signals"
```

---

## Task 8: Gamepad bridge — SDL2 thread feeding InputManager

**Files:**
- Create: `core/input/GamepadBridge.h`
- Create: `core/input/GamepadBridge.cpp`
- Modify: `core/CMakeLists.txt` (find and link SDL2)
- Modify: `app/main.cpp`
- Test: `tests/input/GamepadBridgeTest.cpp`

**Interfaces:**
- Consumes: `InputManager`'s six navigation signals + `capture` (Task 7) —
  `GamepadBridge` holds a pointer to an `InputManager` and calls its
  signals indirectly by emitting through it (see implementation).
- Produces:
  ```cpp
  class GamepadBridge : public QObject {
      Q_OBJECT
  public:
      explicit GamepadBridge(InputManager *inputManager, QObject *parent = nullptr);
      void start();  // spawns the SDL poll thread
      void stop();   // joins the thread cleanly
  };
  ```
  Nothing downstream depends on `GamepadBridge` directly — it only ever
  talks *to* `InputManager`, which is the single source of truth QML uses.

- [ ] **Step 1: Write the failing test**

This test verifies SDL initializes in input-only mode and shuts down
cleanly — it does not require a physical controller to pass.

```cpp
// tests/input/GamepadBridgeTest.cpp
#include <QtTest>
#include "input/InputManager.h"
#include "input/GamepadBridge.h"

class GamepadBridgeTest : public QObject {
    Q_OBJECT
private slots:
    void startsAndStopsWithoutCrashing() {
        InputManager inputManager;
        GamepadBridge bridge(&inputManager);
        bridge.start();
        QTest::qWait(100);
        bridge.stop();
        QVERIFY(true); // reaching here means no crash/deadlock on shutdown
    }
};

QTEST_MAIN(GamepadBridgeTest)
#include "GamepadBridgeTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/input/CMakeLists.txt (append)
qt_add_executable(GamepadBridgeTest GamepadBridgeTest.cpp)
target_link_libraries(GamepadBridgeTest PRIVATE Qt6::Test bili-core)
add_test(NAME GamepadBridgeTest COMMAND GamepadBridgeTest)
```

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R GamepadBridgeTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Find SDL2 in CMake**

```cmake
# core/CMakeLists.txt (add)
find_package(SDL2 REQUIRED)
target_link_libraries(bili-core PUBLIC SDL2::SDL2)
```

- [ ] **Step 4: Write minimal implementation**

```cpp
// core/input/GamepadBridge.h
#pragma once
#include <QObject>
#include <QThread>
#include <atomic>
#include "InputManager.h"

class GamepadBridge : public QObject {
    Q_OBJECT
public:
    explicit GamepadBridge(InputManager *inputManager, QObject *parent = nullptr);
    ~GamepadBridge() override;
    void start();
    void stop();

private:
    void pollLoop();

    InputManager *m_inputManager;
    QThread m_thread;
    std::atomic<bool> m_running{false};
};
```

```cpp
// core/input/GamepadBridge.cpp
#include "GamepadBridge.h"
#include <SDL.h>

GamepadBridge::GamepadBridge(InputManager *inputManager, QObject *parent)
    : QObject(parent), m_inputManager(inputManager) {}

GamepadBridge::~GamepadBridge() { stop(); }

void GamepadBridge::start() {
    if (m_running) return;
    m_running = true;

    QObject *worker = new QObject();
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, worker, [this]() { pollLoop(); });
    m_thread.start();
}

void GamepadBridge::stop() {
    if (!m_running) return;
    m_running = false;
    m_thread.quit();
    m_thread.wait();
}

void GamepadBridge::pollLoop() {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);

    SDL_Event event;
    while (m_running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        emit m_inputManager->navigateUp(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        emit m_inputManager->navigateDown(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        emit m_inputManager->navigateLeft(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        emit m_inputManager->navigateRight(); break;
                    case SDL_CONTROLLER_BUTTON_A:
                        emit m_inputManager->accept(); break;
                    case SDL_CONTROLLER_BUTTON_B:
                        emit m_inputManager->cancel(); break;
                    case SDL_CONTROLLER_BUTTON_START:
                        emit m_inputManager->menu(); break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        emit m_inputManager->capture(); break;
                    default: break;
                }
            }
        }
        SDL_Delay(8); // ~120Hz poll
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
}
```

Add `core/input/GamepadBridge.cpp` to `bili-core` sources.

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R GamepadBridgeTest`
Expected: PASS.

- [ ] **Step 6: Wire into the app**

```cpp
// app/main.cpp (after InputManager is created)
#include "input/GamepadBridge.h"
// ...
GamepadBridge gamepadBridge(&inputManager);
gamepadBridge.start();
QObject::connect(&app, &QCoreApplication::aboutToQuit,
                  [&gamepadBridge]() { gamepadBridge.stop(); });
```

- [ ] **Step 7: Manual test with a physical controller**

Run: build and launch the app with a USB/Bluetooth gamepad connected.
Expected: pressing D-pad/A/B/Start on the controller does not crash the
app (no visible reaction yet, same caveat as Task 7 step 6 — this proves
the signal path, not screen behavior). Test hot-plug: disconnect and
reconnect the controller while the app runs; app must not crash.

- [ ] **Step 8: Commit**

```bash
git add core/input tests/input core/CMakeLists.txt app/main.cpp
git commit -m "feat: add SDL2 GamepadBridge feeding InputManager signals"
```

---

## Task 9: FirstLaunchSetup screen — ROM folder picker

**Files:**
- Modify: `ui/screens/FirstLaunchSetupScreen.qml`
- Modify: `ui/screens/BootScreen.qml`
- Create: `core/storage/RomSourcesStore.h`
- Create: `core/storage/RomSourcesStore.cpp`
- Modify: `app/main.cpp`
- Test: `tests/storage/RomSourcesStoreTest.cpp`

**Interfaces:**
- Consumes: `ConfigStore` (Task 2).
- Produces:
  ```cpp
  struct RomSource { QString path; QString label; bool enabled; };

  class RomSourcesStore : public QObject {
      Q_OBJECT
  public:
      explicit RomSourcesStore(ConfigStore *configStore, QObject *parent = nullptr);
      Q_INVOKABLE QVariantList sources() const;      // list of RomSource as QVariantMap
      Q_INVOKABLE void addSource(const QString &path, const QString &label);
      Q_INVOKABLE void removeSource(const QString &path);
      Q_INVOKABLE void setEnabled(const QString &path, bool enabled);
      Q_INVOKABLE bool hasAnySource() const;
  };
  ```
  Task 10 (Settings screen) reuses `addSource`/`removeSource`/`setEnabled`/
  `sources()` for full CRUD management.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/storage/RomSourcesStoreTest.cpp
#include <QtTest>
#include <QTemporaryDir>
#include "storage/ConfigStore.h"
#include "storage/RomSourcesStore.h"

class RomSourcesStoreTest : public QObject {
    Q_OBJECT
private slots:
    void addPersistsAcrossReload() {
        QTemporaryDir dir;
        ConfigStore config(dir.path());
        RomSourcesStore store(&config);

        QVERIFY(!store.hasAnySource());
        store.addSource(dir.path() + "/ROMs", "Principal");
        QVERIFY(store.hasAnySource());
        QCOMPARE(store.sources().size(), 1);

        ConfigStore reloadedConfig(dir.path());
        reloadedConfig.load();
        RomSourcesStore reloadedStore(&reloadedConfig);
        QCOMPARE(reloadedStore.sources().size(), 1);
    }

    void removeDropsSource() {
        QTemporaryDir dir;
        ConfigStore config(dir.path());
        RomSourcesStore store(&config);
        store.addSource(dir.path() + "/ROMs", "Principal");
        store.removeSource(dir.path() + "/ROMs");
        QVERIFY(!store.hasAnySource());
    }
};

QTEST_MAIN(RomSourcesStoreTest)
#include "RomSourcesStoreTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/storage/CMakeLists.txt (append)
qt_add_executable(RomSourcesStoreTest RomSourcesStoreTest.cpp)
target_link_libraries(RomSourcesStoreTest PRIVATE Qt6::Test bili-core)
add_test(NAME RomSourcesStoreTest COMMAND RomSourcesStoreTest)
```

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R RomSourcesStoreTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/storage/RomSourcesStore.h
#pragma once
#include <QObject>
#include <QVariantList>
#include "ConfigStore.h"

class RomSourcesStore : public QObject {
    Q_OBJECT
public:
    explicit RomSourcesStore(ConfigStore *configStore, QObject *parent = nullptr);
    Q_INVOKABLE QVariantList sources() const;
    Q_INVOKABLE void addSource(const QString &path, const QString &label);
    Q_INVOKABLE void removeSource(const QString &path);
    Q_INVOKABLE void setEnabled(const QString &path, bool enabled);
    Q_INVOKABLE bool hasAnySource() const;

private:
    ConfigStore *m_configStore;
};
```

```cpp
// core/storage/RomSourcesStore.cpp
#include "RomSourcesStore.h"
#include <QJsonArray>
#include <QJsonObject>

RomSourcesStore::RomSourcesStore(ConfigStore *configStore, QObject *parent)
    : QObject(parent), m_configStore(configStore) {}

QVariantList RomSourcesStore::sources() const {
    QVariantList result;
    for (const auto &v : m_configStore->data()["romSources"].toArray()) {
        result.append(v.toObject().toVariantMap());
    }
    return result;
}

void RomSourcesStore::addSource(const QString &path, const QString &label) {
    QJsonObject data = m_configStore->data();
    QJsonArray sources = data["romSources"].toArray();

    QJsonObject entry;
    entry["path"] = path;
    entry["label"] = label;
    entry["enabled"] = true;
    sources.append(entry);

    data["romSources"] = sources;
    m_configStore->setData(data);
    m_configStore->save();
}

void RomSourcesStore::removeSource(const QString &path) {
    QJsonObject data = m_configStore->data();
    QJsonArray sources = data["romSources"].toArray();
    QJsonArray filtered;
    for (const auto &v : sources) {
        if (v.toObject()["path"].toString() != path) filtered.append(v);
    }
    data["romSources"] = filtered;
    m_configStore->setData(data);
    m_configStore->save();
}

void RomSourcesStore::setEnabled(const QString &path, bool enabled) {
    QJsonObject data = m_configStore->data();
    QJsonArray sources = data["romSources"].toArray();
    for (int i = 0; i < sources.size(); ++i) {
        QJsonObject entry = sources[i].toObject();
        if (entry["path"].toString() == path) {
            entry["enabled"] = enabled;
            sources[i] = entry;
        }
    }
    data["romSources"] = sources;
    m_configStore->setData(data);
    m_configStore->save();
}

bool RomSourcesStore::hasAnySource() const {
    return !m_configStore->data()["romSources"].toArray().isEmpty();
}
```

Add `core/storage/RomSourcesStore.cpp` to `bili-core` sources.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R RomSourcesStoreTest`
Expected: PASS.

- [ ] **Step 5: Wire into the app and skip-if-configured boot logic**

```cpp
// app/main.cpp
#include "storage/ConfigStore.h"
#include "storage/RomSourcesStore.h"
#include <QStandardPaths>
#include <QCoreApplication>
// ...
const QString dataDir = QCoreApplication::applicationDirPath() + "/data";
ConfigStore configStore(dataDir);
configStore.load();
RomSourcesStore romSourcesStore(&configStore);
engine.rootContext()->setContextProperty("RomSourcesStore", &romSourcesStore);
```

```qml
// ui/screens/BootScreen.qml
import QtQuick

Rectangle {
    anchors.fill: parent
    color: "black"
    Text {
        anchors.centerIn: parent
        text: "Bili"
        color: "white"
        font.pixelSize: 40
    }
    Timer {
        interval: 500
        running: true
        onTriggered: {
            ScreenManager.push(RomSourcesStore.hasAnySource() ? "MainMenu" : "FirstLaunchSetup")
        }
    }
}
```

```qml
// ui/screens/FirstLaunchSetupScreen.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit * 2

        Text {
            text: "Où sont tes ROMs ?"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        Text {
            text: "Dossier proposé : " + defaultRomsPath
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeBody
        }

        Row {
            spacing: Theme.spacingUnit
            Button {
                text: "Utiliser ce dossier"
                onClicked: {
                    RomSourcesStore.addSource(defaultRomsPath, "Principal")
                    ScreenManager.push("MainMenu")
                }
            }
            Button {
                text: "Choisir un autre dossier"
                onClicked: folderDialog.open()
            }
        }
    }

    property string defaultRomsPath: applicationDirPath + "/ROMs"

    FolderDialog {
        id: folderDialog
        onAccepted: {
            RomSourcesStore.addSource(selectedFolder.toString(), "Principal")
            ScreenManager.push("MainMenu")
        }
    }
}
```

(`applicationDirPath` must be exposed as a context property alongside
`InputManager`/`ScreenManager` in `main.cpp`:
`engine.rootContext()->setContextProperty("applicationDirPath", QCoreApplication::applicationDirPath());`)

- [ ] **Step 6: Build and run manually**

Run: delete `data/config.json` if present, launch the app.
Expected: boot screen shows briefly, then `FirstLaunchSetupScreen` appears
with a proposed path; clicking "Utiliser ce dossier" navigates to
`MainMenu` and creates `data/config.json` with a `romSources` entry.
Relaunching the app skips straight to `MainMenu`.

- [ ] **Step 7: Commit**

```bash
git add core/storage tests/storage ui/screens app/main.cpp
git commit -m "feat: add RomSourcesStore and first-launch ROM folder setup"
```

---

## Task 10: Settings screen — manage ROM folders (CRUD)

**Files:**
- Modify: `ui/screens/SettingsScreen.qml`

**Interfaces:**
- Consumes: `RomSourcesStore` (Task 9), already exposed as a context property.
- Produces: nothing new for other tasks — this is a leaf UI screen.

- [ ] **Step 1: Write `SettingsScreen.qml` with a ROM-folder list + add/remove/toggle**

```qml
// ui/screens/SettingsScreen.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingUnit * 2
        spacing: Theme.spacingUnit

        Text {
            text: "Réglages — Dossiers ROMs"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        ListView {
            width: parent.width
            height: 300
            model: RomSourcesStore.sources()
            delegate: Row {
                spacing: Theme.spacingUnit
                Text {
                    text: modelData.label + " — " + modelData.path
                    color: Theme.colorText
                    font.pixelSize: Theme.fontSizeBody
                }
                CheckBox {
                    checked: modelData.enabled
                    onToggled: {
                        RomSourcesStore.setEnabled(modelData.path, checked)
                        romList.model = RomSourcesStore.sources()
                    }
                }
                Button {
                    text: "Retirer"
                    onClicked: {
                        RomSourcesStore.removeSource(modelData.path)
                        romList.model = RomSourcesStore.sources()
                    }
                }
            }
        }

        Button {
            text: "Ajouter un dossier"
            onClicked: addFolderDialog.open()
        }

        Button {
            text: "Retour"
            onClicked: ScreenManager.pop()
        }
    }

    FolderDialog {
        id: addFolderDialog
        onAccepted: {
            RomSourcesStore.addSource(selectedFolder.toString(), "Dossier " + (RomSourcesStore.sources().length + 1))
        }
    }
}
```

- [ ] **Step 2: Wire MainMenu → Settings navigation**

Edit `ui/screens/MainMenuScreen.qml` to add a button that calls
`ScreenManager.push("Settings")` (see Task 11, which adds this alongside
the Emulators/Scraper buttons in one pass).

- [ ] **Step 3: Build and run manually**

Run: launch the app, navigate MainMenu → Settings.
Expected: the ROM folder added during first launch is listed; adding
a second folder via "Ajouter un dossier" shows both; unchecking a
checkbox and reopening Settings later preserves the disabled state
(persisted through `RomSourcesStore`); "Retirer" removes an entry and
persists.

- [ ] **Step 4: Commit**

```bash
git add ui/screens/SettingsScreen.qml
git commit -m "feat: add Settings screen with ROM folder CRUD"
```

---

## Task 11: Stub providers + MainMenu wiring (Emulators/Scraper/Netplay buttons)

**Files:**
- Create: `core/emulators/IEmulatorProvider.h`
- Create: `core/emulators/StubEmulatorProvider.h`
- Create: `core/emulators/StubEmulatorProvider.cpp`
- Create: `core/scraper/IScraperProvider.h`
- Create: `core/scraper/StubScraperProvider.h`
- Create: `core/scraper/StubScraperProvider.cpp`
- Create: `core/netplay/INetplaySession.h`
- Create: `core/netplay/StubNetplaySession.h`
- Create: `core/netplay/StubNetplaySession.cpp`
- Modify: `ui/screens/MainMenuScreen.qml`
- Modify: `ui/screens/EmulatorManagerScreen.qml`
- Modify: `ui/screens/ScraperManagerScreen.qml`
- Modify: `app/main.cpp`
- Test: `tests/emulators/StubEmulatorProviderTest.cpp`
- Test: `tests/scraper/StubScraperProviderTest.cpp`
- Test: `tests/netplay/StubNetplaySessionTest.cpp`

**Interfaces:**
- Consumes: `NetworkManager` (Task 4, referenced but not called by the
  stubs themselves — real providers in sub-projects 3/4 will take a
  `NetworkManager*` constructor argument).
- Produces:
  ```cpp
  class IEmulatorProvider {
  public:
      virtual ~IEmulatorProvider() = default;
      virtual bool isImplemented() const = 0;
      virtual QString installRetroArch() = 0;      // returns status string
      virtual QString installCore(const QString &system) = 0;
      virtual QString installStandaloneEmulator(const QString &name) = 0;
  };

  class IScraperProvider {
  public:
      virtual ~IScraperProvider() = default;
      virtual bool isImplemented() const = 0;
      virtual QString scrapeGame(qint64 gameId) = 0;
      virtual QString scrapeLibrary() = 0;
  };

  class INetplaySession {
  public:
      virtual ~INetplaySession() = default;
      virtual bool isImplemented() const = 0;
      virtual QString host() = 0;
      virtual QString join(const QString &address) = 0;
  };
  ```
  Sub-project 3 implements `IEmulatorProvider` for real (replacing
  `StubEmulatorProvider` in `main.cpp`'s wiring); sub-project 4 does the
  same for `IScraperProvider`; sub-project 6 for `INetplaySession`. QML
  never references the stub classes directly — only through
  `EmulatorProvider`/`ScraperProvider`/`NetplaySession` context properties,
  so swapping the implementation later touches only `main.cpp`.

- [ ] **Step 1: Write the three failing tests**

```cpp
// tests/emulators/StubEmulatorProviderTest.cpp
#include <QtTest>
#include "emulators/StubEmulatorProvider.h"

class StubEmulatorProviderTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotImplemented() {
        StubEmulatorProvider provider;
        QVERIFY(!provider.isImplemented());
        QVERIFY(provider.installRetroArch().contains("non implémenté"));
        QVERIFY(provider.installCore("nes").contains("non implémenté"));
        QVERIFY(provider.installStandaloneEmulator("Dolphin").contains("non implémenté"));
    }
};

QTEST_MAIN(StubEmulatorProviderTest)
#include "StubEmulatorProviderTest.moc"
```

```cpp
// tests/scraper/StubScraperProviderTest.cpp
#include <QtTest>
#include "scraper/StubScraperProvider.h"

class StubScraperProviderTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotImplemented() {
        StubScraperProvider provider;
        QVERIFY(!provider.isImplemented());
        QVERIFY(provider.scrapeGame(1).contains("non implémenté"));
        QVERIFY(provider.scrapeLibrary().contains("non implémenté"));
    }
};

QTEST_MAIN(StubScraperProviderTest)
#include "StubScraperProviderTest.moc"
```

```cpp
// tests/netplay/StubNetplaySessionTest.cpp
#include <QtTest>
#include "netplay/StubNetplaySession.h"

class StubNetplaySessionTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotImplemented() {
        StubNetplaySession session;
        QVERIFY(!session.isImplemented());
        QVERIFY(session.host().contains("non implémenté"));
        QVERIFY(session.join("127.0.0.1").contains("non implémenté"));
    }
};

QTEST_MAIN(StubNetplaySessionTest)
#include "StubNetplaySessionTest.moc"
```

- [ ] **Step 2: Wire the three test targets, run, verify they fail**

```cmake
# tests/emulators/CMakeLists.txt
qt_add_executable(StubEmulatorProviderTest StubEmulatorProviderTest.cpp)
target_link_libraries(StubEmulatorProviderTest PRIVATE Qt6::Test bili-core)
add_test(NAME StubEmulatorProviderTest COMMAND StubEmulatorProviderTest)
```
```cmake
# tests/scraper/CMakeLists.txt
qt_add_executable(StubScraperProviderTest StubScraperProviderTest.cpp)
target_link_libraries(StubScraperProviderTest PRIVATE Qt6::Test bili-core)
add_test(NAME StubScraperProviderTest COMMAND StubScraperProviderTest)
```
```cmake
# tests/netplay/CMakeLists.txt
qt_add_executable(StubNetplaySessionTest StubNetplaySessionTest.cpp)
target_link_libraries(StubNetplaySessionTest PRIVATE Qt6::Test bili-core)
add_test(NAME StubNetplaySessionTest COMMAND StubNetplaySessionTest)
```
Add all three `add_subdirectory(...)` lines to `tests/CMakeLists.txt`.

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R "StubEmulatorProviderTest|StubScraperProviderTest|StubNetplaySessionTest"`
Expected: FAIL (compile errors).

- [ ] **Step 3: Write minimal implementations**

```cpp
// core/emulators/IEmulatorProvider.h
#pragma once
#include <QString>

class IEmulatorProvider {
public:
    virtual ~IEmulatorProvider() = default;
    virtual bool isImplemented() const = 0;
    virtual QString installRetroArch() = 0;
    virtual QString installCore(const QString &system) = 0;
    virtual QString installStandaloneEmulator(const QString &name) = 0;
};
```

```cpp
// core/emulators/StubEmulatorProvider.h
#pragma once
#include "IEmulatorProvider.h"

class StubEmulatorProvider : public IEmulatorProvider {
public:
    bool isImplemented() const override { return false; }
    QString installRetroArch() override;
    QString installCore(const QString &system) override;
    QString installStandaloneEmulator(const QString &name) override;
};
```

```cpp
// core/emulators/StubEmulatorProvider.cpp
#include "StubEmulatorProvider.h"

QString StubEmulatorProvider::installRetroArch() {
    return "RetroArch : non implémenté (sous-projet 3)";
}
QString StubEmulatorProvider::installCore(const QString &system) {
    return "Core " + system + " : non implémenté (sous-projet 3)";
}
QString StubEmulatorProvider::installStandaloneEmulator(const QString &name) {
    return "Émulateur " + name + " : non implémenté (sous-projet 3)";
}
```

```cpp
// core/scraper/IScraperProvider.h
#pragma once
#include <QString>

class IScraperProvider {
public:
    virtual ~IScraperProvider() = default;
    virtual bool isImplemented() const = 0;
    virtual QString scrapeGame(qint64 gameId) = 0;
    virtual QString scrapeLibrary() = 0;
};
```

```cpp
// core/scraper/StubScraperProvider.h
#pragma once
#include "IScraperProvider.h"

class StubScraperProvider : public IScraperProvider {
public:
    bool isImplemented() const override { return false; }
    QString scrapeGame(qint64 gameId) override;
    QString scrapeLibrary() override;
};
```

```cpp
// core/scraper/StubScraperProvider.cpp
#include "StubScraperProvider.h"

QString StubScraperProvider::scrapeGame(qint64 gameId) {
    return "Scraping jeu #" + QString::number(gameId) + " : non implémenté (sous-projet 4)";
}
QString StubScraperProvider::scrapeLibrary() {
    return "Scraping bibliothèque : non implémenté (sous-projet 4)";
}
```

```cpp
// core/netplay/INetplaySession.h
#pragma once
#include <QString>

class INetplaySession {
public:
    virtual ~INetplaySession() = default;
    virtual bool isImplemented() const = 0;
    virtual QString host() = 0;
    virtual QString join(const QString &address) = 0;
};
```

```cpp
// core/netplay/StubNetplaySession.h
#pragma once
#include "INetplaySession.h"

class StubNetplaySession : public INetplaySession {
public:
    bool isImplemented() const override { return false; }
    QString host() override;
    QString join(const QString &address) override;
};
```

```cpp
// core/netplay/StubNetplaySession.cpp
#include "StubNetplaySession.h"

QString StubNetplaySession::host() {
    return "Multijoueur P2P : non implémenté (sous-projet 6)";
}
QString StubNetplaySession::join(const QString &address) {
    Q_UNUSED(address);
    return "Multijoueur P2P : non implémenté (sous-projet 6)";
}
```

Add the three new `.cpp` files to `bili-core` sources in `core/CMakeLists.txt`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build/windows-portable && ctest --test-dir build/windows-portable -R "StubEmulatorProviderTest|StubScraperProviderTest|StubNetplaySessionTest"`
Expected: PASS (3/3).

- [ ] **Step 5: Expose stubs to QML and wire MainMenu buttons**

Qt's C++/QML bridge needs `Q_INVOKABLE` wrappers, since raw abstract
interfaces aren't QObjects. Add a thin QObject wrapper per provider —
shown here for the emulator provider only (repeat identically for scraper
and netplay, substituting method names):

```cpp
// core/emulators/EmulatorProviderQmlBridge.h
#pragma once
#include <QObject>
#include "IEmulatorProvider.h"

class EmulatorProviderQmlBridge : public QObject {
    Q_OBJECT
public:
    explicit EmulatorProviderQmlBridge(IEmulatorProvider *provider, QObject *parent = nullptr)
        : QObject(parent), m_provider(provider) {}

    Q_INVOKABLE QString installRetroArch() { return m_provider->installRetroArch(); }
    Q_INVOKABLE QString installCore(const QString &system) { return m_provider->installCore(system); }
    Q_INVOKABLE QString installStandaloneEmulator(const QString &name) { return m_provider->installStandaloneEmulator(name); }

private:
    IEmulatorProvider *m_provider;
};
```

Create the analogous `ScraperProviderQmlBridge` (wrapping `scrapeGame`/
`scrapeLibrary`) and `NetplaySessionQmlBridge` (wrapping `host`/`join`) in
their respective module folders, and add all three headers (they are
header-only, no `.cpp` needed) to `bili-core`'s include path (already
covered by `target_include_directories` from Task 1).

```cpp
// app/main.cpp (add)
#include "emulators/StubEmulatorProvider.h"
#include "emulators/EmulatorProviderQmlBridge.h"
#include "scraper/StubScraperProvider.h"
#include "scraper/ScraperProviderQmlBridge.h"
#include "netplay/StubNetplaySession.h"
#include "netplay/NetplaySessionQmlBridge.h"
// ...
StubEmulatorProvider emulatorProvider;
EmulatorProviderQmlBridge emulatorBridge(&emulatorProvider);
engine.rootContext()->setContextProperty("EmulatorProvider", &emulatorBridge);

StubScraperProvider scraperProvider;
ScraperProviderQmlBridge scraperBridge(&scraperProvider);
engine.rootContext()->setContextProperty("ScraperProvider", &scraperBridge);

StubNetplaySession netplaySession;
NetplaySessionQmlBridge netplayBridge(&netplaySession);
engine.rootContext()->setContextProperty("NetplaySession", &netplayBridge);
```

```qml
// ui/screens/MainMenuScreen.qml
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit * 2

        Text {
            text: "MainMenu"
            color: Theme.colorText
            font.pixelSize: Theme.fontSizeTitle
        }

        Button { text: "Bibliothèque"; onClicked: ScreenManager.push("GameList") }
        Button { text: "Émulateurs"; onClicked: ScreenManager.push("EmulatorManager") }
        Button { text: "Scraper"; onClicked: ScreenManager.push("ScraperManager") }
        Button { text: "Multijoueur en ligne (P2P) — bientôt disponible"; enabled: false }
        Button { text: "Réglages"; onClicked: ScreenManager.push("Settings") }
    }
}
```

(The netplay button stays visible but disabled per the approved design —
sub-project 6 adds a real `NetplayScreen` and flips `enabled` once
`NetplaySession` has a working implementation.)

```qml
// ui/screens/EmulatorManagerScreen.qml
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit
        Text { text: "Installer"; color: Theme.colorText; font.pixelSize: Theme.fontSizeTitle }
        Button { text: "Installer RetroArch"; onClicked: statusText.text = EmulatorProvider.installRetroArch() }
        Button { text: "Installer un core"; onClicked: statusText.text = EmulatorProvider.installCore("nes") }
        Button { text: "Installer un émulateur autonome"; onClicked: statusText.text = EmulatorProvider.installStandaloneEmulator("Dolphin") }
        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
        Button { text: "Retour"; onClicked: ScreenManager.pop() }
    }
}
```

```qml
// ui/screens/ScraperManagerScreen.qml
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingUnit
        Text { text: "Scraper"; color: Theme.colorText; font.pixelSize: Theme.fontSizeTitle }
        Button { text: "Scraper toute la bibliothèque"; onClicked: statusText.text = ScraperProvider.scrapeLibrary() }
        Text { id: statusText; color: Theme.colorAccent; font.pixelSize: Theme.fontSizeBody }
        Button { text: "Retour"; onClicked: ScreenManager.pop() }
    }
}
```

- [ ] **Step 6: Build and run manually**

Run: launch the app, navigate MainMenu → Émulateurs, click each install
button; navigate MainMenu → Scraper, click "Scraper toute la
bibliothèque".
Expected: each button shows a "non implémenté (sous-projet N)" status text
without crashing; "Retour" returns to MainMenu each time.

- [ ] **Step 7: Commit**

```bash
git add core/emulators core/scraper core/netplay tests/emulators tests/scraper tests/netplay ui/screens app/main.cpp tests/CMakeLists.txt
git commit -m "feat: add stub providers and wire Emulators/Scraper/Netplay entry points"
```

---

## Task 12: Docs index

**Files:**
- Create: `docs/index.md`

**Interfaces:** none — documentation only.

- [ ] **Step 1: Write `docs/index.md`**

```markdown
# Index des manuels de référence

Ce fichier recense les manuels téléchargés localement dans `docs/`, pour
être lus à la demande plutôt que devinés. Ne pas télécharger de manuel par
anticipation — seulement quand un point précis de l'implémentation en a
besoin.

| Fichier | Sujet | Ajouté pour |
|---|---|---|
| (aucun pour l'instant) | | |
```

- [ ] **Step 2: Commit**

```bash
git add docs/index.md
git commit -m "docs: add reference manual index"
```

---

## Task 13: CMakePresets for linux/rpi/android + Windows portable packaging

**Files:**
- Modify: `CMakePresets.json`
- Create: `platform/windows/package.ps1`

**Interfaces:** none — build/packaging only, no code consumed downstream.

- [ ] **Step 1: Add `linux`, `rpi`, `android` configure presets**

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "windows-portable",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/windows-portable",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },
    {
      "name": "linux",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/linux",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },
    {
      "name": "rpi",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/rpi",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "QT_QPA_PLATFORM": "eglfs"
      }
    },
    {
      "name": "android",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/android",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_TOOLCHAIN_FILE": "$env{ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake",
        "ANDROID_ABI": "arm64-v8a"
      }
    }
  ]
}
```

Note: `rpi`'s cross-compilation toolchain (sysroot, target triple) and
`android`'s Gradle/APK packaging step are detailed in sub-project 7 — these
presets only establish named build directories with the right base flags
so sub-project 7 has a place to attach the rest.

- [ ] **Step 2: Write the Windows portable packaging script**

```powershell
# platform/windows/package.ps1
param(
    [string]$BuildDir = "build/windows-portable",
    [string]$OutDir = "dist/windows-portable"
)

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Copy-Item "$BuildDir/app/bili-frontend.exe" -Destination $OutDir

$windeployqt = (Get-Command windeployqt).Source
& $windeployqt --qmldir ui "$OutDir/bili-frontend.exe"

Write-Host "Portable build ready at $OutDir — no installer, run bili-frontend.exe directly."
```

- [ ] **Step 3: Run the packaging script**

Run: `cmake --build build/windows-portable && pwsh platform/windows/package.ps1`
Expected: `dist/windows-portable/` contains `bili-frontend.exe` plus all
Qt DLLs/plugins `windeployqt` bundles; double-clicking the exe from that
folder (copied to a machine/user profile without Qt installed, if
possible) launches the app with no installer prompt.

- [ ] **Step 4: Commit**

```bash
git add CMakePresets.json platform/windows/package.ps1
git commit -m "build: add linux/rpi/android presets and Windows portable packaging script"
```

---

## Task 14: Raspberry Pi validation spike (manual, non-automated)

**Files:** none created — this is a manual hardware verification task,
documented here so it isn't skipped.

**Interfaces:** none.

- [ ] **Step 1: Cross-build or natively build the `rpi` preset on real Raspberry Pi hardware**

Run: `cmake --preset rpi && cmake --build build/rpi`
Expected: build succeeds targeting the `eglfs` QPA backend.

- [ ] **Step 2: Run the app on-device**

Run: `QT_QPA_PLATFORM=eglfs ./build/rpi/app/bili-frontend`
Expected: window renders full-screen; Boot → FirstLaunchSetup/MainMenu
transition plays at a visually smooth frame rate (no perceptible stutter
on the Timer-driven fade/transition from Task 5).

- [ ] **Step 3: Record the result**

Update the risk note in `C:\Users\Steam\.claude\plans\tranquil-weaving-karp.md`
(Contexte section, "Risque résiduel") with the measured outcome — either
confirm Qt6/QML performs acceptably on the tested Pi model, or flag a
concrete performance problem before any further sub-project builds on top
of this socle.

- [ ] **Step 4: Commit** (only if step 3 touched tracked files)

```bash
git add "C:\Users\Steam\.claude\plans\tranquil-weaving-karp.md"
git commit -m "docs: record Raspberry Pi socle validation result"
```
