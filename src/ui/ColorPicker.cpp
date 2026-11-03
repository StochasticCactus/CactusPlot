// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/ColorPicker.h"

#include <QColorDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/color/Colors.h"
#include "ui/Theme.h"

namespace cactus::ui {

namespace {
QList<QColor>& recentColors()
{
    static QList<QColor> r = [] {
        QList<QColor> l;
        QSettings s;
        for (const QString& n : s.value(QStringLiteral("colors/recent")).toStringList()) {
            QColor c;
            if (colors::parse(n, &c)) l << c;
        }
        return l;
    }();
    return r;
}

QToolButton* swatchButton(const QColor& c, const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setAutoRaise(true);
    b->setIcon(icons::swatch(c, 18));
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tip.isEmpty() ? colors::displayName(c) : tip + QStringLiteral(" (") + colors::toHex(c) + QLatin1Char(')'));
    b->setFixedSize(24, 24);
    return b;
}
}  // namespace

void ColorPopup::addRecent(const QColor& c)
{
    auto& r = recentColors();
    r.removeAll(c);
    r.prepend(c);
    while (r.size() > 16) r.removeLast();
    QStringList names;
    for (const QColor& x : r) names << colors::toHex(x);
    QSettings().setValue(QStringLiteral("colors/recent"), names);
}

ColorPopup::ColorPopup(const QColor& current, QWidget* parent) : QFrame(parent, Qt::Popup), m_current(current)
{
    setFrameShape(QFrame::StyledPanel);
    setAttribute(Qt::WA_DeleteOnClose);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    // recent colours + quick standard colours
    auto* top = new QHBoxLayout;
    top->setSpacing(2);
    auto* lbl = new QLabel(tr("Recent:"), this);
    top->addWidget(lbl);
    const auto& rec = recentColors();
    for (int i = 0; i < std::min(10, int(rec.size())); ++i) {
        auto* b = swatchButton(rec[i], QString(), this);
        const QColor c = rec[i];
        connect(b, &QToolButton::clicked, this, [this, c] { choose(c); });
        top->addWidget(b);
    }
    top->addStretch(1);
    auto* none = new QPushButton(tr("Transparent"), this);
    none->setFlat(true);
    connect(none, &QPushButton::clicked, this, [this] { choose(QColor(0, 0, 0, 0)); });
    top->addWidget(none);
    lay->addLayout(top);

    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    tabs->addTab(buildPalettes(), tr("Palettes"));
    tabs->addTab(buildNamed(), tr("Named (%1)").arg(colors::namedColors().size()));
    m_dialog = new QColorDialog(current, this);
    m_dialog->setWindowFlags(Qt::Widget);
    m_dialog->setOptions(QColorDialog::NoButtons | QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    connect(m_dialog, &QColorDialog::currentColorChanged, this, [this](const QColor& c) {
        m_current = c;
        emit colorPreview(c);
    });
    auto* custom = new QWidget(this);
    auto* cl = new QVBoxLayout(custom);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->addWidget(m_dialog);
    auto* apply = new QPushButton(tr("Use this colour"), custom);
    connect(apply, &QPushButton::clicked, this, [this] { choose(m_dialog->currentColor()); });
    cl->addWidget(apply, 0, Qt::AlignRight);
    tabs->addTab(custom, tr("Custom"));
    lay->addWidget(tabs, 1);

    // opacity slider applies to palette/named picks
    auto* ar = new QHBoxLayout;
    ar->addWidget(new QLabel(tr("Opacity:"), this));
    m_alpha = new QSlider(Qt::Horizontal, this);
    m_alpha->setRange(0, 255);
    m_alpha->setValue(current.alpha());
    auto* alphaLabel = new QLabel(QStringLiteral("%1%").arg(qRound(current.alphaF() * 100)), this);
    alphaLabel->setMinimumWidth(36);
    connect(m_alpha, &QSlider::valueChanged, this, [this, alphaLabel](int v) {
        alphaLabel->setText(QStringLiteral("%1%").arg(qRound(v / 2.55)));
        QColor c = m_current;
        c.setAlpha(v);
        m_current = c;
        emit colorPreview(c);
    });
    connect(m_alpha, &QSlider::sliderReleased, this, [this] { emit colorPicked(m_current); });
    ar->addWidget(m_alpha, 1);
    ar->addWidget(alphaLabel);
    lay->addLayout(ar);
    resize(520, 460);
}

void ColorPopup::choose(const QColor& c0)
{
    QColor c = c0;
    if (c0.alpha() == 255 && m_alpha && m_alpha->value() < 255 && c0 != QColor(0, 0, 0, 0)) c.setAlpha(m_alpha->value());
    addRecent(c);
    emit colorPicked(c);
    close();
}

QWidget* ColorPopup::buildPalettes()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* w = new QWidget(scroll);
    auto* grid = new QGridLayout(w);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setHorizontalSpacing(2);
    grid->setVerticalSpacing(2);
    int row = 0;
    QString group;
    for (const Palette& p : colors::palettes()) {
        if (p.group != group) {
            group = p.group;
            auto* h = new QLabel(group, w);
            h->setObjectName(QStringLiteral("sectionHeader"));
            grid->addWidget(h, row++, 0, 1, 17);
        }
        auto* name = new QLabel(p.name, w);
        name->setMinimumWidth(110);
        grid->addWidget(name, row, 0);
        const int count = std::min(16, int(p.colors.size()));
        for (int i = 0; i < count; ++i) {
            const QColor c = p.colors[i];
            auto* b = swatchButton(c, QStringLiteral("%1 #%2").arg(p.name).arg(i + 1), w);
            connect(b, &QToolButton::clicked, this, [this, c] { choose(c); });
            grid->addWidget(b, row, 1 + i);
        }
        ++row;
    }
    // a strip from each perceptual colormap
    auto* h = new QLabel(tr("Colormaps"), w);
    h->setObjectName(QStringLiteral("sectionHeader"));
    grid->addWidget(h, row++, 0, 1, 17);
    for (const Colormap& m : colors::colormaps()) {
        if (m.category != QLatin1String("Perceptual") && m.source != QLatin1String("cmocean")) continue;
        auto* name = new QLabel(m.name, w);
        grid->addWidget(name, row, 0);
        for (int i = 0; i < 16; ++i) {
            const QColor c = m.at(i / 15.0);
            auto* b = swatchButton(c, m.name, w);
            connect(b, &QToolButton::clicked, this, [this, c] { choose(c); });
            grid->addWidget(b, row, 1 + i);
        }
        ++row;
    }
    grid->setRowStretch(row, 1);
    grid->setColumnStretch(18, 1);
    scroll->setWidget(w);
    return scroll;
}

QWidget* ColorPopup::buildNamed()
{
    auto* w = new QWidget(this);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(4, 4, 4, 4);
    m_search = new QLineEdit(w);
    m_search->setPlaceholderText(tr("Search colour names (CSS, X11, xkcd, Tableau, Grace)..."));
    m_search->setClearButtonEnabled(true);
    l->addWidget(m_search);
    m_list = new QListWidget(w);
    m_list->setViewMode(QListView::ListMode);
    m_list->setUniformItemSizes(true);
    l->addWidget(m_list, 1);
    auto fill = [this](const QString& filter) {
        m_list->clear();
        int n = 0;
        for (const NamedColor& c : colors::namedColors()) {
            if (!filter.isEmpty() && !c.name.contains(filter, Qt::CaseInsensitive) &&
                !c.source.contains(filter, Qt::CaseInsensitive) && !colors::toHex(c.color).contains(filter, Qt::CaseInsensitive))
                continue;
            auto* it = new QListWidgetItem(icons::swatch(c.color, 16),
                                           QStringLiteral("%1   — %2 %3").arg(c.name, c.source, c.color.name()), m_list);
            it->setData(Qt::UserRole, c.color);
            if (++n > 2500) break;
        }
    };
    fill(QString());
    connect(m_search, &QLineEdit::textChanged, this, fill);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) { choose(it->data(Qt::UserRole).value<QColor>()); });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) { choose(it->data(Qt::UserRole).value<QColor>()); });
    return w;
}

// ---------------------------------------------------------------- button

ColorButton::ColorButton(QWidget* parent) : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(110);
    connect(this, &QToolButton::clicked, this, &ColorButton::openPopup);
    updateLook();
}

void ColorButton::setColor(const QColor& c)
{
    m_color = c;
    updateLook();
}

void ColorButton::updateLook()
{
    setIcon(icons::swatch(m_color, 16));
    QString name = m_color.alpha() == 0 ? tr("none") : colors::displayName(m_color);
    if (m_color.alpha() > 0 && m_color.alpha() < 255) name += QStringLiteral(" (%1%)").arg(qRound(m_color.alphaF() * 100));
    setText(name);
    setToolTip(colors::toHex(m_color));
}

void ColorButton::openPopup()
{
    auto* pop = new ColorPopup(m_color, this);
    connect(pop, &ColorPopup::colorPicked, this, [this](const QColor& c) {
        m_color = c;
        updateLook();
        emit colorChanged(c);
    });
    connect(pop, &ColorPopup::colorPreview, this, [this](const QColor& c) {
        m_color = c;
        updateLook();
        emit colorChanged(c);
    });
    QPoint pos = mapToGlobal(QPoint(0, height()));
    if (QScreen* s = screen()) {
        const QRect avail = s->availableGeometry();
        if (pos.x() + pop->width() > avail.right()) pos.setX(avail.right() - pop->width());
        if (pos.y() + pop->height() > avail.bottom()) pos.setY(mapToGlobal(QPoint(0, 0)).y() - pop->height());
    }
    pop->move(pos);
    pop->show();
}

}  // namespace cactus::ui
