// SPDX-License-Identifier: GPL-2.0-or-later
// GUI smoke tests: drive the real widgets with synthetic events.
#include <QtTest>
#include <QComboBox>
#include <QDir>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <cmath>

#include "core/Presets.h"
#include "grace/Interpreter.h"
#include "plot/Fonts.h"
#include "ui/Dialogs.h"
#include "ui/Document.h"
#include "ui/Inspector.h"
#include "ui/MainWindow.h"
#include "ui/Panels.h"
#include "ui/PlotView.h"
#include "ui/Theme.h"

using namespace cactus;
using namespace cactus::ui;

class TestGui : public QObject {
    Q_OBJECT

    static Project demo()
    {
        Project p = presets::newProject();
        Interpreter in(&p);
        in.execute(QStringLiteral("s0.x = mesh(0, 10, 101); s0.y = 2*exp(-(s0.x-5)^2/2) + 0.5"));
        p.graphs[0].sets[0].legend = QStringLiteral("peak");
        p.graphs[0].autoscale();
        return p;
    }

private slots:
    void initTestCase()
    {
        fonts::registerBundledFonts();
        theme::apply(*qApp, ThemeMode::Light, false);
    }

    void wheelZoomAndUndo()
    {
        MainWindow w;
        w.setProject(demo(), QString());
        w.resize(1200, 800);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto* view = w.findChild<PlotView*>();
        QVERIFY(view);
        Document* doc = w.document();
        const double before = doc->project().graphs[0].wxmax - doc->project().graphs[0].wxmin;
        const QPointF center(view->width() / 2.0, view->height() / 2.0);
        QWheelEvent wheel(center, view->mapToGlobal(center), QPoint(), QPoint(0, 240), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(view, &wheel);
        const double after = doc->project().graphs[0].wxmax - doc->project().graphs[0].wxmin;
        QVERIFY2(after < before * 0.9, qPrintable(QStringLiteral("%1 -> %2").arg(before).arg(after)));
        doc->undoStack()->undo();
        QCOMPARE(doc->project().graphs[0].wxmax - doc->project().graphs[0].wxmin, before);
    }

    void clickSelectsCurve()
    {
        MainWindow w;
        w.setProject(demo(), QString());
        w.resize(1200, 800);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto* view = w.findChild<PlotView*>();
        Document* doc = w.document();
        view->repaint();
        // locate the peak of the curve in widget coordinates
        const Graph& g = doc->project().graphs[0];
        const GraphGeometry geo(doc->project().page, g);
        const QPointF pagePt = geo.map(5.0, 2.5);
        const Page& pg = doc->project().page;
        const double margin = 28;
        const double s = std::min((view->width() - 2 * margin) / pg.width, (view->height() - 2 * margin) / pg.height);
        const QPointF origin((view->width() - pg.width * s) / 2, (view->height() - pg.height * s) / 2);
        const QPoint wpt = (origin + pagePt * s).toPoint();
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, wpt);
        QCOMPARE(int(doc->selection().kind), int(Selection::Set));
        QCOMPARE(doc->selection().index, 0);
    }

    void consoleCommand()
    {
        MainWindow w;
        w.setProject(demo(), QString());
        auto* console = w.findChild<Console*>();
        QVERIFY(console);
        const double y0 = w.document()->project().graphs[0].sets[0].cols[1][50];
        console->runCommand(QStringLiteral("s0.y = s0.y * 2"));
        QCOMPARE(w.document()->project().graphs[0].sets[0].cols[1][50], 2 * y0);
        QVERIFY(w.document()->isModified());
    }

    void transformDialog()
    {
        Document doc;
        doc.setProject(demo());
        doc.select(Selection{Selection::Set, 0, 0});
        TransformDialog d(&doc);
        d.showPage(QObject::tr("Running statistics"));
        const auto apply = d.findChildren<QPushButton*>();
        QPushButton* btn = nullptr;
        for (QPushButton* b : apply) if (b->text().contains(QStringLiteral("Apply"))) btn = b;
        QVERIFY(btn);
        btn->click();
        QVERIFY(doc.project().graphs[0].set(1));
        QCOMPARE(doc.project().graphs[0].set(1)->size(), 97);
    }

    void importText()
    {
        Document doc;
        doc.setProject(presets::newProject());
        ImportDialog d(&doc, QString(), QStringLiteral("x a b\n1 2 3\n2 4 9\n3 6 27\n"));
        auto* mode = d.findChildren<QComboBox*>().value(0);
        QVERIFY(mode);
        mode->setCurrentIndex(1);  // NXY
        QMetaObject::invokeMethod(&d, "accept");
        QCOMPARE(doc.project().graphs[0].activeSetCount(), 2);
        QCOMPARE(doc.project().graphs[0].sets[1].cols[1][2], 27.0);
    }

    void openXvgWithErrorBars()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("average.xvg"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# GROMACS\n@    title \"Average\"\n@TYPE xydy\n0 1 0.1\n1 2 0.2\n2 3 0.3\n");
        f.close();
        MainWindow w;
        w.setProject(presets::newProject(), QString());
        QVERIFY(w.openFile(path));  // no import dialog
        const Graph& g = w.document()->project().graphs[0];
        QCOMPARE(g.activeSetCount(), 1);
        QCOMPARE(g.sets[0].type, SetType::XYDY);
        QCOMPARE(g.title.text, QStringLiteral("Average"));
        // File > Import offers the file's type too
        QFile in(path);
        QVERIFY(in.open(QIODevice::ReadOnly));
        ImportDialog d(w.document(), path, QString::fromUtf8(in.readAll()));
        QCOMPARE(d.findChildren<QComboBox*>().value(1)->currentIndex(), int(SetType::XYDY));
    }

    void openWithoutErrorBars()
    {
        // files with and without error bars both open, as in Grace: the type follows "@TYPE",
        // plain files are XY (the import dialog suggests XYDY for three columns)
        QTemporaryDir dir;
        struct Case { const char* name; const char* text; SetType type; int sets; };
        const Case cases[] = {
            {"rmsd.xvg", "# GROMACS\n@    title \"RMSD\"\n@TYPE xy\n@ subtitle \"Backbone\"\n0 0.1\n1 0.2\n2 0.25\n", SetType::XY, 1},
            {"plain.xvg", "0 1\n1 2\n2 4\n", SetType::XY, 1},
            {"gyrate.xvg", "@TYPE xy\n@ s0 legend \"Rg\"\n@ s1 legend \"Rg\\sX\\N\"\n0 1 2\n1 2 3\n", SetType::XY, 2},
            {"avg.xvg", "@TYPE xydy\n0 1 0.1\n1 2 0.2\n", SetType::XYDY, 1},
            {"series.dat", "# t a b\n0 1 5\n1 2 6\n2 3 7\n", SetType::XY, 2},
            {"counts.dat", "# time counts error\n0 98 4\n1 61 3\n2 38 3\n", SetType::XYDY, 1},
            {"temperature.csv", "time,temperature\n2024-01-01 00:00:00,5.1\n2024-01-01 01:00:00,4.8\n", SetType::XY, 1},
        };
        for (const Case& c : cases) {
            const QString path = dir.filePath(QString::fromLatin1(c.name));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(c.text);
            f.close();
            MainWindow w;
            w.setProject(presets::newProject(), QString());
            QVERIFY2(w.openFile(path), c.name);
            const Graph& g = w.document()->project().graphs[0];
            QCOMPARE(g.activeSetCount(), c.sets);
            QCOMPARE(g.sets[0].type, c.type);
        }
        // the import dialog: no error bars unless the file declares them; extra columns are more curves
        const QList<QPair<QString, int>> texts{{QStringLiteral("# t y\n0 1\n1 2\n2 3\n"), 1},
                                               {QStringLiteral("time,value\n0,1\n1,2\n"), 1},
                                               {QStringLiteral("@TYPE xy\n0 1\n1 2\n"), 1},
                                               {QStringLiteral("0 1 5\n1 2 6\n"), 2}};
        for (const auto& t : texts) {
            Document doc;
            doc.setProject(presets::newProject());
            ImportDialog d(&doc, QStringLiteral("x.dat"), t.first);
            QMetaObject::invokeMethod(&d, "accept");
            QCOMPARE(doc.project().graphs[0].activeSetCount(), t.second);
            QCOMPARE(doc.project().graphs[0].sets[0].type, SetType::XY);
        }
    }

    void inspectorBuilds()
    {
        Document doc;
        doc.setProject(demo());
        Inspector ins(&doc);
        for (auto k : {Selection::Page, Selection::Graph, Selection::Axis, Selection::Set, Selection::Legend})
            doc.select(Selection{k, 0, 0});
        doc.modify(QStringLiteral("add text"), [](Project& p) {
            DrawObject o;
            o.text = QStringLiteral("hello");
            p.objects.push_back(o);
        });
        doc.select(Selection{Selection::Object, 0, 0});
        ins.refresh();
        QVERIFY(true);
    }

    void screenshotRenders()
    {
        MainWindow w;
        w.setProject(demo(), QString());
        w.resize(1000, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const QImage img = w.grab().toImage();
        QVERIFY(img.width() >= 1000);
    }
};

QTEST_MAIN(TestGui)
#include "test_gui.moc"
