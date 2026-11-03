// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - colour button with a popup offering palettes, ~1800 named
// colours, a full HSV/RGB/alpha editor with screen picker and recent colours.
#pragma once

#include <QColor>
#include <QFrame>
#include <QToolButton>

class QLineEdit;
class QListWidget;
class QSlider;
class QColorDialog;
class QHBoxLayout;

namespace cactus::ui {

class ColorPopup : public QFrame {
    Q_OBJECT
public:
    explicit ColorPopup(const QColor& current, QWidget* parent = nullptr);
    static void addRecent(const QColor& c);

signals:
    void colorPicked(const QColor& c);   ///< final choice (closes popup)
    void colorPreview(const QColor& c);  ///< live change from the editor

private:
    void choose(const QColor& c);
    QWidget* buildPalettes();
    QWidget* buildNamed();
    QColor m_current;
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
    QSlider* m_alpha = nullptr;
    QColorDialog* m_dialog = nullptr;
};

class ColorButton : public QToolButton {
    Q_OBJECT
public:
    explicit ColorButton(QWidget* parent = nullptr);
    QColor color() const { return m_color; }
    void setColor(const QColor& c);  ///< programmatic, does not emit

signals:
    void colorChanged(const QColor& c);

private:
    void openPopup();
    void updateLook();
    QColor m_color{Qt::black};
};

}  // namespace cactus::ui
