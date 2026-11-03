// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - plot style presets. "Modern" is the default for new projects;
// "Grace Classic" reproduces xmgrace's look and is used for .agr imports.
#pragma once

#include <QString>
#include <QStringList>

#include "core/Model.h"

namespace cactus::presets {

QStringList names();
QString defaultName();  ///< "Modern"
QString description(const QString& name);

/// A fresh project with one graph styled with the preset.
Project newProject(const QString& preset = defaultName());

/// Style a graph (keeps world/view/titles/labels).
void styleGraph(Graph& g, const QString& preset);
/// Style a set with the preset's colour/symbol cycle at position `index`.
void styleSet(DataSet& s, int index, const QString& preset);
/// Apply a preset to an entire project, keeping data, text and structure.
void applyToProject(Project& p, const QString& preset);
/// Recolour all sets from a qualitative palette (cycling).
void applyPalette(Project& p, const QString& paletteName, int graph = -1);

/// Grace defaults used when interpreting partial parameter files.
void graceGraphDefaults(Graph& g);
void graceSetDefaults(DataSet& s, int index);

/// Default palette for a preset.
QVector<QColor> presetPalette(const QString& preset);

}  // namespace cactus::presets
