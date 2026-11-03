// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - rich text: Grace escape sequences plus a $...$ TeX subset.
#pragma once

#include <QColor>
#include <QFont>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QVector>

#include "core/Model.h"

class QPainter;
class QPaintDevice;

namespace cactus {

struct TextRunLayout {
    QString text;
    QFont font;
    QColor color;
    QPointF pos;  ///< baseline origin relative to the layout origin (y down)
    double width = 0.0;
    double ascent = 0.0, descent = 0.0;
    bool underline = false, overline = false;
    QTransform xf;  ///< glyph transformation (Grace \\t, \\r, \\l escapes)
};

struct TextLayout {
    QVector<TextRunLayout> runs;
    QRectF bounds;          ///< ink-ish bounds relative to the origin (first baseline, left)
    double width = 0.0;
    double firstAscent = 0.0;
    bool isEmpty() const { return runs.isEmpty(); }
};

namespace richtext {

/// Convert $...$ TeX-like math into Grace markup (used before layout).
QString texToGrace(const QString& text);
/// Strip all markup, returning plain text (for tooltips, file names).
QString plainText(const QString& markup);

/// Lay out markup with a base style. `sizeUnits` is the font size in painter units.
TextLayout layout(const QString& markup, const QString& baseFont, double sizeUnits, const QColor& color,
                  const Project* project, const QPaintDevice* device);

/// Draw a layout. `anchor` is in painter coordinates; `angleDeg` counter-clockwise;
/// `hjust`: 0 left, 1 right, 2 centre; `vjust`: 0 baseline, 1 bottom, 2 middle, 3 top.
void draw(QPainter* p, const TextLayout& l, const QPointF& anchor, double angleDeg, int hjust, int vjust);
/// Bounding polygon (painter coordinates) the text would occupy when drawn.
QRectF boundingRect(const TextLayout& l, const QPointF& anchor, double angleDeg, int hjust, int vjust);

/// Convenience: layout + draw.
QRectF drawText(QPainter* p, const QString& markup, const TextStyle& style, double sizeUnits, const Project* project,
                const QPointF& anchor, double angleDeg, int hjust, int vjust);

}  // namespace richtext
}  // namespace cactus
