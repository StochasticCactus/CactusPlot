// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - GUI entry point.
#pragma once

#include <QApplication>

#include "app/Cli.h"

namespace cactus::ui {

/// Create the main window for the processed command line and run the event loop.
int runGui(QApplication& app, cli::Outcome& outcome);

}  // namespace cactus::ui
