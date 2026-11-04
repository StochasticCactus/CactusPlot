// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/Cli.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
#include <cstdio>

#include "Version.h"
#include "core/Presets.h"
#include "core/color/Colors.h"
#include "grace/ProjectIO.h"
#include "io/DataReader.h"
#include "math/Expr.h"
#include "render/Export.h"

namespace cactus::cli {

namespace {

void out(const QString& s)
{
    QTextStream ts(stdout);
    ts << s;
    if (!s.endsWith(QLatin1Char('\n'))) ts << '\n';
}

QStringList* g_errors = nullptr;  // collects messages for the GUI while process() runs

void err(const QString& s)
{
    if (g_errors) g_errors->push_back(s.trimmed());
    QTextStream ts(stderr);
    ts << s;
    if (!s.endsWith(QLatin1Char('\n'))) ts << '\n';
}

QString opt(const QString& a)
{
    // accept -opt and --opt
    QString o = a;
    if (o.startsWith(QLatin1String("--"))) o = o.mid(1);
    return o.toLower();
}

bool isBatchProgram(const QString& argv0)
{
    const QString base = QFileInfo(argv0).baseName().toLower();
    return base.contains(QLatin1String("bat"));
}

}  // namespace

QString versionText()
{
    QString s = QStringLiteral(CACTUS_APP_NAME " " CACTUS_VERSION);
    if (!QStringLiteral(CACTUS_BUILD_INFO).isEmpty()) s += QStringLiteral(" (" CACTUS_BUILD_INFO ")");
    s += QLatin1Char('\n');
    s += QStringLiteral("Grace-compatible 2D plotting. Built with Qt %1.\n").arg(QString::fromLatin1(qVersion()));
    s += QStringLiteral("Hardcopy devices: %1\n").arg(exporter::formats().join(QStringLiteral(", ")));
    s += QStringLiteral("License: GPL-2.0-or-later. " CACTUS_HOMEPAGE "\n");
    return s;
}

QString usage(const QString& program)
{
    return QStringLiteral(
               "Usage: %1 [options] [project.cplot|project.agr|data files...]\n"
               "\n"
               "Grace-compatible options (processed in order):\n"
               "  -autoscale x|y|xy|none    Autoscale on read (overrides parameter files)\n"
               "  -barebones                Hide toolbars\n"
               "  -batch file               Execute a batch (command) file after start-up\n"
               "  -block file               Read block data\n"
               "  -bxy x:y:...              Form a set from block data columns\n"
               "  -datehint iso|european|us|days|seconds|nohint\n"
               "  -dpipe fd                 Read data from a file descriptor\n"
               "  -fixed width height       Fixed canvas size\n"
               "  -free                     Free page layout\n"
               "  -graph n                  Set the current graph\n"
               "  -graphtype xy|chart|fixed|polar|pie|smith\n"
               "  -hardcopy                 Print and exit (no GUI)\n"
               "  -hdevice device           Hardcopy device: %2\n"
               "  -legend load              Turn the legend on, labelled with file names\n"
               "  -log x|y|xy               Logarithmic axes for the current graph\n"
               "  -noask -noprint -nosafe -safe -mono -install -noinstall -nosigcatch\n"
               "  -npipe file               Read data from a named pipe\n"
               "  -nxy file                 Read X Y1 Y2 ... data\n"
               "  -param file (-p file)     Load a parameter file\n"
               "  -pexec \"command\"          Execute a command\n"
               "  -pipe                     Read data from standard input\n"
               "  -printfile file           Hardcopy output file\n"
               "  -remove                   Remove data files after reading\n"
               "  -results file             Write results of data manipulations to file\n"
               "  -rvideo                   Exchange black and white\n"
               "  -saveall file             Save the project to file\n"
               "  -seed n                   Random number generator seed\n"
               "  -settype xy|xydy|...      Set type of the next data file\n"
               "  -source disk|pipe         Source of the next data file\n"
               "  -timestamp                Add a time stamp\n"
               "  -viewport xmin ymin xmax ymax\n"
               "  -wd directory             Working directory\n"
               "  -world xmin ymin xmax ymax\n"
               "  -version, -usage, -help\n"
               "\n"
               "CactusPlot options:\n"
               "  -o, --export file         Export to file (format from extension) and exit\n"
               "  --style name              Style preset for new plots: %3\n"
               "  --palette name            Colour palette for the data sets\n"
               "  --dpi n                   Raster export resolution (default 300)\n"
               "  --transparent             Transparent background (PNG/TIFF/WebP/SVG/PDF)\n"
               "  --tight                   Tight EPS bounding box\n"
               "  --title text              Graph title          --xlabel text / --ylabel text\n"
               "  --list-presets | --list-palettes | --list-colormaps | --list-colors\n")
        .arg(QFileInfo(program).fileName(), exporter::formats().join(QLatin1Char('|')), presets::names().join(QStringLiteral(", ")));
}

bool isHeadless(const QStringList& args)
{
    if (!args.isEmpty() && isBatchProgram(args[0])) return true;
    for (int i = 1; i < args.size(); ++i) {
        const QString o = opt(args[i]);
        if (o == QLatin1String("-hardcopy") || o == QLatin1String("-o") || o == QLatin1String("-export") ||
            o == QLatin1String("-version") || o == QLatin1String("-v") || o == QLatin1String("-help") || o == QLatin1String("-h") ||
            o == QLatin1String("-usage") || o.startsWith(QLatin1String("-list-")))
            return true;
    }
    return false;
}

Outcome process(const QStringList& args)
{
    Outcome R;
    g_errors = &R.errors;
    struct Reset { ~Reset() { g_errors = nullptr; } } reset;
    const QString program = args.value(0, QStringLiteral(CACTUS_APP_NAME));
    bool hardcopy = isBatchProgram(program);
    bool noprint = false;
    QString style = presets::defaultName();
    QString palette;
    bool styleExplicit = false;
    QStringList batchFiles;
    QString saveAll;
    QString resultsFile;
    QStringList results;
    bool removeFiles = false;
    bool legendLoad = false;
    bool pipeSource = false;
    QString exportPath;
    double dpi = 300.0;
    bool transparent = false, tight = false;
    QString title, xlabel, ylabel;
    bool anyData = false;
    bool rvideo = false;

    // first pass: style must be known before the project is created
    for (int i = 1; i < args.size(); ++i) {
        const QString o = opt(args[i]);
        if (o == QLatin1String("-style") && i + 1 < args.size()) {
            style = args[++i];
            styleExplicit = true;
        }
    }
    bool styleOk = false;
    for (const QString& n : presets::names()) if (n.compare(style, Qt::CaseInsensitive) == 0) { style = n; styleOk = true; }
    if (!styleOk) {
        err(QStringLiteral("Unknown style '%1'; using %2").arg(style, presets::defaultName()));
        style = presets::defaultName();
    }

    R.project = presets::newProject(style);
    Interpreter in(&R.project);
    in.workingDir = QDir::currentPath();
    in.messageHandler = [&](const QString& m) {
        results << m;
        // errors are reported once, on standard error, by the caller
        if (hardcopy && resultsFile.isEmpty() && !m.startsWith(QLatin1String("Error: "))) out(m);
    };
    SetType settype = SetType::XY;
    bool settypeExplicit = false;
    bool firstData = true;

    auto readData = [&](const QString& path, Interpreter::ReadMode mode) {
        QString text, e;
        QString local = path;
        // file managers may pass "file:///home/me/data.xvg" instead of a path
        if (local.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
            const QUrl url(local);
            if (url.isLocalFile()) local = url.toLocalFile();
        }
        const QString full = (local == QLatin1String("-") || QFileInfo(local).isAbsolute()) ? local : QDir(in.workingDir).filePath(local);
        if (!datareader::readFile(full, &text, &e)) {
            err(e);
            R.exitCode = 1;
            return;
        }
        if ((mode == Interpreter::ReadMode::Single || mode == Interpreter::ReadMode::Auto) && projectio::isProjectText(text)) {
            // a project file: replace the current project (Grace semantics)
            Project p;
            QStringList warns;
            projectio::loadText(text, &p, QFileInfo(full).fileName(), &e, &warns);
            for (const QString& w : warns) results << w;
            R.project = p;
            in.setProject(&R.project);
            R.projectPath = QFileInfo(full).absoluteFilePath();
            if (styleExplicit) presets::applyToProject(R.project, style);
            firstData = false;
            return;
        }
        if (firstData && R.project.graphs.value(0).activeSetCount() == 0) firstData = false;
        if (!in.readDataText(text, mode, settype, in.currentGraph(), QFileInfo(full).fileName())) R.exitCode = 1;
        for (const QString& m : in.errors()) err(m);
        for (const QString& w : in.warnings()) err(QStringLiteral("Warning: ") + w);
        in.clearMessages();
        anyData = true;
        if (removeFiles && path != QLatin1String("-")) QFile::remove(full);
    };

    for (int i = 1; i < args.size(); ++i) {
        const QString a = args[i];
        const QString o = opt(a);
        auto next = [&](const char* what) -> QString {
            if (i + 1 >= args.size()) {
                err(QStringLiteral("Missing argument for %1 (%2)").arg(a, QString::fromLatin1(what)));
                R.exitCode = 1;
                return QString();
            }
            return args[++i];
        };
        if (!a.startsWith(QLatin1Char('-')) || a == QLatin1String("-")) {
            // without -settype, X Y1 Y2 ... gives one curve per column; error bars only when the file
            // says so ("@TYPE xydy") or -settype asks for them
            readData(a, settypeExplicit ? Interpreter::ReadMode::Single : Interpreter::ReadMode::Auto);
            pipeSource = false;
            continue;
        }
        if (o == QLatin1String("-version") || o == QLatin1String("-v")) { out(versionText()); R.done = true; R.launchGui = false; return R; }
        if (o == QLatin1String("-help") || o == QLatin1String("-h") || o == QLatin1String("-usage")) {
            out(usage(program));
            R.done = true;
            R.launchGui = false;
            return R;
        }
        if (o == QLatin1String("-list-presets")) {
            for (const QString& n : presets::names()) out(QStringLiteral("%1 - %2").arg(n, presets::description(n)));
            R.done = true; R.launchGui = false; return R;
        }
        if (o == QLatin1String("-list-palettes")) {
            for (const Palette& p : colors::palettes()) out(QStringLiteral("%1 [%2] (%3 colours)").arg(p.name, p.group).arg(p.colors.size()));
            R.done = true; R.launchGui = false; return R;
        }
        if (o == QLatin1String("-list-colormaps")) {
            for (const Colormap& m : colors::colormaps()) out(QStringLiteral("%1 [%2, %3]").arg(m.name, m.category, m.source));
            R.done = true; R.launchGui = false; return R;
        }
        if (o == QLatin1String("-list-colors") || o == QLatin1String("-list-colours")) {
            for (const NamedColor& c : colors::namedColors()) out(QStringLiteral("%1 %2 (%3)").arg(c.name, c.color.name(), c.source));
            R.done = true; R.launchGui = false; return R;
        }
        if (o == QLatin1String("-style")) { ++i; continue; }
        if (o == QLatin1String("-palette")) { palette = next("palette"); continue; }
        if (o == QLatin1String("-dpi")) { dpi = next("dpi").toDouble(); continue; }
        if (o == QLatin1String("-transparent")) { transparent = true; continue; }
        if (o == QLatin1String("-tight")) { tight = true; continue; }
        if (o == QLatin1String("-title")) { title = next("text"); continue; }
        if (o == QLatin1String("-xlabel")) { xlabel = next("text"); continue; }
        if (o == QLatin1String("-ylabel")) { ylabel = next("text"); continue; }
        if (o == QLatin1String("-o") || o == QLatin1String("-export")) { exportPath = next("file"); hardcopy = true; continue; }
        if (o == QLatin1String("-autoscale")) {
            const QString v = next("x|y|xy").toLower();
            in.autoscaleOnRead = v == QLatin1String("none") ? 0 : v == QLatin1String("x") ? 1 : v == QLatin1String("y") ? 2 : 3;
            continue;
        }
        if (o == QLatin1String("-barebones")) { R.barebones = true; continue; }
        if (o == QLatin1String("-batch")) { batchFiles << next("file"); continue; }
        if (o == QLatin1String("-block")) { readData(next("file"), Interpreter::ReadMode::Block); continue; }
        if (o == QLatin1String("-bxy")) { in.formBlockSet(settype, next("columns"), in.currentGraph()); anyData = true; continue; }
        if (o == QLatin1String("-datehint")) { next("hint"); continue; }
        if (o == QLatin1String("-dpipe")) {
            const int fd = next("fd").toInt();
            QFile f;
            if (f.open(fd, QIODevice::ReadOnly)) {
                in.readDataText(QString::fromUtf8(f.readAll()), Interpreter::ReadMode::Single, settype, in.currentGraph(), QStringLiteral("pipe"));
                anyData = true;
            }
            continue;
        }
        if (o == QLatin1String("-fixed")) {
            const int w = next("width").toInt(), h = next("height").toInt();
            R.fixedCanvas = QSize(w, h);
            continue;
        }
        if (o == QLatin1String("-free") || o == QLatin1String("-noask") || o == QLatin1String("-mono") ||
            o == QLatin1String("-install") || o == QLatin1String("-noinstall") || o == QLatin1String("-nosigcatch")) continue;
        if (o == QLatin1String("-graph")) { in.setCurrentGraph(next("n").toInt()); in.ensureCurrentGraph(); continue; }
        if (o == QLatin1String("-graphtype")) {
            GraphType t;
            if (parseGraphType(next("type"), &t)) in.ensureCurrentGraph().type = t;
            continue;
        }
        if (o == QLatin1String("-hardcopy")) { hardcopy = true; continue; }
        if (o == QLatin1String("-hdevice")) { in.hardcopy().device = next("device"); continue; }
        if (o == QLatin1String("-legend")) { next("load"); legendLoad = true; continue; }
        if (o == QLatin1String("-log")) {
            const QString v = next("x|y|xy").toLower();
            Graph& g = in.ensureCurrentGraph();
            if (v.contains(QLatin1Char('x'))) g.xscale = ScaleType::Log;
            if (v.contains(QLatin1Char('y'))) g.yscale = ScaleType::Log;
            g.autoscale();
            continue;
        }
        if (o == QLatin1String("-maxpath") || o == QLatin1String("-timer")) { next("value"); continue; }
        if (o == QLatin1String("-netcdf") || o == QLatin1String("-netcdfxy")) {
            err(QStringLiteral("netCDF support is not compiled in"));
            next("arg");
            if (o == QLatin1String("-netcdfxy")) next("arg");
            continue;
        }
        if (o == QLatin1String("-noprint")) { noprint = true; continue; }
        if (o == QLatin1String("-nosafe")) { in.safeMode = false; continue; }
        if (o == QLatin1String("-safe")) { in.safeMode = true; continue; }
        if (o == QLatin1String("-npipe")) { readData(next("file"), Interpreter::ReadMode::Single); continue; }
        if (o == QLatin1String("-nxy")) { readData(next("file"), Interpreter::ReadMode::NXY); continue; }
        if (o == QLatin1String("-param") || o == QLatin1String("-p")) {
            const QString f = next("file");
            if (!in.runFile(f)) for (const QString& e : in.errors()) err(e);
            for (const QString& w : in.warnings()) err(QStringLiteral("Warning: ") + w);
            in.clearMessages();
            continue;
        }
        if (o == QLatin1String("-pexec")) {
            in.execute(next("command"));
            for (const QString& e : in.errors()) err(e);
            for (const QString& w : in.warnings()) err(QStringLiteral("Warning: ") + w);
            in.clearMessages();
            continue;
        }
        if (o == QLatin1String("-pipe")) { readData(QStringLiteral("-"), Interpreter::ReadMode::Single); continue; }
        if (o == QLatin1String("-printfile")) { in.hardcopy().printFile = next("file"); continue; }
        if (o == QLatin1String("-remove")) { removeFiles = true; continue; }
        if (o == QLatin1String("-results")) { resultsFile = next("file"); continue; }
        if (o == QLatin1String("-rvideo")) { rvideo = true; continue; }
        if (o == QLatin1String("-saveall")) { saveAll = next("file"); continue; }
        if (o == QLatin1String("-seed")) { expr::seedRandom(next("seed").toULongLong()); continue; }
        if (o == QLatin1String("-settype")) {
            SetType t;
            const QString v = next("type");
            if (parseSetType(v, &t)) { settype = t; settypeExplicit = true; }
            else err(QStringLiteral("Unknown set type %1").arg(v));
            continue;
        }
        if (o == QLatin1String("-source")) { pipeSource = next("disk|pipe").toLower() == QLatin1String("pipe"); continue; }
        if (o == QLatin1String("-timestamp")) { R.project.timestamp.active = true; continue; }
        if (o == QLatin1String("-viewport")) {
            Graph& g = in.ensureCurrentGraph();
            g.vxmin = next("xmin").toDouble(); g.vymin = next("ymin").toDouble();
            g.vxmax = next("xmax").toDouble(); g.vymax = next("ymax").toDouble();
            continue;
        }
        if (o == QLatin1String("-wd")) {
            const QString d = next("dir");
            if (QDir(d).exists()) { in.workingDir = QDir(d).absolutePath(); QDir::setCurrent(d); }
            else err(QStringLiteral("No such directory: %1").arg(d));
            continue;
        }
        if (o == QLatin1String("-world")) {
            Graph& g = in.ensureCurrentGraph();
            g.wxmin = next("xmin").toDouble(); g.wymin = next("ymin").toDouble();
            g.wxmax = next("xmax").toDouble(); g.wymax = next("ymax").toDouble();
            g.autoticks();
            in.autoscaleOnRead = 0;
            continue;
        }
        err(QStringLiteral("Unknown option %1 (try -help)").arg(a));
        R.exitCode = 1;
    }

    // post-processing
    if (legendLoad) {
        for (Graph& g : R.project.graphs) {
            if (!g.active) continue;
            g.legend.active = true;
            for (DataSet& s : g.sets) if (s.active && s.legend.isEmpty()) s.legend = s.comment;
        }
    } else if (anyData && R.projectPath.isEmpty()) {
        // name sets after their source when several files were given (nice legends by default)
        for (Graph& g : R.project.graphs) {
            if (!g.active || g.activeSetCount() < 2) continue;
            for (DataSet& s : g.sets) if (s.active && s.legend.isEmpty() && !s.comment.isEmpty()) s.legend = QFileInfo(s.comment).completeBaseName();
        }
    }
    if (!palette.isEmpty()) {
        if (colors::palette(palette)) presets::applyPalette(R.project, palette);
        else err(QStringLiteral("Unknown palette '%1' (see --list-palettes)").arg(palette));
    }
    if (Graph* g = R.project.graph(in.currentGraph())) {
        if (!title.isEmpty()) g->title.text = title;
        if (!xlabel.isEmpty()) g->axes[0].label = xlabel;
        if (!ylabel.isEmpty()) g->axes[1].label = ylabel;
    }
    if (rvideo) {
        auto swapBW = [](QColor& c) {
            if (c.rgb() == qRgb(0, 0, 0)) c = QColor(255, 255, 255, c.alpha());
            else if (c.rgb() == qRgb(255, 255, 255)) c = QColor(0, 0, 0, c.alpha());
        };
        for (ColorMapEntry& e : R.project.colorMap) swapBW(e.color);
        swapBW(R.project.page.background.color);
        for (Graph& g : R.project.graphs) {
            swapBW(g.frame.line.color);
            for (Axis& ax : g.axes) {
                swapBW(ax.bar.color); swapBW(ax.majorProps.line.color); swapBW(ax.minorProps.line.color);
                swapBW(ax.ticklabelText.color); swapBW(ax.labelText.color);
            }
            swapBW(g.title.style.color);
            swapBW(g.subtitle.style.color);
            swapBW(g.legend.text.color);
            swapBW(g.legend.boxFill.color);
            for (DataSet& s : g.sets) { swapBW(s.line.color); swapBW(s.symbol.line.color); swapBW(s.symbol.fill.color); }
        }
    }

    auto doPrint = [&](const HardcopySettings& hs) -> bool {
        QString file = hs.printFile;
        QString fmt = exporter::normalizeFormat(hs.device);
        if (!exportPath.isEmpty()) {
            file = exportPath;
            const int dot = file.lastIndexOf(QLatin1Char('.'));
            if (dot > 0) fmt = exporter::normalizeFormat(file.mid(dot + 1));
        }
        if (!exporter::formats().contains(fmt)) fmt = QStringLiteral("png");
        if (file.isEmpty()) {
            QString base = QStringLiteral("untitled");
            if (!R.projectPath.isEmpty()) base = QFileInfo(R.projectPath).completeBaseName();
            file = base + QLatin1Char('.') + exporter::extensionFor(fmt);
        }
        if (QFileInfo(file).isRelative() && file != QLatin1String("-")) file = QDir(in.workingDir).filePath(file);
        exporter::Options eo;
        eo.format = fmt;
        eo.dpi = hs.dpi > 0 && dpi == 300.0 ? hs.dpi : dpi;
        eo.transparent = transparent || hs.transparent;
        eo.grayscale = hs.grayscale;
        eo.tightBBox = tight || hs.options.contains(QLatin1String("bbox:tight"));
        QString e;
        if (!exporter::exportToFile(R.project, file, eo, &e)) {
            err(e);
            R.exitCode = 1;
            return false;
        }
        return true;
    };
    bool printed = false;
    in.printHandler = [&](const HardcopySettings& hs) {
        printed = true;
        return doPrint(hs);
    };
    in.exitHandler = [&](int code) {
        R.exitCode = code;
        R.done = true;
        R.launchGui = false;
    };
    for (const QString& b : batchFiles) {
        if (!in.runFile(b)) for (const QString& e : in.errors()) err(e);
        for (const QString& w : in.warnings()) err(QStringLiteral("Warning: ") + w);
        in.clearMessages();
        if (R.done) break;
    }
    for (const QString& w : in.warnings()) err(QStringLiteral("Warning: ") + w);

    if (!saveAll.isEmpty()) {
        QString e;
        if (!projectio::save(R.project, saveAll, &e)) { err(e); R.exitCode = 1; }
    }
    if (!resultsFile.isEmpty() && !results.isEmpty()) {
        QFile f(QDir(in.workingDir).filePath(resultsFile));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(results.join(QLatin1Char('\n')).toUtf8() + '\n');
    }
    if (hardcopy) {
        if (!noprint && !printed && !R.done) doPrint(in.hardcopy());
        R.launchGui = false;
        R.done = true;
    }
    return R;
}

}  // namespace cactus::cli
