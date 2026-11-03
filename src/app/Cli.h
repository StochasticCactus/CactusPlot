// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - Grace-compatible command line processing.
#pragma once

#include <QSize>
#include <QString>
#include <QStringList>

#include "core/Model.h"
#include "grace/Interpreter.h"

namespace cactus::cli {

struct Outcome {
    Project project;
    QString projectPath;     ///< file to associate with the GUI document (if a project was loaded)
    bool launchGui = true;
    int exitCode = 0;
    bool done = false;       ///< nothing else to do (help/version printed, or hardcopy finished)
    QStringList openInGui;   ///< extra files for the GUI
    QSize fixedCanvas;
    bool barebones = false;
    QStringList errors;      ///< problems to show the user (e.g. a file that could not be read)
};

/// Should this invocation run without a GUI? (checked before creating the Qt application)
bool isHeadless(const QStringList& args);

/// Process the arguments (args[0] is the program name). Requires a Q(Gui)Application.
Outcome process(const QStringList& args);

QString usage(const QString& program);
QString versionText();

}  // namespace cactus::cli
