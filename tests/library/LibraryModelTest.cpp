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
