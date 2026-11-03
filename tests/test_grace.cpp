// SPDX-License-Identifier: GPL-2.0-or-later
#include <QtTest>
#include <QTemporaryDir>
#include <QUrl>
#include <cmath>

#include "app/Cli.h"
#include "core/Presets.h"
#include "core/color/Colors.h"
#include "grace/Interpreter.h"
#include "grace/ProjectIO.h"
#include "io/DataReader.h"
#include "plot/Fonts.h"
#include "render/Export.h"
#include "render/PlotRenderer.h"

using namespace cactus;

static const char* kAgr = R"(# Grace project file
#
@version 50125
@page size 792, 612
@map font 0 to "Times-Roman", "Times-Roman"
@map font 4 to "Helvetica", "Helvetica"
@map color 0 to (255, 255, 255), "white"
@map color 1 to (0, 0, 0), "black"
@map color 2 to (255, 0, 0), "red"
@map color 4 to (0, 0, 255), "blue"
@with string
@    string on
@    string loctype view
@    string 0.3, 0.4
@    string color 2
@    string font 4
@    string just 2
@    string char size 1.200000
@    string def "Hello \xa\f{} world"
@g0 on
@g0 hidden false
@g0 type XY
@with g0
@    world 0, -1, 10, 1
@    view 0.150000, 0.150000, 1.150000, 0.850000
@    title "Title"
@    xaxes scale Normal
@    yaxes scale Logarithmic
@    xaxis  label "Time"
@    xaxis  tick major 2
@    xaxis  tick minor ticks 3
@    xaxis  ticklabel format decimal
@    xaxis  ticklabel prec 1
@    legend 0.5, 0.8
@    s0 hidden false
@    s0 type xydy
@    s0 symbol 2
@    s0 symbol size 0.7
@    s0 line type 2
@    s0 line color 4
@    s0 line linewidth 2.5
@    s0 fill type 2
@    s0 legend  "Data \x a"
@target G0.S0
@type xydy
1 2 0.1
2 3 0.2 "lbl"
3 5 0.3
&
)";

class TestGrace : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { fonts::registerBundledFonts(); }

    void parseAgr()
    {
        Project p;
        QVERIFY(projectio::loadText(QString::fromLatin1(kAgr), &p, QStringLiteral("t.agr")));
        const Graph* g = p.graph(0);
        QVERIFY(g);
        QCOMPARE(g->wxmin, 0.0);
        QCOMPARE(g->wymax, 1.0);
        QCOMPARE(g->vxmax, 1.15);
        QCOMPARE(g->yscale, ScaleType::Log);
        QCOMPARE(g->title.text, QStringLiteral("Title"));
        QCOMPARE(g->axes[0].label, QStringLiteral("Time"));
        QCOMPARE(g->axes[0].major, 2.0);
        QCOMPARE(g->axes[0].minorTicks, 3);
        QCOMPARE(g->axes[0].format, LabelFormat::Decimal);
        QCOMPARE(g->legend.pos, QPointF(0.5, 0.8));
        const DataSet* s = g->set(0);
        QVERIFY(s);
        QCOMPARE(s->type, SetType::XYDY);
        QCOMPARE(s->size(), 3);
        QCOMPARE(s->cols[2][1], 0.2);
        QCOMPARE(s->strings.value(1), QStringLiteral("lbl"));
        QCOMPARE(s->symbol.type, Symbol::Square);
        QCOMPARE(s->lineType, LineType::LeftStairs);
        QCOMPARE(s->line.color, QColor(0, 0, 255));
        QCOMPARE(s->line.width, 2.5);
        QCOMPARE(s->fillType, FillType::ToBaseline);
        QCOMPARE(p.objects.size(), 1);
        QCOMPARE(p.objects[0].text, QStringLiteral("Hello \\xa\\f{} world"));
        QCOMPARE(p.objects[0].textStyle.color, QColor(255, 0, 0));
        QCOMPARE(p.objects[0].textStyle.font, QStringLiteral("Helvetica"));
    }

    void roundTripGrace()
    {
        Project p;
        QVERIFY(projectio::loadText(QString::fromLatin1(kAgr), &p, QStringLiteral("t.agr")));
        const QString text = projectio::write(p, projectio::Dialect::Grace);
        QVERIFY(text.startsWith(QStringLiteral("# Grace project file")));
        Project q;
        QVERIFY(projectio::loadText(text, &q, QStringLiteral("rt.agr")));
        const Graph* a = p.graph(0);
        const Graph* b = q.graph(0);
        QVERIFY(a && b);
        QCOMPARE(b->wxmax, a->wxmax);
        QCOMPARE(b->yscale, a->yscale);
        QCOMPARE(b->axes[0].label, a->axes[0].label);
        QCOMPARE(b->set(0)->cols[1], a->set(0)->cols[1]);
        QCOMPARE(b->set(0)->line.color, a->set(0)->line.color);
        QCOMPARE(b->set(0)->legend, a->set(0)->legend);
        QCOMPARE(q.objects.size(), p.objects.size());
        QCOMPARE(q.objects[0].pos, p.objects[0].pos);
    }

    void roundTripNative()
    {
        Project p = presets::newProject(QStringLiteral("Modern"));
        Graph& g = p.graphs[0];
        DataSet& s = g.ensureSet(0);
        s.type = SetType::XYCOLOR;
        s.cols[0] = {1, 2, 3};
        s.cols[1] = {4, 5, 6};
        s.cols[2] = {0.1, 0.5, 0.9};
        s.colormap = QStringLiteral("cmo.thermal");
        s.cmapReversed = true;
        s.fill.color = QColor(10, 20, 30, 77);
        s.symbol.type = Symbol::Star5;
        g.legend.cornerRadius = 0.01;
        p.page.background.gradient = GradientType::Radial;
        const QString text = projectio::write(p, projectio::Dialect::Native);
        Project q;
        QVERIFY(projectio::loadText(text, &q, QStringLiteral("x.cplot")));
        const DataSet* t = q.graph(0)->set(0);
        QVERIFY(t);
        QCOMPARE(t->type, SetType::XYCOLOR);
        QCOMPARE(t->colormap, QStringLiteral("cmo.thermal"));
        QVERIFY(t->cmapReversed);
        QVERIFY(t->colorFromMap);
        QCOMPARE(t->fill.color, QColor(10, 20, 30, 77));
        QCOMPARE(t->symbol.type, Symbol::Star5);
        QCOMPARE(q.stylePreset, QStringLiteral("Modern"));
        QVERIFY(q.prettyNumbers);
        QCOMPARE(q.graph(0)->legend.cornerRadius, 0.01);
        QCOMPARE(q.page.background.gradient, GradientType::Radial);
        QCOMPARE(t->cols[2], s.cols[2]);
    }

    void interpreterCommands()
    {
        Project p = presets::newProject();
        Interpreter in(&p);
        QStringList out;
        in.messageHandler = [&](const QString& m) { out << m; };
        QVERIFY(in.execute(QStringLiteral("s0.x = mesh(0, 1, 11); s0.y = s0.x^2")));
        QCOMPARE(p.graph(0)->set(0)->size(), 11);
        QCOMPARE(p.graph(0)->set(0)->cols[1][10], 1.0);
        QVERIFY(in.execute(QStringLiteral("s0.y = s0.y * 2")));
        QCOMPARE(p.graph(0)->set(0)->cols[1][10], 2.0);
        QVERIFY(in.execute(QStringLiteral("runavg(s0, 3)")));
        QVERIFY(p.graph(0)->set(1));
        QCOMPARE(p.graph(0)->set(1)->size(), 9);
        QVERIFY(in.execute(QStringLiteral("copy s0 to s5")));
        QCOMPARE(p.graph(0)->set(5)->size(), 11);
        QVERIFY(in.execute(QStringLiteral("kill s5")));
        QVERIFY(!p.graph(0)->set(5));
        QVERIFY(in.execute(QStringLiteral("restrict(s0, s0.x < 0.5)")));
        QCOMPARE(p.graph(0)->set(0)->size(), 5);
        QVERIFY(in.execute(QStringLiteral("world xmin -1; world xmax 2; title \"Hi\"")));
        QCOMPARE(p.graph(0)->wxmin, -1.0);
        QCOMPARE(p.graph(0)->title.text, QStringLiteral("Hi"));
        QVERIFY(in.execute(QStringLiteral("xaxes scale logarithmic")));
        QCOMPARE(p.graph(0)->xscale, ScaleType::Log);
        QVERIFY(in.execute(QStringLiteral("define v[]; v[3] = 7")));
        expr::Value v;
        QVERIFY(in.evaluate(QStringLiteral("v[3] + 1"), &v));
        QCOMPARE(v.scalar(), 8.0);
        QVERIFY(in.execute(QStringLiteral("arrange(2, 2, 0.1, 0.15, 0.2)")));
        QCOMPARE(p.activeGraphCount(), 4);
        QVERIFY(in.execute(QStringLiteral("page size 400, 300")));
        QCOMPARE(p.page.width, 400.0);
        QVERIFY(in.execute(QStringLiteral("page size 612 792")));
        QCOMPARE(p.page.height, 792.0);
    }

    void nonlinearFitCommand()
    {
        Project p = presets::newProject();
        Interpreter in(&p);
        QString report;
        in.messageHandler = [&](const QString& m) { report += m; };
        QVERIFY(in.execute(QStringLiteral("s0.x = mesh(0, 10, 50); s0.y = 3*exp(-0.4*s0.x)")));
        QVERIFY(in.execute(QStringLiteral("fit formula \"y = a0*exp(-a1*x)\"; fit with 2 parameters; a0 = 1; a1 = 1")));
        QVERIFY(in.execute(QStringLiteral("nonlfit(s0, 100)")));
        QVERIFY(report.contains(QStringLiteral("a0")));
        expr::Value a0, a1;
        QVERIFY(in.evaluate(QStringLiteral("a0"), &a0));
        QVERIFY(in.evaluate(QStringLiteral("a1"), &a1));
        QVERIFY(std::fabs(a0.scalar() - 3) < 1e-6);
        QVERIFY(std::fabs(a1.scalar() - 0.4) < 1e-7);
        QVERIFY(p.graph(0)->set(1));  // fitted curve
    }

    void dataReading()
    {
        const auto t = datareader::parseText(QStringLiteral("# time value err\n1, 2, 0.5\n2,4,0.6\n&\n3 9\n4 16\n"));
        QCOMPARE(t.size(), 2);
        QCOMPARE(t[0].columns(), 3);
        QCOMPARE(t[0].headers, (QStringList{QStringLiteral("time"), QStringLiteral("value"), QStringLiteral("err")}));
        QCOMPARE(t[1].cols[1][1], 16.0);
        QVector<double> v;
        QString lab;
        QVERIFY(datareader::parseLine(QStringLiteral("2020-01-01T12:00:00 5"), &v, &lab));
        QCOMPARE(v.size(), 2);
        QVERIFY(std::fabs(v[0] - 2458850.0) < 1e-6);
        QVERIFY(datareader::parseLine(QStringLiteral("1.5D+02 nan \"text\""), &v, &lab));
        QCOMPARE(v[0], 150.0);
        QVERIFY(std::isnan(v[1]));
        QCOMPARE(lab, QStringLiteral("text"));

        Project p = presets::newProject();
        Interpreter in(&p);
        QVERIFY(in.readDataText(QStringLiteral("0 1 2 3\n1 2 4 6\n2 3 6 9\n"), Interpreter::ReadMode::NXY, SetType::XY, 0, QStringLiteral("nxy")));
        QCOMPARE(p.graph(0)->activeSetCount(), 3);
        QVERIFY(in.readDataText(QStringLiteral("1 2 3 4\n5 6 7 8\n"), Interpreter::ReadMode::Block, SetType::XY, 0, QStringLiteral("blk")));
        QVERIFY(in.formBlockSet(SetType::XYDY, QStringLiteral("0:2:3")));
        const DataSet* s = p.graph(0)->set(in.lastCreatedSet());
        QVERIFY(s);
        QCOMPARE(s->type, SetType::XYDY);
        QCOMPARE(s->cols[1], (QVector<double>{3, 7}));

        // restrict keeps x and y of the same points
        Project r = presets::newProject();
        Interpreter ir(&r);
        QVERIFY(ir.execute(QStringLiteral("s0.x = mesh(0, 9, 10); s0.y = s0.x * 10; restrict(s0, s0.x > 5)")));
        QCOMPARE(r.graphs[0].sets[0].cols[0], (QVector<double>{6, 7, 8, 9}));
        QCOMPARE(r.graphs[0].sets[0].cols[1], (QVector<double>{60, 70, 80, 90}));

        // "s0" on the right-hand side means the current graph, even when assigning to another graph
        QVERIFY(ir.execute(QStringLiteral("g1.s0.y = s0.y / 10")));
        QCOMPARE(r.graph(1)->sets[0].cols[1], (QVector<double>{6, 7, 8, 9}));
        // define a vector variable
        QVERIFY(ir.execute(QStringLiteral("define v[5]")));
        expr::Value len;
        QString err;
        QVERIFY(ir.evaluate(QStringLiteral("len(v)"), &len, &err));
        QCOMPARE(len.scalar(), 5.0);
    }

    void gromacsXvg()
    {
        // GROMACS writes Grace commands into its .xvg data files
        const QString hdr = QStringLiteral("# This file was created by GROMACS\n# gmx analyze\n"
                                           "@    title \"Average\"\n@    xaxis  label \"Time (ns)\"\n");
        Project p = presets::newProject();
        Interpreter in(&p);
        QVERIFY(in.readDataText(hdr + QStringLiteral("@TYPE xydy\n0 1.0 0.1\n1 2.0 0.2\n2 3.0 0.3\n"), Interpreter::ReadMode::Single,
                                SetType::XY, 0, QStringLiteral("average.xvg")));
        QCOMPARE(p.graph(0)->activeSetCount(), 1);
        const DataSet& avg = p.graphs[0].sets[0];
        QCOMPARE(avg.type, SetType::XYDY);  // error bars by default
        QCOMPARE(avg.cols[2], (QVector<double>{0.1, 0.2, 0.3}));
        QCOMPARE(p.graphs[0].title.text, QStringLiteral("Average"));
        QCOMPARE(p.graphs[0].axes[Graph::X].label, QStringLiteral("Time (ns)"));

        // several columns: one set each, with the file's legends; set numbers in the file are
        // relative to the sets it creates, and the xmgrace page layout is left to the modern style
        const double viewBefore = p.graphs[0].vxmin;
        const QString energy = hdr + QStringLiteral("@TYPE xy\n@ view 0.15, 0.15, 0.75, 0.85\n@ legend loctype view\n@ legend 0.78, 0.8\n"
                                                    "@ s0 legend \"Bond\"\n@ s1 legend \"Angle\"\n0 5 7\n1 6 8\n");
        QVERIFY(in.readDataText(energy, Interpreter::ReadMode::Single, SetType::XY, 0, QStringLiteral("energy.xvg")));
        QCOMPARE(p.graph(0)->activeSetCount(), 3);
        QCOMPARE(p.graphs[0].sets[0].legend, QString());
        QCOMPARE(p.graphs[0].sets[1].legend, QStringLiteral("Bond"));
        QCOMPARE(p.graphs[0].sets[2].legend, QStringLiteral("Angle"));
        QCOMPARE(p.graphs[0].sets[2].cols[1], (QVector<double>{7, 8}));
        QCOMPARE(p.graphs[0].sets[2].type, SetType::XY);
        QCOMPARE(p.graphs[0].vxmin, viewBefore);
        QVERIFY(p.graphs[0].legend.autoPlace);

        // X Y1 dY1 Y2 dY2: two sets with error bars
        Project q = presets::newProject();
        Interpreter iq(&q);
        QVERIFY(iq.readDataText(QStringLiteral("@TYPE xydy\n0 1 .1 5 .5\n1 2 .2 6 .6\n"), Interpreter::ReadMode::Single, SetType::XY, 0,
                                QStringLiteral("two.xvg")));
        QCOMPARE(q.graph(0)->activeSetCount(), 2);
        QCOMPARE(q.graphs[0].sets[1].type, SetType::XYDY);
        QCOMPARE(q.graphs[0].sets[1].cols[2], (QVector<double>{.5, .6}));
        // the TYPE line does not leak into the next file
        QVERIFY(iq.readDataText(QStringLiteral("0 1 2\n1 2 3\n"), Interpreter::ReadMode::Single, SetType::XY, 0, QStringLiteral("plain.dat")));
        QCOMPARE(q.graphs[0].sets[2].type, SetType::XY);

        // the Grace look keeps the GROMACS layout, as xmgrace shows it
        Project r = presets::newProject(QStringLiteral("Grace Classic"));
        Interpreter ir(&r);
        QVERIFY(ir.readDataText(energy, Interpreter::ReadMode::Single, SetType::XY, 0, QStringLiteral("energy.xvg")));
        QVERIFY(std::fabs(r.graphs[0].vxmin - 0.15) < 1e-9);
        QVERIFY(!r.graphs[0].legend.autoPlace);

        // -nxy fills the sets the file named
        Project n = presets::newProject();
        Interpreter in2(&n);
        QVERIFY(in2.readDataText(energy, Interpreter::ReadMode::NXY, SetType::XY, 0, QStringLiteral("energy.xvg")));
        QCOMPARE(n.graph(0)->activeSetCount(), 2);
        QCOMPARE(n.graphs[0].sets[1].legend, QStringLiteral("Angle"));
        QCOMPARE(n.graphs[0].sets[1].cols[1], (QVector<double>{7, 8}));

        // a header line names an XYDY set after its Y column
        Project h = presets::newProject();
        Interpreter ih(&h);
        QVERIFY(ih.readDataText(QStringLiteral("# time counts error\n0 98 4\n1 61 3\n"), Interpreter::ReadMode::Single, SetType::XYDY, 0,
                                QStringLiteral("clipboard")));
        QCOMPARE(h.graphs[0].sets[0].legend, QStringLiteral("counts"));

        QVERIFY(datareader::hasGraceCommands(energy));
        QVERIFY(!datareader::hasGraceCommands(QStringLiteral("# x y\n1 2\n")));
    }

    void exports()
    {
        Project p = presets::newProject();
        Interpreter in(&p);
        in.execute(QStringLiteral("s0.x = mesh(0, 6.28, 100); s0.y = sin(s0.x)"));
        p.graphs[0].sets[0].legend = QStringLiteral("$\\sin x$");
        p.graphs[0].autoscale();
        for (const QString& f : exporter::formats()) {
            exporter::Options o;
            o.format = f;
            o.dpi = 50;
            QString err;
            const QByteArray b = exporter::exportToBytes(p, o, &err);
            QVERIFY2(!b.isEmpty(), qPrintable(f + QStringLiteral(": ") + err));
            if (f == QLatin1String("pdf")) QVERIFY(b.startsWith("%PDF"));
            if (f == QLatin1String("svg")) QVERIFY(b.contains("<svg"));
            if (f == QLatin1String("eps")) {
                QVERIFY(b.startsWith("%!PS-Adobe-3.0 EPSF-3.0"));
                QVERIFY(b.contains("%%BoundingBox: 0 0 792 612"));
                QVERIFY(b.contains("showpage"));
            }
            if (f == QLatin1String("png")) QVERIFY(b.startsWith("\x89PNG"));
        }
        // rendering is not blank and uses colour
        const QImage img = renderToImage(p, 400);
        QCOMPARE(img.width(), 400);
        int coloured = 0;
        for (int y = 0; y < img.height(); y += 2)
            for (int x = 0; x < img.width(); x += 2) {
                const QColor c = img.pixelColor(x, y);
                if (std::abs(c.red() - c.blue()) > 40) ++coloured;
            }
        QVERIFY(coloured > 20);
    }

    void cliHardcopy()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString data = dir.filePath(QStringLiteral("d.dat"));
        QFile f(data);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("1 1\n2 4\n3 9\n");
        f.close();
        const QString out = dir.filePath(QStringLiteral("o.svg"));
        const QString saved = dir.filePath(QStringLiteral("s.agr"));
        auto r = cli::process({QStringLiteral("cactusplot"), QStringLiteral("-hardcopy"), QStringLiteral("-hdevice"),
                               QStringLiteral("SVG"), QStringLiteral("-printfile"), out, QStringLiteral("-log"),
                               QStringLiteral("y"), QStringLiteral("-saveall"), saved, data});
        QCOMPARE(r.exitCode, 0);
        QVERIFY(QFile::exists(out));
        QVERIFY(QFile::exists(saved));
        Project q;
        QVERIFY(projectio::load(saved, &q));
        QCOMPARE(q.graph(0)->set(0)->size(), 3);
        QCOMPARE(q.graph(0)->yscale, ScaleType::Log);

        const QString png = dir.filePath(QStringLiteral("x.png"));
        r = cli::process({QStringLiteral("cactusplot"), QStringLiteral("--style"), QStringLiteral("Dark"),
                          QStringLiteral("--export"), png, QStringLiteral("-pexec"), QStringLiteral("s0.x = mesh(1, 5, 5); s0.y = s0.x")});
        QCOMPARE(r.exitCode, 0);
        QVERIFY(QImage(png).width() > 100);

        // file managers may pass file:// URLs; unreadable files are reported, not silently ignored
        const QString xvg = dir.filePath(QStringLiteral("rmsd.xvg"));
        QFile fx(xvg);
        QVERIFY(fx.open(QIODevice::WriteOnly));
        fx.write("# GROMACS\n@    title \"RMSD\"\n@TYPE xy\n0 0.1\n1 0.2\n2 0.25\n");
        fx.close();
        r = cli::process({QStringLiteral("cactusplot"), QUrl::fromLocalFile(xvg).toString()});
        QVERIFY(r.errors.isEmpty());
        QCOMPARE(r.project.graph(0)->activeSetCount(), 1);
        QCOMPARE(r.project.graph(0)->sets[0].type, SetType::XY);
        r = cli::process({QStringLiteral("cactusplot"), dir.filePath(QStringLiteral("missing.xvg"))});
        QCOMPARE(r.errors.size(), 1);
        QVERIFY(r.launchGui);
    }

    void colors()
    {
        QColor c;
        QVERIFY(colors::parse(QStringLiteral("cornflowerblue"), &c));
        QCOMPARE(c, QColor(100, 149, 237));
        QVERIFY(colors::parse(QStringLiteral("xkcd:dark mint"), &c));
        QVERIFY(colors::parse(QStringLiteral("#ff000080"), &c));
        QCOMPARE(c.alpha(), 128);
        QVERIFY(colors::parse(QStringLiteral("rgba(1,2,3,0.5)"), &c));
        QVERIFY(colors::namedColors().size() > 1500);
        QVERIFY(colors::palettes().size() > 40);
        QVERIFY(colors::colormaps().size() > 150);
        QVERIFY(colors::colormap(QStringLiteral("viridis")));
        QVERIFY(colors::colormap(QStringLiteral("batlow")));
        QVERIFY(colors::colormap(QStringLiteral("thermal")));
        QCOMPARE(colors::graceColor(2), QColor(255, 0, 0));
        const Colormap* v = colors::colormap(QStringLiteral("viridis"));
        QCOMPARE(v->at(0.0).name(), QStringLiteral("#440154"));
    }
};

QTEST_MAIN(TestGrace)
#include "test_grace.moc"
