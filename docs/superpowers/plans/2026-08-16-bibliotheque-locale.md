# Bibliothèque Locale Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Scan the ROM folders configured in sub-project 1, detect each
file's system, index everything into `library.db`, and replace
`GameListScreen`'s placeholder with a real, keyboard/gamepad-navigable
game list.

**Architecture:** A `RomScanner` C++ class walks each enabled
`RomSource`, detects system by extension (including files inside
`.zip`/`.7z` archives), cleans titles, and syncs results into the
existing `LibraryDatabase` (insert new, remove stale — never touches
files on disk). Scanning runs on a dedicated `QThread` (same pattern as
`GamepadBridge`) so it never blocks the UI. A `QAbstractListModel`
(`LibraryModel`) exposes the database to QML for `GameListScreen`.

**Tech Stack:** C++17, Qt6 (Core, Sql, already-linked), the existing
`LibraryDatabase`/`RomSourcesStore` classes from sub-project 1.

**Spec:** `docs/superpowers/specs/2026-08-16-bibliotheque-locale-design.md`

## Global Constraints

- Virtual indexing only — never move, rename, or modify ROM files on disk.
- No deduplication across multiple files/regions of the same game — each
  detected file is its own `library.db` row (existing `rom_path UNIQUE`
  constraint is the only uniqueness rule).
- Scanning must never block the UI thread (background `QThread`, matching
  `core/input/GamepadBridge.cpp`'s pattern from sub-project 1).
- Reuse the existing keyboard/gamepad focus-navigation pattern from
  sub-project 1 (`KeyNavigation` + `Theme.focusBorderColor`/
  `focusBorderWidth`/`focusRadius` tokens) for any new navigable UI —
  never build a second focus mechanism.
- Toolchain is MinGW-w64/Qt-mingw on `D:\Qt`, SDL2 at
  `D:\SDL2\x86_64-w64-mingw32`. Every `cmake`/`ctest`/`ninja` command
  assumes `platform/windows/dev-env.ps1` has been dot-sourced first in
  the session.
- Follow the existing project convention: build-file touches
  (`core/CMakeLists.txt`, `tests/CMakeLists.txt`/subdirectory
  `CMakeLists.txt`) are described in each task's Steps even where not
  spelled out in a task's Files header — the Steps are the source of truth.
- **Never guess a library/API you're not sure about — research it (web
  search, official docs) before writing code that depends on it,** and
  record what you chose and why in `docs/index.md` (create the "Fichier /
  Sujet / Ajouté pour" table entry — the file already exists from
  sub-project 1 with an empty table).

---

## Task 1: `RomScanner` — system detection and title cleanup

**Files:**
- Create: `core/library/RomScanner.h`
- Create: `core/library/RomScanner.cpp`
- Test: `tests/library/RomScannerTest.cpp`

**Interfaces:**
- Consumes: nothing (pure static helpers, no dependencies on other
  sub-project-2 classes yet).
- Produces:
  ```cpp
  class RomScanner {
  public:
      // Returns a lowercase system id ("nes", "snes", "gba", ...) based
      // on the file's extension, or an empty string if unrecognized.
      static QString detectSystem(const QString &fileName);

      // Strips the extension and any No-Intro/TOSEC-style tags in
      // parentheses/brackets (e.g. "(USA)", "[!]", "(Rev 1)") from a
      // filename, returning a clean provisional title.
      static QString cleanTitle(const QString &fileName);
  };
  ```
  Task 2 calls both of these for every file it finds on disk.

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/library/RomScannerTest.cpp
#include <QtTest>
#include "library/RomScanner.h"

class RomScannerTest : public QObject {
    Q_OBJECT
private slots:
    void detectsKnownSystemsByExtension() {
        QCOMPARE(RomScanner::detectSystem("Super Mario World (USA).sfc"), QString("snes"));
        QCOMPARE(RomScanner::detectSystem("Super Mario World (USA).smc"), QString("snes"));
        QCOMPARE(RomScanner::detectSystem("Zelda.nes"), QString("nes"));
        QCOMPARE(RomScanner::detectSystem("Pokemon.gba"), QString("gba"));
        QCOMPARE(RomScanner::detectSystem("Pokemon (Rev 1).gb"), QString("gb"));
        QCOMPARE(RomScanner::detectSystem("Pokemon.gbc"), QString("gb"));
        QCOMPARE(RomScanner::detectSystem("Mario64.n64"), QString("n64"));
        QCOMPARE(RomScanner::detectSystem("Mario64.z64"), QString("n64"));
        QCOMPARE(RomScanner::detectSystem("Sonic.md"), QString("genesis"));
        QCOMPARE(RomScanner::detectSystem("Sonic.gen"), QString("genesis"));
    }

    void detectSystemIsCaseInsensitiveOnExtension() {
        QCOMPARE(RomScanner::detectSystem("Zelda.NES"), QString("nes"));
    }

    void detectSystemReturnsEmptyForUnknownExtension() {
        QCOMPARE(RomScanner::detectSystem("readme.txt"), QString(""));
        QCOMPARE(RomScanner::detectSystem("noextension"), QString(""));
    }

    void cleanTitleStripsTagsAndExtension() {
        QCOMPARE(RomScanner::cleanTitle("Super Mario World (USA).sfc"), QString("Super Mario World"));
        QCOMPARE(RomScanner::cleanTitle("Sonic the Hedgehog (Europe) (Rev 1).md"), QString("Sonic the Hedgehog"));
        QCOMPARE(RomScanner::cleanTitle("Contra [!].nes"), QString("Contra"));
        QCOMPARE(RomScanner::cleanTitle("Kirby's Dream Land (USA, Europe).gb"), QString("Kirby's Dream Land"));
    }

    void cleanTitleHandlesNoTagsGracefully() {
        QCOMPARE(RomScanner::cleanTitle("Tetris.gb"), QString("Tetris"));
    }
};

QTEST_MAIN(RomScannerTest)
#include "RomScannerTest.moc"
```

- [ ] **Step 2: Wire the test target and run to verify it fails**

```cmake
# tests/library/CMakeLists.txt
qt_add_executable(RomScannerTest RomScannerTest.cpp)
target_link_libraries(RomScannerTest PRIVATE Qt6::Test bili-core)
add_test(NAME RomScannerTest COMMAND RomScannerTest)
```
Add `add_subdirectory(library)` to `tests/CMakeLists.txt`.

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R RomScannerTest`
Expected: FAIL (compile error — `RomScanner` doesn't exist).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/library/RomScanner.h
#pragma once
#include <QString>

class RomScanner {
public:
    static QString detectSystem(const QString &fileName);
    static QString cleanTitle(const QString &fileName);
};
```

```cpp
// core/library/RomScanner.cpp
#include "RomScanner.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QMap>

QString RomScanner::detectSystem(const QString &fileName) {
    static const QMap<QString, QString> kExtensionToSystem = {
        {"nes", "nes"},
        {"sfc", "snes"}, {"smc", "snes"},
        {"gba", "gba"},
        {"gb", "gb"}, {"gbc", "gb"},
        {"n64", "n64"}, {"z64", "n64"},
        {"md", "genesis"}, {"gen", "genesis"},
    };
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return kExtensionToSystem.value(ext, QString());
}

QString RomScanner::cleanTitle(const QString &fileName) {
    QString base = QFileInfo(fileName).completeBaseName();
    static const QRegularExpression kTagPattern(R"(\s*[\(\[][^\)\]]*[\)\]]\s*)");
    base.replace(kTagPattern, " ");
    return base.trimmed();
}
```

Add `core/library/RomScanner.cpp` to `bili-core`'s sources in
`core/CMakeLists.txt`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R RomScannerTest`
Expected: PASS (5/5).

- [ ] **Step 5: Commit**

```bash
git add core/library tests/library tests/CMakeLists.txt core/CMakeLists.txt
git commit -m "feat: add RomScanner system detection and title cleanup"
```

---

## Task 2: Directory scanning + incremental sync into `library.db`

**Files:**
- Modify: `core/storage/LibraryDatabase.h`
- Modify: `core/storage/LibraryDatabase.cpp`
- Modify: `core/library/RomScanner.h`
- Modify: `core/library/RomScanner.cpp`
- Test: `tests/storage/LibraryDatabaseTest.cpp` (append cases)
- Test: `tests/library/RomScannerTest.cpp` (append cases)

**Interfaces:**
- Consumes: `RomScanner::detectSystem`/`cleanTitle` (Task 1),
  `LibraryDatabase::insertGame`/`gameCount` (sub-project 1).
- Produces:
  ```cpp
  // LibraryDatabase additions
  QStringList allRomPaths() const;       // every rom_path currently indexed
  void removeGame(const QString &romPath);

  // RomScanner addition
  // Recursively scans dirPath for files with a recognized extension,
  // inserts new ones into db, and removes db entries under dirPath
  // whose file no longer exists on disk. Returns the number of files
  // found this scan (new + already-present, not counting removals).
  static int scanDirectory(const QString &dirPath, LibraryDatabase &db);
  ```
  Task 3 extends `scanDirectory` to also look inside archives. Task 4
  wraps `scanDirectory` in a background thread. Task 7 calls it once per
  enabled `RomSource`.

- [ ] **Step 1: Write the failing tests**

```cpp
// tests/storage/LibraryDatabaseTest.cpp (append inside the existing class)
void allRomPathsReturnsEveryIndexedPath() {
    QTemporaryDir dir;
    LibraryDatabase db(dir.path() + "/library.db");
    QVERIFY(db.open());
    db.insertGame("/roms/a.nes", "nes", "A");
    db.insertGame("/roms/b.sfc", "snes", "B");
    QStringList paths = db.allRomPaths();
    QCOMPARE(paths.size(), 2);
    QVERIFY(paths.contains("/roms/a.nes"));
    QVERIFY(paths.contains("/roms/b.sfc"));
}

void removeGameDeletesTheMatchingRow() {
    QTemporaryDir dir;
    LibraryDatabase db(dir.path() + "/library.db");
    QVERIFY(db.open());
    db.insertGame("/roms/a.nes", "nes", "A");
    QCOMPARE(db.gameCount(), 1);
    db.removeGame("/roms/a.nes");
    QCOMPARE(db.gameCount(), 0);
}
```
(Add both as `private slots:` in the existing `LibraryDatabaseTest` class, same file.)

```cpp
// tests/library/RomScannerTest.cpp (append inside the existing class)
void scanDirectoryIndexesRecognizedFilesAndSkipsOthers() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile(dir.path() + "/Zelda.nes").open(QIODevice::WriteOnly);
    QFile(dir.path() + "/readme.txt").open(QIODevice::WriteOnly);
    QDir(dir.path()).mkdir("sub");
    QFile(dir.path() + "/sub/Mario.sfc").open(QIODevice::WriteOnly);

    LibraryDatabase db(dir.path() + "/library.db");
    QVERIFY(db.open());

    int found = RomScanner::scanDirectory(dir.path(), db);
    QCOMPARE(found, 2);
    QCOMPARE(db.gameCount(), 2);
    QStringList paths = db.allRomPaths();
    QVERIFY(paths.contains(dir.path() + "/Zelda.nes"));
    QVERIFY(paths.contains(dir.path() + "/sub/Mario.sfc"));
}

void scanDirectoryRemovesEntriesForDeletedFiles() {
    QTemporaryDir dir;
    QFile romFile(dir.path() + "/Zelda.nes");
    romFile.open(QIODevice::WriteOnly);
    romFile.close();

    LibraryDatabase db(dir.path() + "/library.db");
    QVERIFY(db.open());
    RomScanner::scanDirectory(dir.path(), db);
    QCOMPARE(db.gameCount(), 1);

    QFile::remove(dir.path() + "/Zelda.nes");
    RomScanner::scanDirectory(dir.path(), db);
    QCOMPARE(db.gameCount(), 0);
}

void scanDirectoryIsIdempotentOnUnchangedFiles() {
    QTemporaryDir dir;
    QFile(dir.path() + "/Zelda.nes").open(QIODevice::WriteOnly);

    LibraryDatabase db(dir.path() + "/library.db");
    QVERIFY(db.open());
    RomScanner::scanDirectory(dir.path(), db);
    RomScanner::scanDirectory(dir.path(), db);
    QCOMPARE(db.gameCount(), 1);
}
```
(Add `#include <QFile>`, `#include <QDir>`, `#include "storage/LibraryDatabase.h"` to the test file's includes.)

- [ ] **Step 2: Run to verify the new cases fail**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R "LibraryDatabaseTest|RomScannerTest"`
Expected: FAIL (compile errors — `allRomPaths`/`removeGame`/`scanDirectory` don't exist).

- [ ] **Step 3: Implement `LibraryDatabase` additions**

```cpp
// core/storage/LibraryDatabase.h (add to the public section)
QStringList allRomPaths() const;
void removeGame(const QString &romPath);
```

```cpp
// core/storage/LibraryDatabase.cpp (append)
QStringList LibraryDatabase::allRomPaths() const {
    QStringList paths;
    QSqlQuery q("SELECT rom_path FROM games", m_db);
    while (q.next()) {
        paths.append(q.value(0).toString());
    }
    return paths;
}

void LibraryDatabase::removeGame(const QString &romPath) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM games WHERE rom_path = ?");
    q.addBindValue(romPath);
    q.exec();
}
```

- [ ] **Step 4: Implement `RomScanner::scanDirectory`**

```cpp
// core/library/RomScanner.h (add to the public section)
#include "storage/LibraryDatabase.h"
// ...
static int scanDirectory(const QString &dirPath, LibraryDatabase &db);
```

```cpp
// core/library/RomScanner.cpp (append)
#include <QDirIterator>
#include <QSet>

int RomScanner::scanDirectory(const QString &dirPath, LibraryDatabase &db) {
    QSet<QString> foundOnDisk;
    int foundCount = 0;

    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString system = detectSystem(filePath);
        if (system.isEmpty()) continue;

        foundOnDisk.insert(filePath);
        foundCount++;

        if (!db.allRomPaths().contains(filePath)) {
            db.insertGame(filePath, system, cleanTitle(filePath));
        }
    }

    for (const QString &existingPath : db.allRomPaths()) {
        if (existingPath.startsWith(dirPath) && !foundOnDisk.contains(existingPath)) {
            db.removeGame(existingPath);
        }
    }

    return foundCount;
}
```

(Note: calling `db.allRomPaths()` inside the loop is O(n²) — acceptable
for this task's scope and test sizes; if a future task profiles this as
slow on large libraries, cache the initial `allRomPaths()` result before
the loop and update a local copy instead of re-querying. Not a
correctness issue, just a possible future optimization — don't
pre-optimize it now.)

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R "LibraryDatabaseTest|RomScannerTest"`
Expected: PASS (all cases, old and new).

- [ ] **Step 6: Commit**

```bash
git add core/storage/LibraryDatabase.h core/storage/LibraryDatabase.cpp core/library/RomScanner.h core/library/RomScanner.cpp tests/storage/LibraryDatabaseTest.cpp tests/library/RomScannerTest.cpp
git commit -m "feat: add directory scanning with incremental sync to library.db"
```

---

## Task 3: Archive (.zip/.7z) scanning support

**Files:**
- Modify: `core/library/RomScanner.h`
- Modify: `core/library/RomScanner.cpp`
- Modify: `core/CMakeLists.txt` (new dependency, exact form TBD by research)
- Modify: `CMakePresets.json` (if the chosen library needs a
  `CMAKE_PREFIX_PATH` entry, same pattern as SDL2 in sub-project 1)
- Modify: `docs/index.md`
- Test: `tests/library/RomScannerTest.cpp` (append cases)

**Interfaces:**
- Consumes: `RomScanner::detectSystem`/`cleanTitle` (Task 1).
- Produces: `RomScanner::scanDirectory` (Task 2) now also finds ROMs
  inside `.zip` archives encountered during the walk — same public
  signature as Task 2, no interface change for callers.

**Before writing any code:** research the current best option for
reading `.zip` (and, if feasible without excessive complexity, `.7z`)
archive contents from C++/Qt6 on Windows/MinGW. Do not guess a library
or API — use web search to confirm what's actually available and
maintained today, matching how SDL2 was sourced in sub-project 1 (exact
version/URL verified via `gh api`/official releases, not assumed).
Candidates worth checking (verify current state, don't assume any of
these is still the right answer): a header-only/minimal `.zip` reader
(e.g. `miniz`), Qt's own `QuaZip` third-party wrapper, or shelling out to
a bundled `7z.exe`. `.7z` is a harder format (LZMA-based) — if a clean
Windows/MinGW-compatible option isn't readily available without
significant added complexity, it's acceptable to ship `.zip` support
only for this task and record `.7z` as an explicit, documented gap in
`docs/index.md` and this plan's own tracking — do not fake or skip
`.zip` support to avoid the research.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/library/RomScannerTest.cpp (append)
void scanDirectoryFindsRomsInsideZipArchives() {
    QTemporaryDir dir;
    // Build a minimal valid .zip containing "Zelda.nes" using whatever
    // API Task 3's implementation uses (e.g. the same library RomScanner
    // itself uses, so this test doesn't need a second dependency) --
    // write the zip's bytes directly if the chosen library exposes a
    // writer, or vendor a tiny pre-built fixture .zip under
    // tests/library/fixtures/sample_rom.zip (containing one file,
    // "Zelda.nes", with arbitrary content) and copy it into the temp dir
    // instead of generating it at runtime, whichever is simpler given
    // the library chosen in Step 1's research.
    // ...
    LibraryDatabase db(dir.path() + "/library.db");
    QVERIFY(db.open());
    int found = RomScanner::scanDirectory(dir.path(), db);
    QCOMPARE(found, 1);
    QVERIFY(db.allRomPaths().at(0).contains("Zelda.nes"));
}
```

This test's exact fixture-creation mechanics depend on the library
chosen in the research step above — write the concrete version once
that's decided (this plan cannot specify exact API calls for a library
not yet chosen; this is the one step in this entire plan where the
concrete code is intentionally deferred, and only because it is
downstream of a required research step, not because it wasn't thought
through).

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R RomScannerTest`
Expected: FAIL (compile error or test failure, depending on how far Step 1 got implemented).

- [ ] **Step 3: Implement archive scanning**

Extend `RomScanner::scanDirectory`'s loop: when a file's extension is
`.zip` (and `.7z` if in scope per the research outcome), open it with
the chosen library, enumerate entries, and for each entry whose name
(not the archive's own name) yields a non-empty `detectSystem(...)`,
treat it as a found ROM. Store its `rom_path` as
`"<archive-path>::<entry-name>"` (a `::` separator convention — later
sub-projects that need to actually extract/launch these will parse this
convention; document it in a comment on `scanDirectory`).

Link the chosen library in `core/CMakeLists.txt` (`target_link_libraries`)
and, if needed, add a `CMAKE_PREFIX_PATH` entry in `CMakePresets.json`'s
`windows-portable` preset (append with `;`, don't replace the existing
Qt/SDL2 entries — same pattern as sub-project 1's SDL2 addition). Update
`platform/windows/dev-env.ps1` and `platform/windows/publish_windows.ps1`
if the library ships its own runtime DLL that needs to be on PATH /
copied into `dist/` (same pattern as SDL2.dll in sub-project 1) — check
whether the library you chose is header-only/static (no DLL needed) or
dynamic (DLL needed) and handle accordingly.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R RomScannerTest`
Expected: PASS (all cases, old and new).

- [ ] **Step 5: Update `docs/index.md`**

Add a row to the existing (currently empty) table documenting the
archive library chosen, a one-line description, and "Ajouté pour:
sous-projet 2, scan d'archives ROMs".

- [ ] **Step 6: Commit**

```bash
git add core/library core/CMakeLists.txt CMakePresets.json platform/windows/dev-env.ps1 platform/windows/publish_windows.ps1 docs/index.md tests/library/RomScannerTest.cpp
git commit -m "feat: scan ROMs inside zip archives"
```

---

## Task 4: Background thread execution + progress signals

**Files:**
- Create: `core/library/LibraryScanner.h`
- Create: `core/library/LibraryScanner.cpp`
- Test: `tests/library/LibraryScannerTest.cpp`

**Interfaces:**
- Consumes: `RomScanner::scanDirectory` (Tasks 2-3), `RomSourcesStore::sources()`
  (sub-project 1, returns `QVariantList` of `{path, label, enabled}`).
- Produces:
  ```cpp
  class LibraryScanner : public QObject {
      Q_OBJECT
  public:
      explicit LibraryScanner(LibraryDatabase *db, QObject *parent = nullptr);
      // Scans every enabled RomSource in sequence, on a background thread.
      // sources: the QVariantList from RomSourcesStore::sources().
      Q_INVOKABLE void startScan(const QVariantList &sources);

  signals:
      void scanStarted();
      void sourceScanned(const QString &path, int filesFound);
      void scanFinished(int totalFilesFound);
  };
  ```
  Task 6 connects to these signals from `GameListScreen.qml` to show a
  progress indicator. Task 7 calls `startScan` on boot and from the
  "Rescanner" button.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/library/LibraryScannerTest.cpp
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "library/LibraryScanner.h"
#include "storage/LibraryDatabase.h"

class LibraryScannerTest : public QObject {
    Q_OBJECT
private slots:
    void scansAllEnabledSourcesAndEmitsFinished() {
        QTemporaryDir dir1, dir2;
        QFile(dir1.path() + "/Zelda.nes").open(QIODevice::WriteOnly);
        QFile(dir2.path() + "/Mario.sfc").open(QIODevice::WriteOnly);

        QTemporaryDir dbDir;
        LibraryDatabase db(dbDir.path() + "/library.db");
        QVERIFY(db.open());

        LibraryScanner scanner(&db);
        QSignalSpy startedSpy(&scanner, &LibraryScanner::scanStarted);
        QSignalSpy sourceSpy(&scanner, &LibraryScanner::sourceScanned);
        QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);

        QVariantList sources;
        QVariantMap s1; s1["path"] = dir1.path(); s1["label"] = "A"; s1["enabled"] = true;
        QVariantMap s2; s2["path"] = dir2.path(); s2["label"] = "B"; s2["enabled"] = false;
        sources << s1 << s2;

        scanner.startScan(sources);
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(sourceSpy.count(), 1); // only the enabled source
        QCOMPARE(finishedSpy.at(0).at(0).toInt(), 1); // 1 file found total
        QCOMPARE(db.gameCount(), 1);
    }
};

QTEST_MAIN(LibraryScannerTest)
#include "LibraryScannerTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/library/CMakeLists.txt (append)
qt_add_executable(LibraryScannerTest LibraryScannerTest.cpp)
target_link_libraries(LibraryScannerTest PRIVATE Qt6::Test bili-core)
add_test(NAME LibraryScannerTest COMMAND LibraryScannerTest)
```

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R LibraryScannerTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/library/LibraryScanner.h
#pragma once
#include <QObject>
#include <QVariantList>
#include <QThread>
#include "storage/LibraryDatabase.h"

class LibraryScanner : public QObject {
    Q_OBJECT
public:
    explicit LibraryScanner(LibraryDatabase *db, QObject *parent = nullptr);
    Q_INVOKABLE void startScan(const QVariantList &sources);

signals:
    void scanStarted();
    void sourceScanned(const QString &path, int filesFound);
    void scanFinished(int totalFilesFound);

private:
    LibraryDatabase *m_db;
};
```

```cpp
// core/library/LibraryScanner.cpp
#include "LibraryScanner.h"
#include "RomScanner.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

LibraryScanner::LibraryScanner(LibraryDatabase *db, QObject *parent)
    : QObject(parent), m_db(db) {}

void LibraryScanner::startScan(const QVariantList &sources) {
    emit scanStarted();

    auto *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher]() {
        emit scanFinished(watcher->result());
        watcher->deleteLater();
    });

    QFuture<int> future = QtConcurrent::run([this, sources]() -> int {
        int total = 0;
        for (const QVariant &sourceVariant : sources) {
            const QVariantMap source = sourceVariant.toMap();
            if (!source.value("enabled").toBool()) continue;
            const QString path = source.value("path").toString();
            const int found = RomScanner::scanDirectory(path, *m_db);
            total += found;
            emit sourceScanned(path, found);
        }
        return total;
    });
    watcher->setFuture(future);
}
```

**Toolchain note:** `QtConcurrent` requires linking `Qt6::Concurrent` —
add it to `find_package(Qt6 REQUIRED COMPONENTS ...)` in the root
`CMakeLists.txt` and to `bili-core`'s `target_link_libraries` in
`core/CMakeLists.txt`. This runs the scan on Qt's global thread pool
rather than a hand-rolled `QThread` — simpler than replicating
`GamepadBridge`'s manual thread pattern for a one-shot background task
like this (that pattern exists because `GamepadBridge` needed a
long-lived poll loop; this is a single run-to-completion job, exactly
what `QtConcurrent::run` is for). Note this deviates slightly from the
Global Constraints' "same pattern as GamepadBridge" phrasing — the
constraint's intent (never block the UI thread) is satisfied; the
specific mechanism is adapted to fit a one-shot task rather than a
long-lived loop. If this deviation seems wrong once you're implementing,
stop and flag it rather than silently picking one approach.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R LibraryScannerTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add core/library/LibraryScanner.h core/library/LibraryScanner.cpp tests/library/LibraryScannerTest.cpp tests/library/CMakeLists.txt core/CMakeLists.txt CMakeLists.txt
git commit -m "feat: run library scan on a background thread with progress signals"
```

---

## Task 5: `LibraryModel` — QML-facing list model

**Files:**
- Create: `core/library/LibraryModel.h`
- Create: `core/library/LibraryModel.cpp`
- Modify: `core/storage/LibraryDatabase.h`
- Modify: `core/storage/LibraryDatabase.cpp`
- Test: `tests/library/LibraryModelTest.cpp`

**Interfaces:**
- Consumes: `LibraryDatabase` (extended below).
- Produces:
  ```cpp
  // LibraryDatabase addition
  struct GameRow { qint64 id; QString romPath; QString system; QString title; };
  QList<GameRow> allGames() const;

  // New class
  class LibraryModel : public QAbstractListModel {
      Q_OBJECT
  public:
      enum Roles { TitleRole = Qt::UserRole + 1, SystemRole, RomPathRole };
      explicit LibraryModel(LibraryDatabase *db, QObject *parent = nullptr);
      int rowCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role) const override;
      QHash<int, QByteArray> roleNames() const override;
      Q_INVOKABLE void refresh(); // re-queries the database, resets the model
  };
  ```
  Task 6 sets this as `GameListScreen.qml`'s `GridView.model`, using
  `model.title`/`model.system` in its delegate. Task 7 calls
  `refresh()` after a scan finishes.

- [ ] **Step 1: Write the failing test**

```cpp
// tests/library/LibraryModelTest.cpp
#include <QtTest>
#include <QTemporaryDir>
#include "library/LibraryModel.h"
#include "storage/LibraryDatabase.h"

class LibraryModelTest : public QObject {
    Q_OBJECT
private slots:
    void exposesInsertedGamesViaRoles() {
        QTemporaryDir dir;
        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());
        db.insertGame("/roms/a.nes", "nes", "Zelda");

        LibraryModel model(&db);
        QCOMPARE(model.rowCount(), 1);
        QModelIndex idx = model.index(0, 0);
        QCOMPARE(model.data(idx, LibraryModel::TitleRole).toString(), QString("Zelda"));
        QCOMPARE(model.data(idx, LibraryModel::SystemRole).toString(), QString("nes"));
        QCOMPARE(model.data(idx, LibraryModel::RomPathRole).toString(), QString("/roms/a.nes"));
    }

    void refreshPicksUpNewlyInsertedGames() {
        QTemporaryDir dir;
        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());

        LibraryModel model(&db);
        QCOMPARE(model.rowCount(), 0);

        db.insertGame("/roms/a.nes", "nes", "Zelda");
        model.refresh();
        QCOMPARE(model.rowCount(), 1);
    }
};

QTEST_MAIN(LibraryModelTest)
#include "LibraryModelTest.moc"
```

- [ ] **Step 2: Wire test target, run, verify it fails**

```cmake
# tests/library/CMakeLists.txt (append)
qt_add_executable(LibraryModelTest LibraryModelTest.cpp)
target_link_libraries(LibraryModelTest PRIVATE Qt6::Test bili-core)
add_test(NAME LibraryModelTest COMMAND LibraryModelTest)
```

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R LibraryModelTest`
Expected: FAIL (compile error).

- [ ] **Step 3: Write minimal implementation**

```cpp
// core/storage/LibraryDatabase.h (add near the top, before the class, and inside the public section)
struct GameRow {
    qint64 id;
    QString romPath;
    QString system;
    QString title;
};
// ... inside class LibraryDatabase, public section:
QList<GameRow> allGames() const;
```

```cpp
// core/storage/LibraryDatabase.cpp (append)
QList<GameRow> LibraryDatabase::allGames() const {
    QList<GameRow> games;
    QSqlQuery q("SELECT id, rom_path, system, title FROM games ORDER BY title", m_db);
    while (q.next()) {
        games.append({q.value(0).toLongLong(), q.value(1).toString(),
                       q.value(2).toString(), q.value(3).toString()});
    }
    return games;
}
```

```cpp
// core/library/LibraryModel.h
#pragma once
#include <QAbstractListModel>
#include "storage/LibraryDatabase.h"

class LibraryModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { TitleRole = Qt::UserRole + 1, SystemRole, RomPathRole };

    explicit LibraryModel(LibraryDatabase *db, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void refresh();

private:
    LibraryDatabase *m_db;
    QList<GameRow> m_games;
};
```

```cpp
// core/library/LibraryModel.cpp
#include "LibraryModel.h"

LibraryModel::LibraryModel(LibraryDatabase *db, QObject *parent)
    : QAbstractListModel(parent), m_db(db) {
    m_games = m_db->allGames();
}

int LibraryModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_games.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_games.size()) return {};
    const GameRow &game = m_games.at(index.row());
    switch (role) {
        case TitleRole: return game.title;
        case SystemRole: return game.system;
        case RomPathRole: return game.romPath;
        default: return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const {
    return {
        {TitleRole, "title"},
        {SystemRole, "system"},
        {RomPathRole, "romPath"},
    };
}

void LibraryModel::refresh() {
    beginResetModel();
    m_games = m_db->allGames();
    endResetModel();
}
```

Add `core/library/LibraryModel.cpp` to `bili-core`'s sources.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build\windows-portable && ctest --test-dir build\windows-portable -R LibraryModelTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add core/library/LibraryModel.h core/library/LibraryModel.cpp core/storage/LibraryDatabase.h core/storage/LibraryDatabase.cpp tests/library/LibraryModelTest.cpp tests/library/CMakeLists.txt core/CMakeLists.txt
git commit -m "feat: add LibraryModel exposing library.db to QML"
```

---

## Task 6: `GameListScreen.qml` — real game list with focus navigation

**Files:**
- Modify: `ui/screens/GameListScreen.qml`
- Modify: `app/main.cpp`

**Interfaces:**
- Consumes: `LibraryModel` (Task 5, exposed as a QML context property
  named `LibraryModel`), `LibraryScanner` (Task 4, exposed as
  `LibraryScanner`), the existing `Theme.focusBorderColor`/
  `focusBorderWidth`/`focusRadius` tokens and `KeyNavigation` pattern
  from sub-project 1.
- Produces: nothing new for later tasks — this is a leaf UI screen.

- [ ] **Step 1: Wire `LibraryModel`/`LibraryScanner` into `app/main.cpp`**

```cpp
// app/main.cpp (add near the other service instantiations)
#include "library/LibraryModel.h"
#include "library/LibraryScanner.h"
// ...
LibraryModel libraryModel(&libraryDb); // libraryDb already constructed in sub-project 1's Task 4 fix
engine.rootContext()->setContextProperty("LibraryModel", &libraryModel);

LibraryScanner libraryScanner(&libraryDb);
engine.rootContext()->setContextProperty("LibraryScanner", &libraryScanner);

QObject::connect(&libraryScanner, &LibraryScanner::scanFinished,
                  &libraryModel, &LibraryModel::refresh);
```

(This assumes `libraryDb` already exists in `main.cpp` from sub-project
1's final fix wave, which wired `LibraryDatabase` construction+`open()`
but had no QML exposure yet — read the current `app/main.cpp` first to
confirm the exact variable name and insert these lines after it, rather
than assuming the exact surrounding line numbers.)

- [ ] **Step 2: Write `GameListScreen.qml`**

```qml
// ui/screens/GameListScreen.qml
import QtQuick
import QtQuick.Controls
import Bili

Rectangle {
    anchors.fill: parent
    color: Theme.colorBackground

    Text {
        id: statusText
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: Theme.spacingUnit
        color: Theme.colorText
        font.pixelSize: Theme.fontSizeBody
        visible: text.length > 0
    }

    Text {
        anchors.centerIn: parent
        text: "Aucun jeu trouvé — vérifie tes dossiers ROMs dans Réglages."
        color: Theme.colorText
        font.pixelSize: Theme.fontSizeBody
        visible: gameGrid.count === 0 && !statusText.visible
    }

    GridView {
        id: gameGrid
        anchors.fill: parent
        anchors.topMargin: Theme.spacingUnit * 4
        anchors.margins: Theme.spacingUnit * 2
        cellWidth: 200
        cellHeight: 60
        model: LibraryModel
        delegate: Rectangle {
            id: gameDelegate
            width: gameGrid.cellWidth - Theme.spacingUnit
            height: gameGrid.cellHeight - Theme.spacingUnit
            color: gameDelegate.activeFocus ? Theme.focusBorderColor : "#22222a"
            border.color: Theme.focusBorderColor
            border.width: gameDelegate.activeFocus ? Theme.focusBorderWidth : 0
            radius: Theme.focusRadius

            Text {
                anchors.centerIn: parent
                anchors.margins: Theme.spacingUnit
                text: model.title
                color: Theme.colorText
                font.pixelSize: Theme.fontSizeBody
                elide: Text.ElideRight
                width: parent.width - Theme.spacingUnit * 2
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Connections {
        target: LibraryScanner
        function onScanStarted() { statusText.text = "Scan en cours..." }
        function onSourceScanned(path, filesFound) {
            statusText.text = "Scan en cours... (" + filesFound + " trouvés dans " + path + ")"
        }
        function onScanFinished(totalFilesFound) {
            statusText.text = ""
        }
    }
}
```

`GridView` has built-in keyboard-arrow navigation between cells when it
has focus (same built-in behavior `SettingsScreen`'s `ListView` already
relies on from sub-project 1's focus-navigation work) — give it initial
focus:

```qml
    GridView {
        id: gameGrid
        // ...
        focus: true
        keyNavigationEnabled: true
        highlightFollowsCurrentItem: true
        Component.onCompleted: forceActiveFocus()
```

(Merge this into the `GridView` block above rather than duplicating it —
shown separately here only to call out that these four lines are
required, matching the exact pattern `SettingsScreen.qml`'s `romList`
already uses from the prior focus-navigation task.)

- [ ] **Step 3: Build and manually verify**

Run: `cmake --build build\windows-portable`, then launch
`build\windows-portable\Bili.exe` (dot-source `dev-env.ps1` first) with
at least one ROM source folder containing a few real or dummy files with
recognized extensions (e.g. touch empty `.nes`/`.sfc` files — content
doesn't matter for detection, only the extension).

Expected: on reaching `GameList`, a brief "Scan en cours..." status
appears then clears, and the grid populates with one cell per detected
file, titled with the cleaned filename. Arrow keys move focus between
cells (visible highlight, matching the established focus-styling
pattern). No games found shows the "Aucun jeu trouvé" message instead.

- [ ] **Step 4: Commit**

```bash
git add ui/screens/GameListScreen.qml app/main.cpp
git commit -m "feat: wire GameListScreen to real library data with focus navigation"
```

---

## Task 7: Trigger wiring — auto-scan + manual "Rescanner" button

**Files:**
- Modify: `app/main.cpp`
- Modify: `ui/screens/SettingsScreen.qml`

**Interfaces:**
- Consumes: `LibraryScanner::startScan` (Task 4), `RomSourcesStore::sources()`
  (sub-project 1), `ScreenManager.currentScreen` (sub-project 1).

- [ ] **Step 1: Trigger a scan when `GameList` is first reached**

The simplest, most robust trigger point is a `Connections` block on
`ScreenManager` watching for the transition onto `"GameList"`, placed in
`ui/Main.qml` (which already has a `Connections { target: InputManager }`
block from the focus-navigation task — add a second, separate
`Connections` block for `ScreenManager` rather than overloading the
existing one):

```qml
// ui/Main.qml (add near the existing InputManager Connections block)
Connections {
    target: ScreenManager
    function onCurrentScreenChanged() {
        if (ScreenManager.currentScreen === "GameList") {
            LibraryScanner.startScan(RomSourcesStore.sources())
        }
    }
}
```

This fires every time `GameList` is (re-)entered, not just the first
time — acceptable and actually desirable (re-entering the library
re-syncs it cheaply via the incremental scan from Task 2, so newly added
files show up without a manual rescan if the user tabs away and back).

- [ ] **Step 2: Add the "Rescanner" button to `SettingsScreen.qml`**

```qml
// ui/screens/SettingsScreen.qml (add near the existing "Ajouter un dossier" button)
Button {
    id: rescanButton
    text: "Rescanner la bibliothèque"
    KeyNavigation.up: addFolderButton
    background: Rectangle {
        color: rescanButton.activeFocus ? Theme.focusBorderColor : "transparent"
        border.color: Theme.focusBorderColor
        border.width: rescanButton.activeFocus ? Theme.focusBorderWidth : 0
        radius: Theme.focusRadius
    }
    onClicked: LibraryScanner.startScan(RomSourcesStore.sources())
}
```

(Match this against the actual current `SettingsScreen.qml` content —
read the file first to get `addFolderButton`'s real `id` and the exact
focus-styling pattern already used there from the prior task, and place
this new button consistently within the existing layout rather than
guessing its surrounding structure.)

- [ ] **Step 3: Build and manually verify**

Launch the app, confirm a scan runs automatically on first reaching
GameList (per Task 6's verification), navigate to Settings, click
"Rescanner la bibliothèque" (or focus it via keyboard/gamepad and press
Enter/A), confirm the same "Scan en cours..." feedback appears on
GameList when you navigate back to it (or immediately if you're still
watching GameList's status text — check whichever is actually visible
given the two screens are different `Loader` instances).

- [ ] **Step 4: Commit**

```bash
git add ui/Main.qml ui/screens/SettingsScreen.qml
git commit -m "feat: trigger library scan on GameList entry and via Rescanner button"
```

---

## Vérification (end-to-end, for the final whole-branch review)

- A test ROM folder with files across several recognized systems (raw
  files + at least one `.zip`) all appear correctly titled and
  system-tagged in `GameListScreen` after launch.
- Deleting a file from a configured ROM folder and re-entering
  `GameList` removes it from the grid (incremental sync, not just
  additive).
- The "Rescanner" button in Settings re-triggers a scan on demand.
- Keyboard and gamepad arrow navigation both move focus visibly between
  game cells, matching the established `Theme.focus*` styling from
  sub-project 1.
- All `ctest` suites pass, including the new `RomScannerTest`/
  `LibraryScannerTest`/`LibraryModelTest` targets.
- `docs/index.md` has a real entry (not empty) documenting whatever
  archive-reading library Task 3 chose.
