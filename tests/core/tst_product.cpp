// SPDX-License-Identifier: GPL-2.0-or-later
#include "NoteStore.h"
#include "Product.h"
#include "TestHome.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTest>

using namespace Gooseberry;

// The name the person sees: Notes where Shuffle is installed, Gooseberry
// elsewhere, and nothing kept on disk follows it.
class ProductTest : public QObject
{
    Q_OBJECT

public:
    explicit ProductTest(TestHome *home)
        : m_home(home)
    {
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Only the test's home is searched, so an installed Shuffle on the
        // computer running the test is not seen.
        qputenv("XDG_DATA_DIRS", QFile::encodeName(m_home->path() + QStringLiteral("/system")));
    }

    void cleanup()
    {
        setInsideShuffle(false);
    }

    void nameFollowsShuffle()
    {
        QVERIFY(!insideShuffle());
        QCOMPARE(productName(), QStringLiteral("Gooseberry"));
        setInsideShuffle(true);
        QCOMPARE(productName(), QStringLiteral("Notes"));
        setInsideShuffle(false);
        QCOMPARE(productName(), QStringLiteral("Gooseberry"));
    }

    void shuffleIsFoundByItsBottomSurface()
    {
        QVERIFY(!shuffleInstalled());
        const QString surface = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/plasma/plasmoids/studio.warbler.shuffle.bottomsurface");
        QVERIFY(QDir().mkpath(surface));
        QVERIFY(shuffleInstalled());
        QVERIFY(QDir(surface).removeRecursively());
        QVERIFY(!shuffleInstalled());
    }

    void messagesUseTheNameButTheFolderDoesNot()
    {
        setInsideShuffle(true);
        QVERIFY(NoteStore::defaultFolder().endsWith(QStringLiteral("/Gooseberry")));
        const QString folder = m_home->notesFolder();
        QVERIFY(QDir().mkpath(folder));
        QFile marker(folder + QStringLiteral("/.gooseberry"));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("format: 2\n");
        marker.close();
        NoteStore store(folder);
        QVERIFY(store.open());
        QVERIFY(store.readOnly());
        QVERIFY(!store.trash(QStringLiteral("anything")).has_value());
        QVERIFY(store.lastError().contains(QStringLiteral("newer Notes")));
        QVERIFY(!store.lastError().contains(QStringLiteral("Gooseberry")));
    }

private:
    TestHome *m_home;
};

int main(int argc, char *argv[])
{
    TestHome home;
    QCoreApplication app(argc, argv);
    ProductTest test(&home);
    return QTest::qExec(&test, argc, argv);
}

#include "tst_product.moc"
