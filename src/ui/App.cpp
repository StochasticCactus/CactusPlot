// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/App.h"

#include <QFileOpenEvent>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include "ui/Document.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

namespace cactus::ui {

namespace {
/// Handles macOS "open with" / double-clicked documents.
class FileOpenFilter : public QObject {
public:
    explicit FileOpenFilter(MainWindow* w) : m_w(w) {}
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (e->type() == QEvent::FileOpen) {
            m_w->openFile(static_cast<QFileOpenEvent*>(e)->file());
            return true;
        }
        return QObject::eventFilter(o, e);
    }

private:
    MainWindow* m_w;
};
}  // namespace

int runGui(QApplication& app, cli::Outcome& outcome)
{
    QApplication::setWindowIcon(icons::appIcon());
    QGuiApplication::setDesktopFileName(QStringLiteral("org.cactusplot.CactusPlot"));
    QSettings st;
    theme::apply(app, ThemeMode(st.value(QStringLiteral("ui/theme"), 0).toInt()), st.value(QStringLiteral("ui/native"), false).toBool());

    MainWindow w;
    FileOpenFilter filter(&w);
    app.installEventFilter(&filter);
    const bool hasContent = !outcome.projectPath.isEmpty() || [&] {
        for (const Graph& g : outcome.project.graphs) if (g.active && g.activeSetCount() > 0) return true;
        return false;
    }() || !outcome.project.objects.isEmpty();
    if (hasContent) {
        w.setProject(outcome.project, outcome.projectPath.endsWith(QLatin1String(".cplot")) || outcome.projectPath.endsWith(QLatin1String(".agr"))
                                          ? outcome.projectPath
                                          : QString());
    }
    if (outcome.fixedCanvas.isValid()) w.resize(outcome.fixedCanvas + QSize(600, 200));
    // files given at start-up (double-click, "Open With") that could not be read: say so, don't just open empty
    QStringList problems;
    for (const QString& e : outcome.errors)
        if (!e.startsWith(QLatin1String("Warning:"))) problems << e;
    if (!problems.isEmpty() && qEnvironmentVariableIsEmpty("CACTUS_SCREENSHOT"))
        QTimer::singleShot(300, &w, [&w, problems] {
            QMessageBox::warning(&w, QObject::tr("Could not open"), problems.mid(0, 10).join(QLatin1Char('\n')));
        });
    // Documentation/testing aid: CACTUS_SCREENSHOT=file.png saves a screenshot of the window and quits.
    const QString shot = qEnvironmentVariable("CACTUS_SCREENSHOT");
    if (!shot.isEmpty()) {
        const QStringList size = qEnvironmentVariable("CACTUS_SCREENSHOT_SIZE", QStringLiteral("1440x900")).split(QLatin1Char('x'));
        w.resize(size.value(0).toInt(), size.value(1).toInt());
        w.show();
        const QString which = qEnvironmentVariable("CACTUS_SCREENSHOT_WINDOW");
        QWidget* target = which.isEmpty() ? static_cast<QWidget*>(&w) : w.openWindow(which);
        QTimer::singleShot(1500, &w, [target, shot] {
            target->grab().save(shot);
            QApplication::quit();
        });
        return app.exec();
    }
    w.show();
    if (!hasContent) QTimer::singleShot(200, &w, [&w] { w.checkAutosaveRecovery(); });
    return app.exec();
}

}  // namespace cactus::ui
