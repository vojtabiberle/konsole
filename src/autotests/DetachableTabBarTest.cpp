/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../widgets/DetachableTabBar.h"
#include "../widgets/ViewContainer.h"
#include "../widgets/ViewSplitter.h"
#include "KonsoleSettings.h"

#include <QSignalSpy>
#include <QStyleOptionTab>
#include <QTemporaryDir>
#include <QTest>

class TestTabBar : public Konsole::DetachableTabBar
{
public:
    using DetachableTabBar::initStyleOption;
};

class DetachableTabBarTest : public QObject
{
    Q_OBJECT
    QTemporaryDir _configDir;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(_configDir.isValid());
        qputenv("XDG_CONFIG_HOME", _configDir.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    }

    void resizeSidebar_data()
    {
        QTest::addColumn<int>("side");
        QTest::newRow("left") << int(QTabWidget::West);
        QTest::newRow("right") << int(QTabWidget::East);
    }

    void resizeSidebar()
    {
        QFETCH(int, side);
        Konsole::KonsoleSettings::setTabBarPosition(side);
        Konsole::KonsoleSettings::setTabBarVisibility(Konsole::KonsoleSettings::AlwaysShowTabBar);
        Konsole::KonsoleSettings::setSideTabBarWidth(220);
        Konsole::TabbedViewContainer container(nullptr, nullptr);
        container.addTab(new Konsole::ViewSplitter, QStringLiteral("Terminal"));
        container.resize(900, 400);
        container.show();
        QVERIFY(QTest::qWaitForWindowExposed(&container));
        auto *bar = container.findChild<Konsole::DetachableTabBar *>();
        auto *handle = container.findChild<QWidget *>(QStringLiteral("sidebarResizeHandle"));
        QVERIFY(bar);
        QVERIFY(handle);
        QTRY_COMPARE(bar->tabRect(0).width(), 220);
        QVERIFY(handle->isVisible());
        QCOMPARE(handle->height(), container.height());
        const int direction = side == QTabWidget::West ? 1 : -1;
        const QPoint press = handle->rect().center();
        QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(handle, press + QPoint(direction * 100, 0));
        QTest::mouseRelease(handle, Qt::LeftButton);
        QTRY_COMPARE(bar->tabRect(0).width(), 320);
        Konsole::KonsoleSettings::self()->load();
        QCOMPARE(Konsole::KonsoleSettings::sideTabBarWidth(), 320);

        // A new container restores the persisted width.
        Konsole::TabbedViewContainer restored(nullptr, nullptr);
        restored.addTab(new Konsole::ViewSplitter, QStringLiteral("Restored terminal"));
        restored.resize(900, 400);
        restored.show();
        auto *restoredBar = restored.findChild<Konsole::DetachableTabBar *>();
        QTRY_COMPARE(restoredBar->tabRect(0).width(), 320);

        QTest::keyClick(handle, side == QTabWidget::West ? Qt::Key_Right : Qt::Key_Left);
        QTRY_COMPARE(bar->tabRect(0).width(), 330);
        container.resize(500, 400);
        QTRY_VERIFY(bar->tabRect(0).width() <= container.width() / 2);
        container.resize(900, 400);
        QTRY_COMPARE(bar->tabRect(0).width(), 330);
        QTest::mouseDClick(handle, Qt::LeftButton);
        QTRY_COMPARE(Konsole::KonsoleSettings::sideTabBarWidth(), 0);
        QTRY_VERIFY(bar->width() < 220);
        bar->hide();
        QTRY_VERIFY(!handle->isVisible());
        bar->show();
        QTRY_VERIFY(handle->isVisible());
        container.setTabPosition(QTabWidget::North);
        QTRY_VERIFY(!handle->isVisible());
    }

    void selectionPalette_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("rtl");
        QTest::newRow("light-ltr") << false << false;
        QTest::newRow("dark-ltr") << true << false;
        QTest::newRow("light-rtl") << false << true;
        QTest::newRow("dark-rtl") << true << true;
    }

    void selectionPalette()
    {
        QFETCH(bool, dark);
        QFETCH(bool, rtl);
        TestTabBar bar;
        bar.setShape(QTabBar::RoundedWest);
        bar.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        bar.setExpanding(false);
        bar.addTab(QStringLiteral("First terminal"));
        bar.addTab(QStringLiteral("Second terminal"));
        QPalette palette = bar.palette();
        palette.setColor(QPalette::Window, dark ? QColor(35, 38, 41) : QColor(239, 240, 241));
        palette.setColor(QPalette::WindowText, dark ? Qt::white : Qt::black);
        palette.setColor(QPalette::Highlight, QColor(61, 174, 233));
        bar.setPalette(palette);
        bar.resize(bar.sizeHint());
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));

        for (int selected = 0; selected < 2; ++selected) {
            bar.setCurrentIndex(selected);
            // Changing the palette must affect an already visible tab bar.
            palette.setColor(QPalette::Highlight, selected == 0 ? QColor(61, 174, 233) : QColor(180, 95, 205));
            bar.setPalette(palette);
            const QImage image = bar.grab().toImage().scaled(bar.size());
            const QRect rect = bar.tabRect(selected);
            const QPoint marker(rect.left() + 1, rect.center().y());
            QCOMPARE(image.pixelColor(marker), palette.color(QPalette::Highlight));
            const QColor background = image.pixelColor(rect.center().x(), rect.top() + 3);
            QVERIFY(background != palette.color(QPalette::Window));
            QVERIFY(background != palette.color(QPalette::Highlight));
            const QRect other = bar.tabRect(1 - selected);
            QVERIFY(image.pixelColor(marker.x(), other.center().y()) != palette.color(QPalette::Highlight));
        }
    }

    void layout_data()
    {
        QTest::addColumn<int>("shape");
        QTest::addColumn<bool>("rtl");
        for (int shape = QTabBar::RoundedNorth; shape <= QTabBar::TriangularEast; ++shape) {
            for (bool rtl : {false, true}) {
                QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(shape).arg(rtl))) << shape << rtl;
            }
        }
    }

    void layout()
    {
        QFETCH(int, shape);
        QFETCH(bool, rtl);
        TestTabBar bar;
        bar.setShape(static_cast<QTabBar::Shape>(shape));
        bar.setDocumentMode(true);
        bar.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        bar.setExpanding(false);
        bar.setTabsClosable(true);
        const QIcon icon = bar.style()->standardIcon(QStyle::SP_DirIcon);
        bar.addTab(icon, QStringLiteral("First terminal"));
        bar.addTab(icon, QStringLiteral("Second terminal"));
        const bool vertical =
            shape == QTabBar::RoundedWest || shape == QTabBar::RoundedEast || shape == QTabBar::TriangularWest || shape == QTabBar::TriangularEast;
        bar.resize(vertical ? QSize(bar.sizeHint().width(), 400) : QSize(600, bar.sizeHint().height()));
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));

        for (int i = 0; i < bar.count(); ++i) {
            QStyleOptionTab option;
            bar.initStyleOption(&option, i);
            QVERIFY(option.shape != QTabBar::RoundedWest && option.shape != QTabBar::RoundedEast && option.shape != QTabBar::TriangularWest
                    && option.shape != QTabBar::TriangularEast);
            QCOMPARE(option.text, bar.tabText(i));
            QVERIFY(bar.tabRect(i).width() > bar.tabRect(i).height());
            QWidget *button = bar.tabButton(i, QTabBar::RightSide);
            if (!button) {
                button = bar.tabButton(i, QTabBar::LeftSide);
            }
            QVERIFY(button);
            QVERIFY(bar.tabRect(i).contains(button->geometry()));
            const QRect textRect = bar.style()->subElementRect(QStyle::SE_TabBarTabText, &option, &bar);
            QVERIFY(!textRect.intersects(button->geometry()));
        }
        if (vertical) {
            QVERIFY(bar.tabRect(1).top() >= bar.tabRect(0).bottom());
        }
        QTest::mouseClick(&bar, Qt::LeftButton, Qt::NoModifier, bar.tabRect(1).center());
        QCOMPARE(bar.currentIndex(), 1);
        QSignalSpy closeSpy(&bar, &QTabBar::tabCloseRequested);
        QWidget *button = bar.tabButton(1, QTabBar::RightSide);
        if (!button) {
            button = bar.tabButton(1, QTabBar::LeftSide);
        }
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(closeSpy.count(), 1);
        QCOMPARE(closeSpy.first().first().toInt(), 1);
    }

    void overflow()
    {
        TestTabBar bar;
        bar.setShape(QTabBar::RoundedWest);
        bar.setExpanding(false);
        bar.setUsesScrollButtons(true);
        bar.addTab(QString(200, QLatin1Char('W')) + QStringLiteral(" suffix"));
        for (int i = 1; i < 20; ++i) {
            bar.addTab(QStringLiteral("Terminal %1").arg(i));
        }
        bar.resize(bar.sizeHint().width(), 180);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        QVERIFY(bar.width() <= bar.fontMetrics().averageCharWidth() * 30);
        QStyleOptionTab option;
        bar.initStyleOption(&option, 0);
        QVERIFY(option.text.size() < bar.tabText(0).size());
        QVERIFY(option.text.endsWith(QStringLiteral(" suffix")));
        QCOMPARE(bar.tabRect(0).height(), bar.tabRect(1).height());
        bar.setCurrentIndex(19);
        QVERIFY(bar.rect().contains(bar.tabRect(19).center()));
        bar.moveTab(19, 0);
        QCOMPARE(bar.tabText(0), QStringLiteral("Terminal 19"));
        QCOMPARE(bar.currentIndex(), 0);
    }

    void indicators()
    {
        TestTabBar bar;
        bar.setShape(QTabBar::RoundedEast);
        bar.setExpanding(false);
        bar.addTab(QStringLiteral("First terminal"));
        bar.addTab(QStringLiteral("Second terminal"));
        bar.addTab(QStringLiteral("Hidden terminal"));
        bar.setColor(0, Qt::red);
        bar.setColor(1, Qt::green);
        bar.setProgress(1, 50);
        bar.setColor(2, Qt::blue);
        bar.setTabVisible(2, false);
        bar.resize(bar.sizeHint());
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        const QImage image = bar.grab().toImage().scaled(bar.size());
        const int indicatorY = bar.fontMetrics().height() + 7;
        QCOMPARE(image.pixelColor(bar.tabRect(0).topLeft() + QPoint(8, indicatorY)), QColor(Qt::red));
        QCOMPARE(image.pixelColor(bar.tabRect(1).topLeft() + QPoint(8, indicatorY)), QColor(Qt::green));
        QVERIFY(image.pixelColor(bar.tabRect(1).topRight() + QPoint(-8, indicatorY)) != QColor(Qt::green));
    }

    void stylesheet()
    {
        TestTabBar bar;
        bar.setShape(QTabBar::RoundedWest);
        bar.setExpanding(false);
        bar.setStyleSheet(QStringLiteral("QTabBar::tab { min-width: 180px; max-width: 180px; padding: 4px; }"));
        bar.addTab(QStringLiteral("Terminal"));
        auto *button = new QWidget;
        button->setFixedSize(48, 18);
        bar.setTabButton(0, QTabBar::RightSide, button);
        bar.resize(bar.sizeHint());
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        QVERIFY(bar.tabRect(0).width() >= 180);
        QVERIFY(bar.tabRect(0).height() < 100);
        QVERIFY(bar.tabRect(0).contains(button->geometry()));
        QStyleOptionTab option;
        bar.initStyleOption(&option, 0);
        QCOMPARE(option.text, bar.tabText(0));
        const QRect textRect = bar.style()->subElementRect(QStyle::SE_TabBarTabText, &option, &bar);
        QVERIFY(!textRect.intersects(button->geometry()));
    }

    void paintingDoesNotCoverOtherTabs()
    {
        TestTabBar bar;
        bar.setShape(QTabBar::RoundedWest);
        bar.setExpanding(false);
        bar.setDocumentMode(true);
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::magenta);
        for (int i = 0; i < 3; ++i) {
            bar.addTab(QIcon(pixmap), QStringLiteral("Terminal %1").arg(i));
        }
        bar.resize(bar.sizeHint());
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        for (int selected = 0; selected < bar.count(); ++selected) {
            bar.setCurrentIndex(selected);
            const QImage image = bar.grab().toImage().scaled(bar.size());
            for (int i = 0; i < bar.count(); ++i) {
                const QRect rect = bar.tabRect(i);
                int iconPixels = 0;
                for (int y = rect.top(); y <= rect.bottom(); ++y) {
                    for (int x = rect.left(); x <= rect.right(); ++x) {
                        iconPixels += image.pixelColor(x, y) == QColor(Qt::magenta);
                    }
                }
                QVERIFY2(iconPixels > 0, "A tab's icon was covered by another tab's background");
            }
        }
    }
};

QTEST_MAIN(DetachableTabBarTest)

#include "DetachableTabBarTest.moc"
