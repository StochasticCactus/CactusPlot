// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/Form.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTimer>

#include "core/color/Colors.h"
#include "plot/Fonts.h"
#include "render/PlotRenderer.h"
#include "ui/ColorPicker.h"
#include "ui/Document.h"
#include "ui/Theme.h"

namespace cactus::ui {

namespace {

class PlainTextCommitter : public QObject {
public:
    PlainTextCommitter(QPlainTextEdit* e, std::function<void()> commit) : QObject(e), m_commit(std::move(commit))
    {
        e->installEventFilter(this);
    }
    bool eventFilter(QObject* o, QEvent* ev) override
    {
        if (ev->type() == QEvent::FocusOut) m_commit();
        return QObject::eventFilter(o, ev);
    }

private:
    std::function<void()> m_commit;
};

QIcon lineIcon(int style)
{
    QPixmap pm(120, 24);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(theme::isDark() ? QColor(220, 220, 225) : QColor(40, 40, 45), 1.6);
    const auto dp = PlotRenderer::dashPattern(style);
    if (!dp.isEmpty()) {
        pen.setCapStyle(Qt::FlatCap);
        pen.setDashPattern(dp);
    }
    if (style > 0) {
        p.setPen(pen);
        p.drawLine(QPointF(2, 6), QPointF(58, 6));
    }
    p.end();
    return QIcon(pm);
}

QIcon patternIcon(int pattern)
{
    QPixmap pm(32, 32);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::white);
    QPainter p(&pm);
    p.setPen(QPen(QColor(120, 120, 120), 0.5));
    p.setBrush(PlotRenderer::makeBrush(FillStyle{QColor(30, 30, 30), pattern}));
    p.drawRect(QRectF(0.5, 0.5, 15, 15));
    p.end();
    return QIcon(pm);
}

}  // namespace

Form::Form(Document* doc, QWidget* parent) : QWidget(parent), m_doc(doc)
{
    m_layout = new QFormLayout(this);
    m_layout->setContentsMargins(8, 4, 8, 8);
    m_layout->setHorizontalSpacing(10);
    m_layout->setVerticalSpacing(5);
    m_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

void Form::apply(const QString& label, const std::function<void(Project&)>& fn, bool merge)
{
    if (m_updating) return;
    if (m_valid && !m_valid(m_doc->project())) return;
    m_doc->modify(tr("Change %1").arg(label), fn,
                  merge ? QStringLiteral("form:%1:%2").arg(quintptr(this)).arg(label) : QString());
}

void Form::section(const QString& title)
{
    auto* l = new QLabel(title, this);
    l->setObjectName(QStringLiteral("sectionHeader"));
    m_layout->addRow(l);
    ++m_sectionCount;
}

void Form::row(const QString& label, QWidget* w) { m_layout->addRow(label, w); }
void Form::widget(QWidget* w) { m_layout->addRow(w); }

QLabel* Form::note(const QString& text)
{
    auto* l = new QLabel(text, this);
    l->setWordWrap(true);
    QFont f = l->font();
    f.setPointSizeF(f.pointSizeF() * 0.9);
    l->setFont(f);
    l->setForegroundRole(QPalette::PlaceholderText);
    m_layout->addRow(l);
    return l;
}

QDoubleSpinBox* Form::real(const QString& label, Get<double> get, Set<double> set, double min, double max, double step,
                           int decimals, const QString& suffix)
{
    auto* w = new QDoubleSpinBox(this);
    w->setRange(min, max);
    w->setSingleStep(step);
    w->setDecimals(decimals);
    w->setKeyboardTracking(false);
    w->setAccelerated(true);
    if (!suffix.isEmpty()) w->setSuffix(QLatin1Char(' ') + suffix);
    m_layout->addRow(label, w);
    connect(w, &QDoubleSpinBox::valueChanged, this, [this, label, set](double v) { apply(label, [set, v](Project& p) { set(p, v); }); });
    m_refreshers.push_back([this, w, get] { w->setValue(get(m_doc->project())); });
    return w;
}

QSpinBox* Form::integer(const QString& label, Get<int> get, Set<int> set, int min, int max, const QString& suffix)
{
    auto* w = new QSpinBox(this);
    w->setRange(min, max);
    w->setKeyboardTracking(false);
    if (!suffix.isEmpty()) w->setSuffix(QLatin1Char(' ') + suffix);
    m_layout->addRow(label, w);
    connect(w, &QSpinBox::valueChanged, this, [this, label, set](int v) { apply(label, [set, v](Project& p) { set(p, v); }); });
    m_refreshers.push_back([this, w, get] { w->setValue(get(m_doc->project())); });
    return w;
}

QCheckBox* Form::check(const QString& label, Get<bool> get, Set<bool> set)
{
    auto* w = new QCheckBox(label, this);
    m_layout->addRow(QString(), w);
    connect(w, &QCheckBox::toggled, this, [this, label, set](bool v) { apply(label, [set, v](Project& p) { set(p, v); }, false); });
    m_refreshers.push_back([this, w, get] { w->setChecked(get(m_doc->project())); });
    return w;
}

QComboBox* Form::combo(const QString& label, const QStringList& items, Get<int> get, Set<int> set)
{
    auto* w = new QComboBox(this);
    w->addItems(items);
    w->setMaxVisibleItems(20);
    m_layout->addRow(label, w);
    connect(w, &QComboBox::activated, this, [this, label, set](int v) { apply(label, [set, v](Project& p) { set(p, v); }, false); });
    m_refreshers.push_back([this, w, get] { w->setCurrentIndex(get(m_doc->project())); });
    return w;
}

QLineEdit* Form::text(const QString& label, Get<QString> get, Set<QString> set, const QString& placeholder)
{
    auto* w = new QLineEdit(this);
    w->setPlaceholderText(placeholder);
    w->setClearButtonEnabled(true);
    m_layout->addRow(label, w);
    connect(w, &QLineEdit::editingFinished, this, [this, w, label, set, get] {
        const QString v = w->text();
        if (v == get(m_doc->project())) return;
        apply(label, [set, v](Project& p) { set(p, v); }, false);
    });
    m_refreshers.push_back([this, w, get] {
        const QString v = get(m_doc->project());
        if (w->text() != v) w->setText(v);
    });
    return w;
}

QPlainTextEdit* Form::multiText(const QString& label, Get<QString> get, Set<QString> set)
{
    auto* w = new QPlainTextEdit(this);
    w->setMaximumHeight(80);
    w->setTabChangesFocus(true);
    m_layout->addRow(label, w);
    auto commit = [this, w, label, set, get] {
        const QString v = w->toPlainText();
        if (v == get(m_doc->project())) return;
        apply(label, [set, v](Project& p) { set(p, v); }, false);
    };
    new PlainTextCommitter(w, commit);
    m_refreshers.push_back([this, w, get] {
        const QString v = get(m_doc->project());
        if (w->toPlainText() != v) w->setPlainText(v);
    });
    return w;
}

ColorButton* Form::color(const QString& label, Get<QColor> get, Set<QColor> set)
{
    auto* w = new ColorButton(this);
    m_layout->addRow(label, w);
    connect(w, &ColorButton::colorChanged, this, [this, label, set](const QColor& c) { apply(label, [set, c](Project& p) { set(p, c); }); });
    m_refreshers.push_back([this, w, get] { w->setColor(get(m_doc->project())); });
    return w;
}

QComboBox* Form::font(const QString& label, Get<QString> get, Set<QString> set)
{
    auto* w = new QComboBox(this);
    w->setEditable(true);
    w->setInsertPolicy(QComboBox::NoInsert);
    w->setMaxVisibleItems(24);
    // populate lazily: the system font list can be long
    w->addItems(fonts::standardFontNames());
    QTimer::singleShot(0, w, [w] {
        const QString cur = w->currentText();
        w->blockSignals(true);
        w->clear();
        w->addItems(fonts::availableFontNames());
        w->setCurrentText(cur);
        w->blockSignals(false);
    });
    m_layout->addRow(label, w);
    connect(w, &QComboBox::textActivated, this, [this, label, set](const QString& v) { apply(label, [set, v](Project& p) { set(p, v); }, false); });
    m_refreshers.push_back([this, w, get] { w->setCurrentText(get(m_doc->project())); });
    return w;
}

QComboBox* Form::colormap(const QString& label, Get<QString> get, Set<QString> set)
{
    auto* w = new QComboBox(this);
    w->setIconSize(QSize(96, 14));
    w->setMaxVisibleItems(24);
    QString cat;
    for (const Colormap& m : colors::colormaps()) {
        if (m.category != cat) {
            cat = m.category;
            w->insertSeparator(w->count());
        }
        w->addItem(icons::colormapIcon(m.name), m.name, m.name);
        w->setItemData(w->count() - 1, QStringLiteral("%1 (%2, %3)").arg(m.name, m.category, m.source), Qt::ToolTipRole);
    }
    m_layout->addRow(label, w);
    connect(w, &QComboBox::activated, this, [this, w, label, set](int i) {
        const QString v = w->itemData(i).toString();
        apply(label, [set, v](Project& p) { set(p, v); }, false);
    });
    m_refreshers.push_back([this, w, get] {
        const int i = w->findData(get(m_doc->project()));
        if (i >= 0) w->setCurrentIndex(i);
    });
    return w;
}

QComboBox* Form::lineStyle(const QString& label, Get<int> get, Set<int> set)
{
    auto* w = combo(label, {tr("None"), tr("Solid"), tr("Dotted"), tr("Dashed"), tr("Long dashed"), tr("Dot-dashed"),
                            tr("Long dash-dot"), tr("Dash-dot-dot"), tr("Dash-dash-dot")},
                    get, set);
    w->setIconSize(QSize(60, 12));
    for (int i = 0; i < w->count(); ++i) w->setItemIcon(i, lineIcon(i));
    return w;
}

QComboBox* Form::pattern(const QString& label, Get<int> get, Set<int> set)
{
    QStringList items{tr("None"), tr("Solid")};
    for (int i = 2; i < 32; ++i) items << tr("Pattern %1").arg(i);
    auto* w = combo(label, items, get, set);
    w->setIconSize(QSize(16, 16));
    for (int i = 0; i < w->count(); ++i) w->setItemIcon(i, patternIcon(i));
    return w;
}

void Form::refresh()
{
    m_updating = true;
    const bool ok = !m_valid || m_valid(m_doc->project());
    setEnabled(ok);
    if (ok) {
        for (auto& r : m_refreshers) r();
    }
    m_updating = false;
}

}  // namespace cactus::ui
