// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QSvgRenderer>

#include "core/color/Colors.h"

static void cactusInitAppResources()
{
    static bool done = false;
    if (!done) {
        Q_INIT_RESOURCE(app);
        done = true;
    }
}

namespace cactus::ui {

namespace {
bool g_dark = false;
QHash<QString, QIcon> g_icons;

bool systemPrefersDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark) return true;
    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light) return false;
#endif
    const QColor w = QGuiApplication::palette().color(QPalette::Window);
    return w.lightness() < 110;
}

QPalette lightPalette()
{
    QPalette p;
    const QColor window(246, 247, 249), base(255, 255, 255), text(28, 30, 34), mid(222, 225, 230);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(244, 246, 248));
    p.setColor(QPalette::ToolTipBase, QColor(255, 255, 240));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(140, 145, 150));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(250, 251, 252));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Light, Qt::white);
    p.setColor(QPalette::Midlight, QColor(236, 238, 241));
    p.setColor(QPalette::Mid, mid);
    p.setColor(QPalette::Dark, QColor(170, 175, 182));
    p.setColor(QPalette::Shadow, QColor(120, 125, 130));
    p.setColor(QPalette::Highlight, QColor(0, 122, 204));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(0, 102, 204));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(160, 164, 170));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(160, 164, 170));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(160, 164, 170));
    return p;
}

QPalette darkPalette()
{
    QPalette p;
    const QColor window(32, 34, 39), base(24, 26, 30), text(226, 228, 232);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(36, 38, 44));
    p.setColor(QPalette::ToolTipBase, QColor(48, 50, 56));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(120, 124, 130));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(44, 46, 52));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Light, QColor(62, 64, 72));
    p.setColor(QPalette::Midlight, QColor(52, 54, 60));
    p.setColor(QPalette::Mid, QColor(40, 42, 48));
    p.setColor(QPalette::Dark, QColor(18, 19, 22));
    p.setColor(QPalette::Shadow, QColor(0, 0, 0));
    p.setColor(QPalette::Highlight, QColor(46, 140, 220));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(110, 170, 255));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 114, 120));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 114, 120));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(110, 114, 120));
    return p;
}

QString styleSheet(bool dark)
{
    const QString border = dark ? QStringLiteral("#3a3d45") : QStringLiteral("#dcdfe4");
    const QString header = dark ? QStringLiteral("#9aa0aa") : QStringLiteral("#5f6670");
    return QStringLiteral(
               "QToolBar { border: none; spacing: 2px; padding: 3px; }"
               "QToolButton { border-radius: 6px; padding: 4px; }"
               "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text); }"
               "QDockWidget::title { padding: 6px 8px; font-weight: 600; }"
               "QLabel#sectionHeader { color: %2; font-weight: 600; padding-top: 10px; padding-bottom: 2px;"
               "  border-bottom: 1px solid %1; }"
               "QTabWidget::pane { border: 1px solid %1; border-radius: 6px; top: -1px; }"
               "QStatusBar QLabel { padding: 0 6px; }")
        .arg(border, header);
}

}  // namespace

namespace theme {

void apply(QApplication& app, ThemeMode mode, bool nativeStyle)
{
    cactusInitAppResources();
    bool dark = mode == ThemeMode::Dark || (mode == ThemeMode::System && systemPrefersDark());
    g_dark = dark;
#ifdef Q_OS_MACOS
    const bool useNative = nativeStyle || mode == ThemeMode::System;
#else
    const bool useNative = nativeStyle;
#endif
    if (useNative) {
        app.setPalette(QApplication::style()->standardPalette());
    } else {
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        app.setPalette(dark ? darkPalette() : lightPalette());
    }
    app.setStyleSheet(styleSheet(dark));
    icons::reset();
}

bool isDark() { return g_dark; }

QColor accent() { return g_dark ? QColor(76, 160, 255) : QColor(0, 120, 215); }

QColor canvasBackground() { return g_dark ? QColor(22, 23, 27) : QColor(229, 231, 235); }

}  // namespace theme

namespace icons {

void reset() { g_icons.clear(); }

QIcon get(const QString& name)
{
    cactusInitAppResources();
    auto it = g_icons.constFind(name);
    if (it != g_icons.constEnd()) return *it;
    QFile f(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly)) return QIcon();
    const QByteArray svg = f.readAll();
    QIcon icon;
    const QColor normal = theme::isDark() ? QColor(222, 225, 230) : QColor(55, 60, 68);
    const QColor disabled = theme::isDark() ? QColor(100, 104, 110) : QColor(175, 180, 186);
    const QColor active = QColor(Qt::white);
    for (int size : {16, 20, 24, 32, 48}) {
        for (int mode = 0; mode < 3; ++mode) {
            const QColor c = mode == 0 ? normal : mode == 1 ? disabled : active;
            QByteArray data = svg;
            data.replace("currentColor", c.name().toLatin1());
            QSvgRenderer r(data);
            QPixmap pm(size * 2, size * 2);
            pm.setDevicePixelRatio(2.0);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            r.render(&p, QRectF(0, 0, size, size));
            p.end();
            if (mode == 0) icon.addPixmap(pm, QIcon::Normal, QIcon::Off);
            else if (mode == 1) icon.addPixmap(pm, QIcon::Disabled, QIcon::Off);
            else icon.addPixmap(pm, QIcon::Normal, QIcon::On);
        }
    }
    g_icons.insert(name, icon);
    return icon;
}

QIcon appIcon()
{
    cactusInitAppResources();
    return QIcon(QStringLiteral(":/cactusplot.svg"));
}

QIcon swatch(const QColor& c, int size)
{
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(1, 1, size - 2, size - 2);
    if (c.alpha() < 255) {
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(r, 3, 3);
        p.setClipPath(clip);
        const int s = std::max(2, size / 4);
        for (int y = 0; y < size; y += s)
            for (int x = 0; x < size; x += s)
                p.fillRect(QRectF(x, y, s, s), ((x + y) / s) % 2 ? QColor(200, 200, 200) : Qt::white);
        p.restore();
    }
    p.setPen(QPen(theme::isDark() ? QColor(120, 120, 130) : QColor(150, 150, 160), 1));
    p.setBrush(c);
    p.drawRoundedRect(r, 3, 3);
    p.end();
    return QIcon(pm);
}

QIcon colormapIcon(const QString& name, int w, int h)
{
    const Colormap* cm = colors::colormap(name);
    QPixmap pm(w * 2, h * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    if (!cm) return QIcon(pm);
    QPainter p(&pm);
    for (int x = 0; x < w * 2; ++x) {
        p.setPen(cm->at(double(x) / (w * 2 - 1)));
        p.drawLine(QPointF(x / 2.0, 0), QPointF(x / 2.0, h));
    }
    p.end();
    return QIcon(pm);
}

}  // namespace icons
}  // namespace cactus::ui
