// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/Dialogs.h"

#include <QCheckBox>
#include <QDir>
#include <QFontDatabase>
#include <iterator>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

#include "core/Presets.h"
#include "grace/Interpreter.h"
#include "io/DataReader.h"
#include "math/Transforms.h"
#include "plot/RichText.h"
#include "render/Export.h"
#include "render/PlotRenderer.h"
#include "ui/Document.h"
#include "ui/Theme.h"

namespace cactus::ui {

void fillSetCombo(QComboBox* combo, const Project& p, int selectGraph, int selectSet)
{
    combo->clear();
    for (int g = 0; g < p.graphs.size(); ++g) {
        if (!p.graphs[g].active) continue;
        for (int s = 0; s < p.graphs[g].sets.size(); ++s) {
            const DataSet& d = p.graphs[g].sets[s];
            if (!d.active) continue;
            QString n = QStringLiteral("G%1.S%2").arg(g).arg(s);
            const QString leg = richtext::plainText(d.legend);
            if (!leg.isEmpty()) n += QStringLiteral("  — ") + leg;
            n += QStringLiteral("  (%1 pts)").arg(d.size());
            combo->addItem(icons::swatch(d.line.color, 12), n, QPoint(g, s));
            if (g == selectGraph && s == selectSet) combo->setCurrentIndex(combo->count() - 1);
        }
    }
}

namespace {

/// Add a new styled XY set; returns its index.
int addResultSet(Project& p, int g, const QVector<double>& x, const QVector<double>& y, const QString& legend, const QString& comment)
{
    Graph* gr = p.graph(g);
    if (!gr) return -1;
    const int si = gr->nextFreeSet();
    DataSet& d = gr->ensureSet(si);
    presets::styleSet(d, si, p.stylePreset);
    d.type = SetType::XY;
    d.cols[0] = x;
    d.cols[1] = y;
    d.legend = legend;
    d.comment = comment;
    return si;
}

QDoubleSpinBox* dspin(double min, double max, double val, int dec = 4, double step = 1.0)
{
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(dec);
    s->setSingleStep(step);
    s->setValue(val);
    return s;
}

QSpinBox* ispin(int min, int max, int val)
{
    auto* s = new QSpinBox;
    s->setRange(min, max);
    s->setValue(val);
    return s;
}

QComboBox* choiceBox(const QStringList& items, int cur = 0)
{
    auto* c = new QComboBox;
    c->addItems(items);
    c->setCurrentIndex(cur);
    return c;
}

}  // namespace

// ====================================================================== transformations

TransformDialog::TransformDialog(Document* doc, QWidget* parent) : QDialog(parent), m_doc(doc)
{
    setWindowTitle(tr("Data transformations"));
    setWindowIcon(icons::get(QStringLiteral("transform")));
    auto* lay = new QVBoxLayout(this);
    auto* split = new QSplitter(this);
    split->setObjectName(QStringLiteral("transformSplit"));
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);  // wide enough to grab on every platform
    m_list = new QListWidget(split);
    auto* right = new QWidget(split);
    auto* rl = new QVBoxLayout(right);
    auto* srcForm = new QFormLayout;
    m_source = new QComboBox(right);
    m_source2 = new QComboBox(right);
    const Selection& sel = doc->selection();
    fillSetCombo(m_source, doc->project(), sel.kind == Selection::Set ? sel.graph : -1, sel.index);
    fillSetCombo(m_source2, doc->project());
    srcForm->addRow(tr("Source set"), m_source);
    srcForm->addRow(tr("Second set"), m_source2);
    m_replace = new QCheckBox(tr("Replace the source set instead of creating a new set"), right);
    srcForm->addRow(QString(), m_replace);
    rl->addLayout(srcForm);
    m_stack = new QStackedWidget(right);
    rl->addWidget(m_stack, 1);
    m_log = new QTextEdit(right);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(120);
    rl->addWidget(m_log);
    split->addWidget(m_list);
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    lay->addWidget(split, 1);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    lay->addWidget(bb);
    connect(bb->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &TransformDialog::apply);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ---- pages
    auto* p = page(tr("Evaluate expression"));
    field(tr("Evaluate expression"), QStringLiteral("x"), new QLineEdit(QStringLiteral("x")));
    field(tr("Evaluate expression"), QStringLiteral("y"), new QLineEdit(QStringLiteral("y")));
    static_cast<QFormLayout*>(p->layout())->addRow(new QLabel(tr("x and y refer to the source set; other sets as s1.y or g1.s0.x. "
                                                                 "Generate data with mesh(start, stop, n), e.g. x = mesh(0, 10, 200), y = sin(x)."), p));
    field(tr("Histogram"), QStringLiteral("bins"), ispin(1, 100000, 20));
    field(tr("Histogram"), QStringLiteral("auto"), new QCheckBox(tr("Automatic range")));
    static_cast<QCheckBox*>(m_fields[tr("Histogram") + QStringLiteral(".auto")])->setChecked(true);
    field(tr("Histogram"), QStringLiteral("min"), dspin(-1e300, 1e300, 0));
    field(tr("Histogram"), QStringLiteral("max"), dspin(-1e300, 1e300, 1));
    field(tr("Histogram"), QStringLiteral("cumulative"), new QCheckBox(tr("Cumulative")));
    field(tr("Histogram"), QStringLiteral("normalize"), new QCheckBox(tr("Normalise (probability density)")));
    field(tr("Histogram"), QStringLiteral("centered"), new QCheckBox(tr("Points at bin centres (Grace uses upper edges)")));
    field(tr("Histogram"), QStringLiteral("bars"), new QCheckBox(tr("Draw as bars")));
    static_cast<QCheckBox*>(m_fields[tr("Histogram") + QStringLiteral(".bars")])->setChecked(true);
    static_cast<QCheckBox*>(m_fields[tr("Histogram") + QStringLiteral(".centered")])->setChecked(true);
    field(tr("Fourier transform"), QStringLiteral("output"), choiceBox({tr("Magnitude"), tr("Phase"), tr("Power"), tr("Real part"), tr("Imaginary part"), tr("Power (dB)")}));
    field(tr("Fourier transform"), QStringLiteral("window"), choiceBox({tr("None (rectangular)"), tr("Triangular"), tr("Hanning"), tr("Welch"), tr("Hamming"), tr("Blackman"), tr("Parzen")}));
    field(tr("Fourier transform"), QStringLiteral("xaxis"), choiceBox({tr("Index"), tr("Frequency"), tr("Period")}, 1));
    field(tr("Fourier transform"), QStringLiteral("inverse"), new QCheckBox(tr("Inverse transform")));
    field(tr("Fourier transform"), QStringLiteral("complex"), new QCheckBox(tr("Complex input (x = real, y = imaginary)")));
    field(tr("Running statistics"), QStringLiteral("stat"), choiceBox({tr("Average"), tr("Median"), tr("Minimum"), tr("Maximum"), tr("Standard deviation")}));
    field(tr("Running statistics"), QStringLiteral("window"), ispin(1, 100000, 5));
    field(tr("Differentiate"), QStringLiteral("type"), choiceBox({tr("Forward difference"), tr("Backward difference"), tr("Centred difference")}, 2));
    page(tr("Integrate"));
    field(tr("Interpolation / splines"), QStringLiteral("method"), choiceBox({tr("Linear"), tr("Cubic spline"), tr("Akima spline")}, 1));
    field(tr("Interpolation / splines"), QStringLiteral("points"), ispin(2, 10000000, 200));
    field(tr("Interpolation / splines"), QStringLiteral("useset"), new QCheckBox(tr("Sample at the x values of the second set")));
    field(tr("Regression"), QStringLiteral("type"), choiceBox({tr("Linear"), tr("Polynomial"), tr("Exponential  y = A e^(Bx)"), tr("Power  y = A x^B"),
                                                               tr("Logarithmic  y = A + B ln x"), tr("Inverse  y = 1/(A + Bx)"), tr("Saturation  y = A(1 - e^(-Bx))")}));
    field(tr("Regression"), QStringLiteral("degree"), ispin(1, 10, 2));
    field(tr("Regression"), QStringLiteral("output"), choiceBox({tr("Fitted curve"), tr("Fitted values at data x"), tr("Residuals")}));
    field(tr("Correlation"), QStringLiteral("lag"), ispin(0, 1000000, 50));
    field(tr("Correlation"), QStringLiteral("covariance"), new QCheckBox(tr("Covariance instead of correlation")));
    page(tr("Convolution"));
    page(tr("Digital filter"));
    static_cast<QFormLayout*>(page(tr("Digital filter"))->layout())->addRow(new QLabel(tr("The y values of the second set are the filter weights.")));
    field(tr("Geometric transform"), QStringLiteral("rot"), dspin(-360, 360, 0, 3));
    field(tr("Geometric transform"), QStringLiteral("cx"), dspin(-1e300, 1e300, 0));
    field(tr("Geometric transform"), QStringLiteral("cy"), dspin(-1e300, 1e300, 0));
    field(tr("Geometric transform"), QStringLiteral("sx"), dspin(-1e300, 1e300, 1));
    field(tr("Geometric transform"), QStringLiteral("sy"), dspin(-1e300, 1e300, 1));
    field(tr("Geometric transform"), QStringLiteral("tx"), dspin(-1e300, 1e300, 0));
    field(tr("Geometric transform"), QStringLiteral("ty"), dspin(-1e300, 1e300, 0));
    field(tr("Sample points"), QStringLiteral("start"), ispin(0, 100000000, 0));
    field(tr("Sample points"), QStringLiteral("step"), ispin(1, 100000000, 2));
    field(tr("Prune"), QStringLiteral("type"), choiceBox({tr("Interpolation"), tr("Circle"), tr("Ellipse"), tr("Rectangle")}));
    field(tr("Prune"), QStringLiteral("dx"), dspin(0, 1e300, 0.01, 6));
    field(tr("Prune"), QStringLiteral("dy"), dspin(0, 1e300, 0.01, 6));
    field(tr("Prune"), QStringLiteral("relative"), new QCheckBox(tr("Relative tolerances")));
    field(tr("Seasonal differences"), QStringLiteral("period"), ispin(1, 1000000, 12));
    page(tr("Cumulative sum"));
    field(tr("Sort"), QStringLiteral("key"), choiceBox({tr("By X"), tr("By Y")}));
    field(tr("Sort"), QStringLiteral("desc"), new QCheckBox(tr("Descending")));
    page(tr("Reverse"));
    field(tr("Restrict"), QStringLiteral("cond"), new QLineEdit(QStringLiteral("y > 0")));
    field(tr("Restrict"), QStringLiteral("region"), choiceBox({tr("Use the condition"), QStringLiteral("R0"), QStringLiteral("R1"), QStringLiteral("R2"), QStringLiteral("R3"), QStringLiteral("R4")}));
    field(tr("Restrict"), QStringLiteral("negate"), new QCheckBox(tr("Keep points outside the region")));
    field(tr("Feature extraction"), QStringLiteral("feature"), choiceBox(transforms::featureNames()));
    static_cast<QFormLayout*>(page(tr("Feature extraction"))->layout())->addRow(new QLabel(tr("Computes the feature for every set of the source graph and creates a set (set index, value).")));
    page(tr("Statistics"));

    connect(m_list, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) {
        const QString n = m_list->currentItem() ? m_list->currentItem()->text() : QString();
        const bool two = n == tr("Correlation") || n == tr("Convolution") || n == tr("Digital filter") || n == tr("Interpolation / splines");
        m_source2->setEnabled(two);
        const bool inPlace = n == tr("Sort") || n == tr("Reverse") || n == tr("Restrict");
        m_replace->setEnabled(!inPlace && n != tr("Statistics") && n != tr("Feature extraction"));
    });
    m_list->setCurrentRow(0);

    // the list is as wide as its longest entry; long help texts wrap instead of widening the window
    m_list->setMinimumWidth(m_list->sizeHintForColumn(0) + 2 * m_list->frameWidth() + 24);
    m_list->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    for (QLabel* l : m_stack->findChildren<QLabel*>())
        if (!l->text().startsWith(QLatin1String("<b>"))) l->setWordWrap(true);
    QSettings st;
    if (!restoreGeometry(st.value(QStringLiteral("windows/transform")).toByteArray())) resize(900, 600);
    if (!split->restoreState(st.value(QStringLiteral("windows/transformSplit")).toByteArray()))
        split->setSizes({m_list->minimumWidth(), 900 - m_list->minimumWidth()});
    connect(this, &QDialog::finished, this, [this, split] {
        QSettings st;
        st.setValue(QStringLiteral("windows/transform"), saveGeometry());
        st.setValue(QStringLiteral("windows/transformSplit"), split->saveState());
    });
}

QWidget* TransformDialog::page(const QString& name)
{
    if (m_pageIndex.contains(name)) return m_stack->widget(m_pageIndex.value(name));
    auto* w = new QWidget(m_stack);
    auto* f = new QFormLayout(w);
    f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto* title = new QLabel(QStringLiteral("<b>%1</b>").arg(name), w);
    f->addRow(title);
    m_pageIndex.insert(name, m_stack->addWidget(w));
    m_list->addItem(name);
    return w;
}

QWidget* TransformDialog::field(const QString& pageName, const QString& key, QWidget* w)
{
    QWidget* p = page(pageName);
    auto* f = static_cast<QFormLayout*>(p->layout());
    static const QHash<QString, QString> labels = {
        {QStringLiteral("x"), QObject::tr("x =")}, {QStringLiteral("y"), QObject::tr("y =")},
        {QStringLiteral("bins"), QObject::tr("Number of bins")}, {QStringLiteral("min"), QObject::tr("Minimum")},
        {QStringLiteral("max"), QObject::tr("Maximum")}, {QStringLiteral("output"), QObject::tr("Output")},
        {QStringLiteral("window"), QObject::tr("Window")}, {QStringLiteral("xaxis"), QObject::tr("X axis")},
        {QStringLiteral("stat"), QObject::tr("Statistic")}, {QStringLiteral("type"), QObject::tr("Type")},
        {QStringLiteral("method"), QObject::tr("Method")}, {QStringLiteral("points"), QObject::tr("Points")},
        {QStringLiteral("degree"), QObject::tr("Degree")}, {QStringLiteral("lag"), QObject::tr("Maximum lag")},
        {QStringLiteral("rot"), QObject::tr("Rotation (°)")}, {QStringLiteral("cx"), QObject::tr("Centre x")},
        {QStringLiteral("cy"), QObject::tr("Centre y")}, {QStringLiteral("sx"), QObject::tr("Scale x")},
        {QStringLiteral("sy"), QObject::tr("Scale y")}, {QStringLiteral("tx"), QObject::tr("Shift x")},
        {QStringLiteral("ty"), QObject::tr("Shift y")}, {QStringLiteral("start"), QObject::tr("Start index")},
        {QStringLiteral("step"), QObject::tr("Every n-th point")}, {QStringLiteral("dx"), QObject::tr("Tolerance x")},
        {QStringLiteral("dy"), QObject::tr("Tolerance y")}, {QStringLiteral("period"), QObject::tr("Period (points)")},
        {QStringLiteral("key"), QObject::tr("Sort key")}, {QStringLiteral("cond"), QObject::tr("Keep where")},
        {QStringLiteral("region"), QObject::tr("Region")}, {QStringLiteral("feature"), QObject::tr("Feature")}};
    if (qobject_cast<QCheckBox*>(w)) f->addRow(QString(), w);
    else f->addRow(labels.value(key, key), w);
    m_fields.insert(pageName + QLatin1Char('.') + key, w);
    return w;
}

void TransformDialog::showPage(const QString& name)
{
    const auto items = m_list->findItems(name, Qt::MatchExactly);
    if (!items.isEmpty()) m_list->setCurrentItem(items.first());
}

double TransformDialog::num(const QString& key) const
{
    QWidget* w = m_fields.value(m_list->currentItem()->text() + QLatin1Char('.') + key);
    if (auto* d = qobject_cast<QDoubleSpinBox*>(w)) return d->value();
    if (auto* i = qobject_cast<QSpinBox*>(w)) return i->value();
    return 0.0;
}
int TransformDialog::integer(const QString& key) const { return int(num(key)); }
QString TransformDialog::str(const QString& key) const
{
    auto* e = qobject_cast<QLineEdit*>(m_fields.value(m_list->currentItem()->text() + QLatin1Char('.') + key));
    return e ? e->text() : QString();
}
bool TransformDialog::flag(const QString& key) const
{
    auto* c = qobject_cast<QCheckBox*>(m_fields.value(m_list->currentItem()->text() + QLatin1Char('.') + key));
    return c && c->isChecked();
}
int TransformDialog::choice(const QString& key) const
{
    auto* c = qobject_cast<QComboBox*>(m_fields.value(m_list->currentItem()->text() + QLatin1Char('.') + key));
    return c ? c->currentIndex() : 0;
}

void TransformDialog::apply()
{
    if (!m_list->currentItem() || m_source->currentIndex() < 0) {
        m_log->append(tr("Select a source set first."));
        return;
    }
    const QString name = m_list->currentItem()->text();
    const QPoint gs = m_source->currentData().toPoint();
    const QPoint gs2 = m_source2->currentData().toPoint();
    const Project& pr = m_doc->project();
    const Graph* g = pr.graph(gs.x());
    const DataSet* src = g ? g->set(gs.y()) : nullptr;
    const Graph* g2 = pr.graph(gs2.x());
    const DataSet* src2 = g2 ? g2->set(gs2.y()) : nullptr;
    if (!src) return;
    const QVector<double> X = src->cols[0], Y = src->cols[1];
    transforms::XY out;
    QString legend;
    bool inPlace = false;
    bool asBars = false;

    if (name == tr("Evaluate expression")) {
        Project tmp = pr;
        Interpreter in(&tmp);
        expr::Value vx, vy;
        QString err;
        if (!in.evaluate(str(QStringLiteral("x")), &vx, &err, gs.x(), gs.y()) || !in.evaluate(str(QStringLiteral("y")), &vy, &err, gs.x(), gs.y())) {
            m_log->append(tr("Error: %1").arg(err));
            return;
        }
        int n = std::max(vx.size(), vy.size());
        out.x = vx.toVector(n);
        out.y = vy.toVector(n);
        out.x.resize(n);
        out.y.resize(n);
        legend = QStringLiteral("y = %1").arg(str(QStringLiteral("y")));
    } else if (name == tr("Histogram")) {
        double lo = num(QStringLiteral("min")), hi = num(QStringLiteral("max"));
        if (flag(QStringLiteral("auto"))) {
            const auto st = transforms::statistics(Y);
            lo = st.min;
            hi = st.max;
        }
        out = transforms::histogram(Y, transforms::linspace(lo, hi, integer(QStringLiteral("bins")) + 1), flag(QStringLiteral("cumulative")),
                                    flag(QStringLiteral("normalize")), flag(QStringLiteral("centered")));
        asBars = flag(QStringLiteral("bars"));
        legend = tr("Histogram of S%1").arg(gs.y());
    } else if (name == tr("Fourier transform")) {
        transforms::FftOptions o;
        static const transforms::FftOutput outs[] = {transforms::FftOutput::Magnitude, transforms::FftOutput::Phase, transforms::FftOutput::Power,
                                                     transforms::FftOutput::Real, transforms::FftOutput::Imaginary, transforms::FftOutput::DB};
        o.output = outs[choice(QStringLiteral("output"))];
        o.window = transforms::FftWindow(choice(QStringLiteral("window")));
        o.xaxis = transforms::FftXAxis(choice(QStringLiteral("xaxis")));
        o.inverse = flag(QStringLiteral("inverse"));
        o.complexInput = flag(QStringLiteral("complex"));
        out = transforms::fourier(X, Y, o);
        legend = tr("Spectrum of S%1").arg(gs.y());
    } else if (name == tr("Running statistics")) {
        out = transforms::running(X, Y, integer(QStringLiteral("window")), transforms::RunStat(choice(QStringLiteral("stat"))));
        legend = tr("Running %1 of S%2").arg(qobject_cast<QComboBox*>(m_fields.value(name + QStringLiteral(".stat")))->currentText().toLower()).arg(gs.y());
    } else if (name == tr("Differentiate")) {
        out = transforms::differentiate(X, Y, transforms::DiffType(choice(QStringLiteral("type"))));
        legend = tr("d/dx S%1").arg(gs.y());
    } else if (name == tr("Integrate")) {
        double total = 0;
        out = transforms::integrate(X, Y, &total);
        m_log->append(tr("Integral of S%1 = %2").arg(gs.y()).arg(total, 0, 'g', 12));
        legend = tr("∫ S%1 dx").arg(gs.y());
    } else if (name == tr("Interpolation / splines")) {
        QVector<double> mesh;
        if (flag(QStringLiteral("useset")) && src2) mesh = src2->cols[0];
        else {
            const auto st = transforms::statistics(X);
            mesh = transforms::linspace(st.min, st.max, integer(QStringLiteral("points")));
        }
        out = transforms::interpolate(X, Y, mesh, transforms::InterpMethod(choice(QStringLiteral("method"))));
        legend = tr("Interpolation of S%1").arg(gs.y());
    } else if (name == tr("Regression")) {
        static const fit::Regression types[] = {fit::Regression::Linear, fit::Regression::Polynomial, fit::Regression::Exponential,
                                                fit::Regression::Power, fit::Regression::Logarithmic, fit::Regression::Inverse,
                                                fit::Regression::InverseExponential};
        const fit::Regression t = types[choice(QStringLiteral("type"))];
        const auto r = fit::regression(X, Y, t, integer(QStringLiteral("degree")));
        if (!r.ok) {
            m_log->append(tr("Regression failed: %1").arg(r.message));
            return;
        }
        m_log->append(r.report());
        const int o = choice(QStringLiteral("output"));
        if (o == 0) {
            const auto st = transforms::statistics(X);
            out.x = transforms::linspace(st.min, st.max, 300);
            out.y = fit::evaluateRegression(r, t, out.x);
        } else {
            out.x = X;
            out.y = fit::evaluateRegression(r, t, X);
            if (o == 2)
                for (int i = 0; i < out.y.size(); ++i) out.y[i] = Y.value(i) - out.y[i];
        }
        legend = o == 2 ? tr("Residuals of S%1").arg(gs.y()) : QStringLiteral("y = ") + r.formula;
    } else if (name == tr("Correlation")) {
        if (!src2) return;
        out = transforms::crossCorrelation(Y, src2->cols[1], integer(QStringLiteral("lag")), flag(QStringLiteral("covariance")));
        legend = tr("Correlation S%1, S%2").arg(gs.y()).arg(gs2.y());
    } else if (name == tr("Convolution")) {
        if (!src2) return;
        out = transforms::linearConvolution(Y, src2->cols[1]);
        legend = tr("S%1 ∗ S%2").arg(gs.y()).arg(gs2.y());
    } else if (name == tr("Digital filter")) {
        if (!src2) return;
        out = transforms::digitalFilter(X, Y, src2->cols[1]);
        legend = tr("Filtered S%1").arg(gs.y());
    } else if (name == tr("Geometric transform")) {
        out = transforms::geometric(X, Y, num(QStringLiteral("rot")), num(QStringLiteral("cx")), num(QStringLiteral("cy")), num(QStringLiteral("sx")),
                                    num(QStringLiteral("sy")), num(QStringLiteral("tx")), num(QStringLiteral("ty")));
        legend = src->legend;
    } else if (name == tr("Sample points")) {
        out = transforms::sampleEvery(X, Y, integer(QStringLiteral("start")), integer(QStringLiteral("step")));
        legend = src->legend;
    } else if (name == tr("Prune")) {
        out = transforms::prune(X, Y, transforms::PruneType(choice(QStringLiteral("type"))), num(QStringLiteral("dx")), num(QStringLiteral("dy")),
                                flag(QStringLiteral("relative")));
        m_log->append(tr("Pruned %1 → %2 points").arg(X.size()).arg(out.x.size()));
        legend = src->legend;
    } else if (name == tr("Seasonal differences")) {
        out = transforms::seasonalDifference(X, Y, integer(QStringLiteral("period")));
        legend = tr("Seasonal difference of S%1").arg(gs.y());
    } else if (name == tr("Cumulative sum")) {
        out = transforms::cumulativeSum(X, Y);
        legend = tr("Cumulative sum of S%1").arg(gs.y());
    } else if (name == tr("Sort") || name == tr("Reverse") || name == tr("Restrict")) {
        inPlace = true;
        QString cmd;
        const QString ref = QStringLiteral("g%1.s%2").arg(gs.x()).arg(gs.y());
        if (name == tr("Sort")) cmd = QStringLiteral("sort %1 %2 %3").arg(ref, choice(QStringLiteral("key")) ? QStringLiteral("y") : QStringLiteral("x"),
                                                                         flag(QStringLiteral("desc")) ? QStringLiteral("descending") : QString());
        else if (name == tr("Reverse")) cmd = QStringLiteral("reverse %1").arg(ref);
        else if (choice(QStringLiteral("region")) > 0)
            cmd = QStringLiteral("restrict(%1, r%2, %3)").arg(ref).arg(choice(QStringLiteral("region")) - 1).arg(flag(QStringLiteral("negate")) ? QStringLiteral("on") : QStringLiteral("off"));
        else cmd = QStringLiteral("restrict(%1, %2)").arg(ref, str(QStringLiteral("cond")));
        QStringList errs;
        m_doc->modify(name, [&](Project& p) {
            Interpreter in(&p);
            in.setCurrentGraph(gs.x());
            in.execute(cmd);
            errs = in.errors();
        });
        m_log->append(errs.isEmpty() ? tr("%1 applied to S%2").arg(name).arg(gs.y()) : errs.join(QLatin1Char('\n')));
        return;
    } else if (name == tr("Feature extraction")) {
        const QString feat = qobject_cast<QComboBox*>(m_fields.value(name + QStringLiteral(".feature")))->currentText();
        for (int s = 0; s < g->sets.size(); ++s) {
            if (!g->sets[s].active) continue;
            out.x << s;
            out.y << transforms::feature(g->sets[s].cols[0], g->sets[s].cols[1], feat);
        }
        legend = feat;
    } else if (name == tr("Statistics")) {
        const auto sx = transforms::statistics(X), sy = transforms::statistics(Y);
        double total = 0;
        transforms::integrate(X, Y, &total);
        m_log->append(tr("S%1: %2 points").arg(gs.y()).arg(src->size()));
        m_log->append(tr("  X: min %1  max %2  mean %3  sd %4  median %5").arg(sx.min).arg(sx.max).arg(sx.mean).arg(sx.sd).arg(sx.median));
        m_log->append(tr("  Y: min %1  max %2  mean %3  sd %4  median %5  sum %6").arg(sy.min).arg(sy.max).arg(sy.mean).arg(sy.sd).arg(sy.median).arg(sy.sum));
        m_log->append(tr("  Integral (trapezoid): %1").arg(total, 0, 'g', 12));
        return;
    }
    Q_UNUSED(inPlace);
    if (out.x.isEmpty()) {
        m_log->append(tr("%1: no output (check the parameters and the source data).").arg(name));
        return;
    }
    const bool replace = m_replace->isChecked() && m_replace->isEnabled();
    int created = -1;
    m_doc->modify(name, [&](Project& p) {
        if (replace) {
            Graph* gr = p.graph(gs.x());
            if (!gr || !gr->set(gs.y())) return;
            DataSet& d = gr->sets[gs.y()];
            d.setType(SetType::XY);
            d.cols[0] = out.x;
            d.cols[1] = out.y;
            created = gs.y();
        } else {
            created = addResultSet(p, gs.x(), out.x, out.y, legend, name);
            if (asBars && created >= 0) {
                DataSet& d = p.graph(gs.x())->sets[created];
                d.type = SetType::BAR;
                d.symbol.size = 0.8;
                d.symbol.line = LineStyle{d.line.color, 0.8, 1};
                QColor fc = d.line.color;
                fc.setAlpha(170);
                d.symbol.fill = FillStyle{fc, 1};
            }
        }
        if (Graph* gr = p.graph(gs.x())) gr->autoscale();
    });
    if (created >= 0) {
        m_log->append(tr("%1: %2 points written to G%3.S%4").arg(name).arg(out.x.size()).arg(gs.x()).arg(created));
        fillSetCombo(m_source2, m_doc->project());
    }
}

// ====================================================================== fitting

namespace {
struct ModelDef {
    const char* name;
    const char* formula;
};
const ModelDef kModels[] = {
    {"Linear", "a0 + a1*x"},
    {"Quadratic", "a0 + a1*x + a2*x^2"},
    {"Cubic", "a0 + a1*x + a2*x^2 + a3*x^3"},
    {"Exponential decay", "a0*exp(-x/a1) + a2"},
    {"Exponential growth", "a0*exp(a1*x)"},
    {"Gaussian peak", "a0*exp(-(x-a1)^2/(2*a2^2)) + a3"},
    {"Lorentzian peak", "a0*a2^2/((x-a1)^2 + a2^2) + a3"},
    {"Voigt peak", "a0*voigt(a2, a3, x - a1) + a4"},
    {"Logistic (sigmoid)", "a0/(1 + exp(-(x - a1)/a2)) + a3"},
    {"Power law", "a0*x^a1"},
    {"Sine wave", "a0*sin(a1*x + a2) + a3"},
    {"Michaelis-Menten", "a0*x/(a1 + x)"},
    {"Hill", "a0*x^a2/(a1^a2 + x^a2)"},
    {"Arrhenius", "a0*exp(-a1/x)"},
    {"Custom", ""},
};

int paramCountOf(const QString& formula)
{
    static const QRegularExpression re(QStringLiteral("\\ba(\\d)\\b"));
    int n = 0;
    auto it = re.globalMatch(formula);
    while (it.hasNext()) n = std::max(n, it.next().captured(1).toInt() + 1);
    return std::max(1, n);
}
}  // namespace

FitDialog::FitDialog(Document* doc, QWidget* parent) : QDialog(parent), m_doc(doc)
{
    setWindowTitle(tr("Curve fitting"));
    setWindowIcon(icons::get(QStringLiteral("fit")));
    auto* lay = new QHBoxLayout(this);
    auto* split = new QSplitter(this);
    split->setObjectName(QStringLiteral("fitSplit"));
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);
    lay->addWidget(split);
    auto* leftW = new QWidget(split);
    auto* left = new QVBoxLayout(leftW);
    left->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout;
    m_source = new QComboBox(this);
    const Selection& sel = doc->selection();
    fillSetCombo(m_source, doc->project(), sel.kind == Selection::Set ? sel.graph : -1, sel.index);
    form->addRow(tr("Data set"), m_source);
    m_model = new QComboBox(this);
    for (const ModelDef& m : kModels) m_model->addItem(tr(m.name), QString::fromLatin1(m.formula));
    form->addRow(tr("Model"), m_model);
    m_formula = new QLineEdit(this);
    m_formula->setPlaceholderText(tr("y = f(x; a0 ... a9), any expression"));
    form->addRow(tr("y ="), m_formula);
    m_weights = new QComboBox(this);
    m_weights->addItems({tr("None"), tr("1/y"), tr("1/y²"), tr("1/dy² (XYDY sets)")});
    form->addRow(tr("Weights"), m_weights);
    m_iter = new QSpinBox(this);
    m_iter->setRange(1, 100000);
    m_iter->setValue(500);
    form->addRow(tr("Max. iterations"), m_iter);
    m_output = new QComboBox(this);
    m_output->addItems({tr("Smooth fitted curve"), tr("Fitted values at data x"), tr("Residuals")});
    form->addRow(tr("Result set"), m_output);
    left->addLayout(form);
    m_params = new QTableWidget(0, 6, this);
    m_params->setHorizontalHeaderLabels({tr("Initial"), tr("Fixed"), tr("Min"), tr("Max"), tr("Result"), tr("± Std. error")});
    m_params->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_params->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    left->addWidget(m_params, 1);
    auto* btns = new QHBoxLayout;
    auto* guessB = new QPushButton(tr("Guess initial values"), this);
    auto* fitB = new QPushButton(icons::get(QStringLiteral("fit")), tr("Fit"), this);
    fitB->setDefault(true);
    auto* applyB = new QPushButton(tr("Create result set"), this);
    auto* closeB = new QPushButton(tr("Close"), this);
    btns->addWidget(guessB);
    btns->addStretch(1);
    btns->addWidget(fitB);
    btns->addWidget(applyB);
    btns->addWidget(closeB);
    left->addLayout(btns);
    auto* rightW = new QWidget(split);
    auto* right = new QVBoxLayout(rightW);
    right->setContentsMargins(0, 0, 0, 0);
    m_preview = new QLabel(this);
    m_preview->setMinimumSize(240, 160);
    m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);  // the image follows the pane, not the reverse
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(120);
    connect(m_previewTimer, &QTimer::timeout, this, [this] { updatePreview(m_previewParams); });
    m_preview->installEventFilter(this);
    m_preview->setAlignment(Qt::AlignCenter);
    right->addWidget(m_preview, 1);
    m_report = new QPlainTextEdit(this);
    m_report->setReadOnly(true);
    m_report->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    right->addWidget(m_report);
    split->addWidget(leftW);
    split->addWidget(rightW);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    QSettings st;
    if (!restoreGeometry(st.value(QStringLiteral("windows/fit")).toByteArray())) resize(1000, 640);
    if (!split->restoreState(st.value(QStringLiteral("windows/fitSplit")).toByteArray())) split->setSizes({560, 440});
    connect(this, &QDialog::finished, this, [this, split] {
        QSettings st;
        st.setValue(QStringLiteral("windows/fit"), saveGeometry());
        st.setValue(QStringLiteral("windows/fitSplit"), split->saveState());
    });

    connect(m_model, &QComboBox::activated, this, &FitDialog::onModelChosen);
    connect(m_formula, &QLineEdit::textChanged, this, [this] {
        syncParamCount();
        m_haveResult = false;
    });
    connect(m_formula, &QLineEdit::editingFinished, this, [this] {
        QVector<double> p;
        for (int r = 0; r < m_params->rowCount(); ++r) p << m_params->item(r, 0)->text().toDouble();
        updatePreview(p);
    });
    connect(guessB, &QPushButton::clicked, this, &FitDialog::guess);
    connect(fitB, &QPushButton::clicked, this, &FitDialog::runFit);
    connect(applyB, &QPushButton::clicked, this, &FitDialog::applyResult);
    connect(closeB, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_source, &QComboBox::activated, this, [this] { guess(); });
    m_model->setCurrentIndex(5);
    onModelChosen(5);
}

void FitDialog::onModelChosen(int i)
{
    const QString f = m_model->itemData(i).toString();
    if (!f.isEmpty()) m_formula->setText(f);
    syncParamCount();
    guess();
}

void FitDialog::syncParamCount()
{
    const int n = paramCountOf(m_formula->text());
    const int old = m_params->rowCount();
    m_params->setRowCount(n);
    QStringList names;
    for (int k = 0; k < n; ++k) names << QStringLiteral("a%1").arg(k);
    m_params->setVerticalHeaderLabels(names);
    for (int r = old; r < n; ++r) {
        m_params->setItem(r, 0, new QTableWidgetItem(QStringLiteral("1")));
        auto* fixed = new QTableWidgetItem;
        fixed->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        fixed->setCheckState(Qt::Unchecked);
        m_params->setItem(r, 1, fixed);
        m_params->setItem(r, 2, new QTableWidgetItem);
        m_params->setItem(r, 3, new QTableWidgetItem);
        for (int c = 4; c < 6; ++c) {
            auto* it = new QTableWidgetItem;
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_params->setItem(r, c, it);
        }
    }
}

void FitDialog::guess()
{
    const QPoint gs = m_source->currentData().toPoint();
    const Graph* g = m_doc->project().graph(gs.x());
    const DataSet* s = g ? g->set(gs.y()) : nullptr;
    if (!s || s->size() == 0) return;
    const auto sx = transforms::statistics(s->cols[0]), sy = transforms::statistics(s->cols[1]);
    const double xr = std::max(1e-12, sx.max - sx.min);
    const QString model = QString::fromLatin1(kModels[std::clamp(m_model->currentIndex(), 0, int(std::size(kModels)) - 1)].name);
    QVector<double> p(m_params->rowCount(), 1.0);
    auto set = [&](int k, double v) { if (k < p.size() && std::isfinite(v)) p[k] = v; };
    const double xAtMax = s->cols[0].value(sy.imax), yFirst = s->cols[1].value(0), yLast = s->cols[1].value(s->size() - 1);
    if (model.contains(QLatin1String("Gaussian")) || model.contains(QLatin1String("Lorentzian"))) {
        set(0, sy.max - sy.min); set(1, xAtMax); set(2, xr / 10); set(3, sy.min);
    } else if (model.contains(QLatin1String("Voigt"))) {
        set(0, (sy.max - sy.min) * xr / 4); set(1, xAtMax); set(2, xr / 20); set(3, xr / 20); set(4, sy.min);
    } else if (model.contains(QLatin1String("decay"))) {
        set(0, yFirst - yLast); set(1, xr / 3); set(2, yLast);
    } else if (model.contains(QLatin1String("growth"))) {
        set(0, std::max(1e-12, std::fabs(yFirst))); set(1, std::log(std::max(1e-12, std::fabs(yLast / (yFirst == 0 ? 1 : yFirst)))) / xr);
    } else if (model.contains(QLatin1String("Logistic"))) {
        set(0, sy.max - sy.min); set(1, 0.5 * (sx.min + sx.max)); set(2, xr / 10 * (yLast >= yFirst ? 1 : -1)); set(3, sy.min);
    } else if (model.contains(QLatin1String("Sine"))) {
        set(0, 0.5 * (sy.max - sy.min)); set(1, 2 * M_PI / (xr / 3)); set(2, 0); set(3, sy.mean);
    } else if (model.contains(QLatin1String("Michaelis")) || model.contains(QLatin1String("Hill"))) {
        set(0, sy.max); set(1, 0.5 * (sx.min + sx.max)); set(2, 1);
    } else if (model.contains(QLatin1String("Linear")) || model.contains(QLatin1String("Quadratic")) || model.contains(QLatin1String("Cubic"))) {
        const auto r = fit::regression(s->cols[0], s->cols[1], fit::Regression::Polynomial, p.size() - 1);
        for (int k = 0; k < p.size() && r.ok; ++k) set(k, r.coefficients.value(k));
    } else if (model.contains(QLatin1String("Arrhenius"))) {
        set(0, sy.max); set(1, sx.mean);
    }
    for (int r = 0; r < m_params->rowCount(); ++r) m_params->item(r, 0)->setText(QString::number(p[r], 'g', 6));
    updatePreview(p);
}

fit::Options FitDialog::options() const
{
    fit::Options o;
    o.formula = m_formula->text();
    o.nparams = m_params->rowCount();
    o.maxIterations = m_iter->value();
    o.weighting = fit::Weighting(m_weights->currentIndex() == 3 ? int(fit::Weighting::InverseDY2) : m_weights->currentIndex());
    for (int r = 0; r < m_params->rowCount() && r < fit::kMaxParams; ++r) {
        fit::Param& pp = o.params[r];
        pp.value = m_params->item(r, 0)->text().toDouble();
        pp.fixed = m_params->item(r, 1)->checkState() == Qt::Checked;
        bool okLo = false, okHi = false;
        const double lo = m_params->item(r, 2)->text().toDouble(&okLo);
        const double hi = m_params->item(r, 3)->text().toDouble(&okHi);
        if (okLo || okHi) {
            pp.constrained = true;
            pp.lo = okLo ? lo : -HUGE_VAL;
            pp.hi = okHi ? hi : HUGE_VAL;
        }
    }
    return o;
}

void FitDialog::runFit()
{
    const QPoint gs = m_source->currentData().toPoint();
    const Graph* g = m_doc->project().graph(gs.x());
    const DataSet* s = g ? g->set(gs.y()) : nullptr;
    if (!s) return;
    const fit::Options o = options();
    QVector<double> dy;
    if (s->type == SetType::XYDY) dy = s->cols[2];
    m_result = fit::nonlinear(s->cols[0], s->cols[1], dy, o);
    if (!m_result.ok) {
        m_report->setPlainText(tr("Fit failed: %1").arg(m_result.message));
        m_haveResult = false;
        return;
    }
    m_haveResult = true;
    for (int r = 0; r < m_params->rowCount(); ++r) {
        m_params->item(r, 4)->setText(QString::number(m_result.values.value(r), 'g', 10));
        m_params->item(r, 5)->setText(QString::number(m_result.errors.value(r), 'g', 4));
    }
    m_report->setPlainText(m_result.report(QStringLiteral("y = ") + o.formula));
    updatePreview(m_result.values);
}

void FitDialog::updatePreview(const QVector<double>& params)
{
    const QPoint gs = m_source->currentData().toPoint();
    const Graph* g = m_doc->project().graph(gs.x());
    const DataSet* s = g ? g->set(gs.y()) : nullptr;
    if (!s) return;
    Project p = presets::newProject(presets::defaultName());
    Graph& pg = p.graphs[0];
    pg.xscale = g->xscale;
    pg.yscale = g->yscale;
    DataSet& d = pg.ensureSet(0);
    presets::styleSet(d, 0, p.stylePreset);
    d.cols[0] = s->cols[0];
    d.cols[1] = s->cols[1];
    d.lineType = LineType::None;
    d.symbol.type = Symbol::Circle;
    d.symbol.size = 0.7;
    d.legend = tr("data");
    const auto sx = transforms::statistics(s->cols[0]);
    const QVector<double> xs = transforms::linspace(sx.min, sx.max, 400);
    QString err;
    const QVector<double> ys = fit::evaluate(m_formula->text(), xs, params, &err);
    DataSet& f = pg.ensureSet(1);
    presets::styleSet(f, 1, p.stylePreset);
    f.cols[0] = xs;
    f.cols[1] = ys;
    f.legend = m_haveResult ? tr("fit") : tr("initial guess");
    pg.autoscale(3, 0);
    m_previewParams = params;
    // the preview page has the shape of the pane, and the graph fills the page with room for the labels
    const double aspect = std::clamp(double(m_preview->height()) / std::max(1, m_preview->width()), 0.4, 1.5);
    p.page.width = 640;
    p.page.height = 640 * aspect;
    const double w = p.page.viewWidth(), h = p.page.viewHeight();  // the short side is 1
    pg.vxmin = 0.13 * w + 0.05; pg.vxmax = w - 0.04;
    pg.vymin = 0.10 * h + 0.06; pg.vymax = h - 0.04;
    const QImage img = renderToImage(p, int(m_preview->width() * devicePixelRatioF()));
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(devicePixelRatioF());
    m_preview->setPixmap(pm);
    if (!err.isEmpty()) m_report->setPlainText(tr("Formula error: %1").arg(err));
}

bool FitDialog::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_preview && e->type() == QEvent::Resize && m_previewTimer) m_previewTimer->start();
    return QDialog::eventFilter(o, e);
}

void FitDialog::applyResult()
{
    if (!m_haveResult) runFit();
    if (!m_haveResult) return;
    const QPoint gs = m_source->currentData().toPoint();
    const Graph* g = m_doc->project().graph(gs.x());
    const DataSet* s = g ? g->set(gs.y()) : nullptr;
    if (!s) return;
    QVector<double> xs;
    const int mode = m_output->currentIndex();
    if (mode == 0) {
        const auto sx = transforms::statistics(s->cols[0]);
        xs = transforms::linspace(sx.min, sx.max, std::max(400, s->size()));
    } else {
        xs = s->cols[0];
    }
    QVector<double> ys = fit::evaluate(m_formula->text(), xs, m_result.values);
    if (mode == 2)
        for (int i = 0; i < ys.size(); ++i) ys[i] = s->cols[1].value(i) - ys[i];
    QString formula = m_formula->text();
    for (int k = m_result.values.size() - 1; k >= 0; --k)
        formula.replace(QRegularExpression(QStringLiteral("\\ba%1\\b").arg(k)), QString::number(m_result.values[k], 'g', 5));
    const QString legend = mode == 2 ? tr("Residuals") : tr("Fit: y = %1").arg(formula);
    const QString report = m_result.report(m_formula->text());
    m_doc->modify(tr("Curve fit"), [&](Project& p) {
        const int si = addResultSet(p, gs.x(), xs, ys, legend, report.left(400));
        if (si >= 0 && mode != 2) {
            DataSet& d = p.graph(gs.x())->sets[si];
            d.symbol.type = Symbol::None;
            d.lineType = LineType::Straight;
        }
    });
}

// ====================================================================== import

ImportDialog::ImportDialog(Document* doc, const QString& path, const QString& text, QWidget* parent)
    : QDialog(parent), m_doc(doc), m_path(path), m_text(text)
{
    setWindowTitle(path.isEmpty() ? tr("Import data from the clipboard") : tr("Import %1").arg(QFileInfo(path).fileName()));
    setWindowIcon(icons::get(QStringLiteral("import")));
    resize(760, 540);
    auto* lay = new QVBoxLayout(this);
    m_previewText = new QPlainTextEdit(this);
    m_previewText->setReadOnly(true);
    m_previewText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_previewText->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_previewText->setPlainText(text.left(6000));
    lay->addWidget(m_previewText, 1);
    auto* form = new QFormLayout;
    m_mode = new QComboBox(this);
    m_mode->addItems({tr("One set per data block (columns follow the set type)"), tr("X, then one set per Y column (NXY)"),
                      tr("Choose columns (block data)")});
    form->addRow(tr("Layout"), m_mode);
    m_type = new QComboBox(this);
    for (int i = 0; i < int(SetType::Count); ++i) m_type->addItem(setTypeName(SetType(i)).toUpper() + QStringLiteral("   (") + setTypeColumnNames(SetType(i)).join(QStringLiteral(", ")) + QLatin1Char(')'));
    form->addRow(tr("Set type"), m_type);
    m_columns = new QLineEdit(QStringLiteral("0:1"), this);
    m_columns->setToolTip(tr("Zero-based columns separated by ':' in the order of the set type, e.g. 0:2:3 for X, Y, dY"));
    form->addRow(tr("Columns"), m_columns);
    m_target = new QComboBox(this);
    m_target->addItem(tr("Current graph"), -1);
    m_target->addItem(tr("New graph"), -2);
    for (int g = 0; g < doc->project().graphs.size(); ++g)
        if (doc->project().graphs[g].active) m_target->addItem(QStringLiteral("G%1").arg(g), g);
    form->addRow(tr("Into"), m_target);
    m_autoscale = new QCheckBox(tr("Autoscale the graph"), this);
    m_autoscale->setChecked(true);
    form->addRow(QString(), m_autoscale);
    m_legends = new QCheckBox(tr("Use column headers / file name as legends"), this);
    m_legends->setChecked(true);
    form->addRow(QString(), m_legends);
    lay->addLayout(form);
    m_summary = new QLabel(this);
    lay->addWidget(m_summary);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ImportDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_mode, &QComboBox::currentIndexChanged, this, &ImportDialog::reparse);
    // a set type declared in the file ("@TYPE xydy" in GROMACS .xvg files) is the default
    QStringList commands;
    const auto tables = datareader::parseText(m_text, &commands, doc->project().referenceDate);
    static const QRegularExpression typeRe(QStringLiteral("^\\s*type\\s+(\\w+)"), QRegularExpression::CaseInsensitiveOption);
    for (const QString& c : commands) {
        SetType t;
        const auto m = typeRe.match(c);
        if (m.hasMatch() && parseSetType(m.captured(1).toLower(), &t)) m_fileType = int(t);
    }
    if (m_fileType >= 0) {
        const int cols = tables.isEmpty() ? 0 : tables[0].columns();
        if (SetType(m_fileType) == SetType::XY && cols > 2) m_mode->setCurrentIndex(1);
        m_type->setCurrentIndex(m_fileType);
    }
    reparse();
}

void ImportDialog::reparse()
{
    const auto tables = datareader::parseText(m_text, nullptr, m_doc->project().referenceDate);
    int rows = 0, cols = 0;
    for (const auto& t : tables) {
        rows += t.rows();
        cols = std::max(cols, t.columns());
    }
    QString headers;
    if (!tables.isEmpty() && !tables[0].headers.isEmpty()) headers = tr("  Columns: %1").arg(tables[0].headers.join(QStringLiteral(", ")));
    m_summary->setText(tr("%n block(s), ", nullptr, tables.size()) + tr("%1 rows, %2 columns.").arg(rows).arg(cols) + headers);
    m_columns->setEnabled(m_mode->currentIndex() == 2);
    m_type->setEnabled(m_mode->currentIndex() != 1);
    if (m_fileType < 0 && m_mode->currentIndex() == 0 && m_type->currentIndex() == 0 && cols == 3) {
        // three columns: a common case is X Y dY
        m_type->setCurrentIndex(int(SetType::XYDY));
    }
}

void ImportDialog::accept()
{
    const int mode = m_mode->currentIndex();
    const SetType type = SetType(m_type->currentIndex());
    const QString cols = m_columns->text();
    const int target = m_target->currentData().toInt();
    const bool autoscale = m_autoscale->isChecked();
    const bool legends = m_legends->isChecked();
    const QString name = m_path.isEmpty() ? tr("clipboard") : QFileInfo(m_path).fileName();
    QStringList errs;
    int graph = 0;
    m_doc->modify(tr("Import data"), [&](Project& p) {
        int gi = target;
        if (target == -1) gi = std::max(0, m_doc->currentGraph());
        if (target == -2) {
            gi = p.graphs.size();
            Graph g = p.graph(m_doc->currentGraph()) ? *p.graph(m_doc->currentGraph()) : Graph();
            g.sets.clear();
            g.title.text.clear();
            g.active = true;
            p.graphs.push_back(g);
            arrangeGraphs(p, p.activeGraphCount(), 1, 0.12, 0.2, 0.25);
        }
        graph = gi;
        if (!p.graph(gi)) {
            Graph& g = p.ensureGraph(gi);
            presets::styleGraph(g, p.stylePreset);
        }
        const int before = p.graph(gi)->sets.size();
        Interpreter in(&p);
        in.autoscaleOnRead = 0;
        in.setCurrentGraph(gi);
        // the file's own type (and its several column groups) only applies if it was kept
        in.fileTypeOverrides = mode == 0 && int(type) == m_fileType;
        if (mode == 2) {
            in.readDataText(m_text, Interpreter::ReadMode::Block, type, gi, name);
            in.formBlockSet(type, cols, gi);
        } else {
            in.readDataText(m_text, mode == 1 ? Interpreter::ReadMode::NXY : Interpreter::ReadMode::Single, type, gi, name);
        }
        errs = in.errors();
        Graph* g = p.graph(gi);
        if (legends) {
            int k = 0;
            for (int s = before; s < g->sets.size(); ++s) {
                DataSet& d = g->sets[s];
                if (!d.active || !d.legend.isEmpty()) continue;
                d.legend = QFileInfo(name).completeBaseName() + (k++ ? QStringLiteral(" %1").arg(k) : QString());
            }
        }
        if (autoscale) g->autoscale();
    });
    m_doc->setCurrentGraph(graph);
    if (!errs.isEmpty()) QMessageBox::warning(this, windowTitle(), errs.join(QLatin1Char('\n')));
    QDialog::accept();
}

// ====================================================================== export

ExportDialog::ExportDialog(Document* doc, QWidget* parent) : QDialog(parent), m_doc(doc)
{
    setWindowTitle(tr("Export plot"));
    setWindowIcon(icons::get(QStringLiteral("export")));
    QSettings st;
    auto* lay = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_format = new QComboBox(this);
    const QStringList fmts = exporter::formats();
    static const QHash<QString, QString> desc = {
        {QStringLiteral("pdf"), tr("PDF — vector, for papers and LaTeX")}, {QStringLiteral("svg"), tr("SVG — vector, for the web and editing")},
        {QStringLiteral("eps"), tr("EPS — vector, for LaTeX/dvips")},     {QStringLiteral("ps"), tr("PostScript")},
        {QStringLiteral("png"), tr("PNG — lossless raster")},              {QStringLiteral("jpeg"), tr("JPEG — photo raster")},
        {QStringLiteral("tiff"), tr("TIFF raster")},                             {QStringLiteral("bmp"), tr("BMP raster")},
        {QStringLiteral("ppm"), tr("PPM/PNM raster")},                           {QStringLiteral("webp"), tr("WebP raster")}};
    for (const QString& f : fmts) m_format->addItem(desc.value(f, f.toUpper()), f);
    m_format->setCurrentIndex(std::max(0, m_format->findData(st.value(QStringLiteral("export/format"), QStringLiteral("pdf")).toString())));
    form->addRow(tr("Format"), m_format);
    auto* pathRow = new QHBoxLayout;
    m_path = new QLineEdit(this);
    QString base = doc->filePath().isEmpty() ? QStringLiteral("plot") : QFileInfo(doc->filePath()).completeBaseName();
    QString dir = st.value(QStringLiteral("export/dir"), QDir::homePath()).toString();
    if (!doc->filePath().isEmpty()) dir = QFileInfo(doc->filePath()).absolutePath();
    m_path->setText(QDir(dir).filePath(base + QLatin1Char('.') + exporter::extensionFor(m_format->currentData().toString())));
    auto* browse = new QPushButton(tr("Browse..."), this);
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browse);
    form->addRow(tr("File"), pathRow);
    m_dpi = new QDoubleSpinBox(this);
    m_dpi->setRange(10, 4800);
    m_dpi->setDecimals(0);
    m_dpi->setSuffix(tr(" dpi"));
    m_dpi->setValue(st.value(QStringLiteral("export/dpi"), 300).toDouble());
    form->addRow(tr("Resolution"), m_dpi);
    m_transparent = new QCheckBox(tr("Transparent background"), this);
    m_transparent->setChecked(st.value(QStringLiteral("export/transparent"), false).toBool());
    form->addRow(QString(), m_transparent);
    m_gray = new QCheckBox(tr("Greyscale"), this);
    form->addRow(QString(), m_gray);
    m_tight = new QCheckBox(tr("Tight bounding box (EPS)"), this);
    m_tight->setChecked(st.value(QStringLiteral("export/tight"), true).toBool());
    form->addRow(QString(), m_tight);
    lay->addLayout(form);
    m_info = new QLabel(this);
    lay->addWidget(m_info);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ExportDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_format, &QComboBox::currentIndexChanged, this, [this] {
        QString p = m_path->text();
        const int dot = p.lastIndexOf(QLatin1Char('.'));
        if (dot > p.lastIndexOf(QLatin1Char('/'))) p = p.left(dot);
        m_path->setText(p + QLatin1Char('.') + exporter::extensionFor(m_format->currentData().toString()));
        updateInfo();
    });
    connect(m_dpi, &QDoubleSpinBox::valueChanged, this, &ExportDialog::updateInfo);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, tr("Export to"), m_path->text(), exporter::fileFilter());
        if (f.isEmpty()) return;
        m_path->setText(f);
        const QString fmt = exporter::normalizeFormat(QFileInfo(f).suffix());
        const int i = m_format->findData(fmt);
        if (i >= 0) m_format->setCurrentIndex(i);
    });
    updateInfo();
    resize(560, sizeHint().height());
}

void ExportDialog::updateInfo()
{
    const QString f = m_format->currentData().toString();
    const bool vec = exporter::isVector(f);
    m_dpi->setEnabled(!vec);
    m_tight->setEnabled(f == QLatin1String("eps"));
    const Page& pg = m_doc->project().page;
    if (vec)
        m_info->setText(tr("Page %1 × %2 pt (%3 × %4 mm). Vector output, fonts embedded.")
                            .arg(pg.width).arg(pg.height).arg(pg.width * 25.4 / 72, 0, 'f', 1).arg(pg.height * 25.4 / 72, 0, 'f', 1));
    else
        m_info->setText(tr("Image size: %1 × %2 pixels.").arg(qRound(pg.width * m_dpi->value() / 72)).arg(qRound(pg.height * m_dpi->value() / 72)));
}

void ExportDialog::accept()
{
    exporter::Options o;
    o.format = m_format->currentData().toString();
    o.dpi = m_dpi->value();
    o.transparent = m_transparent->isChecked();
    o.grayscale = m_gray->isChecked();
    o.tightBBox = m_tight->isChecked();
    QString err;
    if (!exporter::exportToFile(m_doc->project(), m_path->text(), o, &err)) {
        QMessageBox::critical(this, tr("Export failed"), err);
        return;
    }
    QSettings st;
    st.setValue(QStringLiteral("export/format"), o.format);
    st.setValue(QStringLiteral("export/dpi"), o.dpi);
    st.setValue(QStringLiteral("export/transparent"), o.transparent);
    st.setValue(QStringLiteral("export/tight"), o.tightBBox);
    st.setValue(QStringLiteral("export/dir"), QFileInfo(m_path->text()).absolutePath());
    st.setValue(QStringLiteral("export/last"), m_path->text());
    QDialog::accept();
}

bool ExportDialog::exportWithLastSettings(Document* doc, QWidget* parent)
{
    QSettings st;
    const QString last = st.value(QStringLiteral("export/last")).toString();
    if (last.isEmpty()) return false;
    exporter::Options o;
    o.format = st.value(QStringLiteral("export/format"), QStringLiteral("pdf")).toString();
    o.dpi = st.value(QStringLiteral("export/dpi"), 300).toDouble();
    o.transparent = st.value(QStringLiteral("export/transparent"), false).toBool();
    o.tightBBox = st.value(QStringLiteral("export/tight"), true).toBool();
    QString err;
    if (!exporter::exportToFile(doc->project(), last, o, &err)) {
        QMessageBox::critical(parent, tr("Export failed"), err);
        return false;
    }
    return true;
}

// ====================================================================== arrange

ArrangeDialog::ArrangeDialog(Document* doc, QWidget* parent) : QDialog(parent), m_doc(doc)
{
    setWindowTitle(tr("Arrange graphs"));
    setWindowIcon(icons::get(QStringLiteral("grid")));
    auto* form = new QFormLayout(this);
    const int n = std::max(1, doc->project().activeGraphCount());
    m_rows = ispin(1, 20, std::max(1, int(std::ceil(std::sqrt(double(n))))));
    m_cols = ispin(1, 20, std::max(1, int(std::ceil(double(n) / m_rows->value()))));
    m_offset = dspin(0, 0.5, 0.12, 3, 0.01);
    m_hgap = dspin(0, 2, 0.25, 3, 0.05);
    m_vgap = dspin(0, 2, 0.3, 3, 0.05);
    m_colMajor = new QCheckBox(tr("Fill column by column"), this);
    m_snake = new QCheckBox(tr("Snake order"), this);
    form->addRow(tr("Rows"), m_rows);
    form->addRow(tr("Columns"), m_cols);
    form->addRow(tr("Page margin"), m_offset);
    form->addRow(tr("Horizontal gap"), m_hgap);
    form->addRow(tr("Vertical gap"), m_vgap);
    form->addRow(QString(), m_colMajor);
    form->addRow(QString(), m_snake);
    form->addRow(new QLabel(tr("Gaps are relative to the graph size. Graphs are added when needed.")));
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ArrangeDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ArrangeDialog::accept()
{
    const int r = m_rows->value(), c = m_cols->value();
    const double off = m_offset->value(), hg = m_hgap->value(), vg = m_vgap->value();
    const bool cm = m_colMajor->isChecked(), sn = m_snake->isChecked();
    m_doc->modify(tr("Arrange graphs"), [=](Project& p) { arrangeGraphs(p, r, c, off, hg, vg, cm, false, false, sn); });
    QDialog::accept();
}

// ====================================================================== preferences

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setWindowIcon(icons::get(QStringLiteral("settings")));
    QSettings st;
    auto* form = new QFormLayout(this);
    m_theme = new QComboBox(this);
    m_theme->addItems({tr("Follow the system"), tr("Light"), tr("Dark")});
    m_theme->setCurrentIndex(st.value(QStringLiteral("ui/theme"), 0).toInt());
    form->addRow(tr("Theme"), m_theme);
    m_native = new QCheckBox(tr("Use the platform's native widget style"), this);
    m_native->setChecked(st.value(QStringLiteral("ui/native"), false).toBool());
    form->addRow(QString(), m_native);
    m_preset = new QComboBox(this);
    m_preset->addItems(presets::names());
    m_preset->setCurrentText(st.value(QStringLiteral("new/preset"), presets::defaultName()).toString());
    form->addRow(tr("Style of new plots"), m_preset);
    m_autosave = ispin(0, 120, st.value(QStringLiteral("autosave/minutes"), 3).toInt());
    m_autosave->setSuffix(tr(" min (0 = off)"));
    form->addRow(tr("Autosave every"), m_autosave);
    m_recent = ispin(0, 50, st.value(QStringLiteral("recent/max"), 12).toInt());
    form->addRow(tr("Recent files"), m_recent);
    m_safe = new QCheckBox(tr("Safe mode for scripts in project files (no file writes)"), this);
    m_safe->setChecked(st.value(QStringLiteral("scripts/safe"), true).toBool());
    form->addRow(QString(), m_safe);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PreferencesDialog::accept()
{
    QSettings st;
    st.setValue(QStringLiteral("ui/theme"), m_theme->currentIndex());
    st.setValue(QStringLiteral("ui/native"), m_native->isChecked());
    st.setValue(QStringLiteral("new/preset"), m_preset->currentText());
    st.setValue(QStringLiteral("autosave/minutes"), m_autosave->value());
    st.setValue(QStringLiteral("recent/max"), m_recent->value());
    st.setValue(QStringLiteral("scripts/safe"), m_safe->isChecked());
    emit themeChanged();
    QDialog::accept();
}

}  // namespace cactus::ui
