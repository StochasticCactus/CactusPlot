// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - program entry point.
#include <QGuiApplication>
#include <QStringList>
#include <cstdlib>
#include <memory>

#include "Version.h"
#include "app/Cli.h"
#include "plot/Fonts.h"

#ifdef CACTUS_HAVE_GUI
#include <QApplication>
#include "ui/App.h"
#endif

int main(int argc, char** argv)
{
    QStringList args;
    for (int i = 0; i < argc; ++i) args << QString::fromLocal8Bit(argv[i]);
    const bool headless = cactus::cli::isHeadless(args);

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    // Batch mode must work on servers without a display.
    if (headless && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && qEnvironmentVariableIsEmpty("DISPLAY") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QCoreApplication::setApplicationName(QStringLiteral(CACTUS_APP_NAME));
    QCoreApplication::setOrganizationName(QStringLiteral("CactusPlot"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("cactusplot.org"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CACTUS_VERSION));

#ifdef CACTUS_HAVE_GUI
    if (!headless) {
        QApplication app(argc, argv);
        cactus::fonts::registerBundledFonts();
        cactus::cli::Outcome o = cactus::cli::process(args);
        if (o.done || !o.launchGui) return o.exitCode;
        return cactus::ui::runGui(app, o);
    }
#endif
    QGuiApplication app(argc, argv);
    cactus::fonts::registerBundledFonts();
    cactus::cli::Outcome o = cactus::cli::process(args);
    if (!o.done && o.launchGui) {
#ifndef CACTUS_HAVE_GUI
        if (!o.done) {
            fprintf(stderr, "This build has no graphical interface; use -hardcopy or --export.\n");
            return 2;
        }
#endif
    }
    return o.exitCode;
}
