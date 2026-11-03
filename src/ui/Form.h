// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - declarative property forms bound to the document.
#pragma once

#include <QStringList>
#include <QWidget>
#include <functional>

#include "core/Model.h"

class QFormLayout;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;

namespace cactus::ui {

class Document;
class ColorButton;

class Form : public QWidget {
    Q_OBJECT
public:
    template <class T>
    using Get = std::function<T(const Project&)>;
    template <class T>
    using Set = std::function<void(Project&, T)>;

    explicit Form(Document* doc, QWidget* parent = nullptr);

    void section(const QString& title);
    QDoubleSpinBox* real(const QString& label, Get<double> get, Set<double> set, double min, double max, double step,
                         int decimals, const QString& suffix = QString());
    QSpinBox* integer(const QString& label, Get<int> get, Set<int> set, int min, int max, const QString& suffix = QString());
    QCheckBox* check(const QString& label, Get<bool> get, Set<bool> set);
    QComboBox* combo(const QString& label, const QStringList& items, Get<int> get, Set<int> set);
    QLineEdit* text(const QString& label, Get<QString> get, Set<QString> set, const QString& placeholder = QString());
    QPlainTextEdit* multiText(const QString& label, Get<QString> get, Set<QString> set);
    ColorButton* color(const QString& label, Get<QColor> get, Set<QColor> set);
    QComboBox* font(const QString& label, Get<QString> get, Set<QString> set);
    QComboBox* colormap(const QString& label, Get<QString> get, Set<QString> set);
    QComboBox* lineStyle(const QString& label, Get<int> get, Set<int> set);
    QComboBox* pattern(const QString& label, Get<int> get, Set<int> set);
    void row(const QString& label, QWidget* w);
    void widget(QWidget* w);
    QLabel* note(const QString& text);

    /// Disable the whole form when the bound target does not exist.
    void setValidity(std::function<bool(const Project&)> v) { m_valid = std::move(v); }
    void refresh();
    Document* document() const { return m_doc; }

private:
    void apply(const QString& label, const std::function<void(Project&)>& fn, bool merge = true);

    Document* m_doc;
    QFormLayout* m_layout;
    QVector<std::function<void()>> m_refreshers;
    std::function<bool(const Project&)> m_valid;
    bool m_updating = false;
    int m_sectionCount = 0;
};

}  // namespace cactus::ui
