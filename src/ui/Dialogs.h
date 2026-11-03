// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - dialogs: transformations, curve fitting, import, export,
// arrange graphs and preferences.
#pragma once

#include <QDialog>
#include <QStringList>

#include "core/Model.h"
#include "math/Fit.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QStackedWidget;
class QTableWidget;
class QLabel;
class QTextEdit;

namespace cactus::ui {

class Document;

/// Fill a combo with all sets of the project ("G0.S1  legend"), data = QPoint(graph, set).
void fillSetCombo(QComboBox* combo, const Project& p, int selectGraph = -1, int selectSet = -1);

// ---------------------------------------------------------------- transformations

class TransformDialog : public QDialog {
    Q_OBJECT
public:
    explicit TransformDialog(Document* doc, QWidget* parent = nullptr);
    void showPage(const QString& name);

private:
    void apply();
    QWidget* page(const QString& name);
    QWidget* field(const QString& page, const QString& key, QWidget* w);
    double num(const QString& key) const;
    int integer(const QString& key) const;
    QString str(const QString& key) const;
    bool flag(const QString& key) const;
    int choice(const QString& key) const;

    Document* m_doc;
    QListWidget* m_list;
    QStackedWidget* m_stack;
    QComboBox* m_source;
    QComboBox* m_source2;
    QCheckBox* m_replace;
    QTextEdit* m_log;
    QHash<QString, QWidget*> m_fields;
    QHash<QString, int> m_pageIndex;
};

// ---------------------------------------------------------------- curve fitting

class FitDialog : public QDialog {
    Q_OBJECT
public:
    explicit FitDialog(Document* doc, QWidget* parent = nullptr);

private:
    void onModelChosen(int i);
    void syncParamCount();
    fit::Options options() const;
    void runFit();
    void updatePreview(const QVector<double>& params);
    void applyResult();
    void guess();

    Document* m_doc;
    QComboBox* m_source;
    QComboBox* m_model;
    QLineEdit* m_formula;
    QTableWidget* m_params;
    QComboBox* m_weights;
    QSpinBox* m_iter;
    QComboBox* m_output;
    QLabel* m_preview;
    QPlainTextEdit* m_report;
    fit::Result m_result;
    bool m_haveResult = false;
};

// ---------------------------------------------------------------- import

class ImportDialog : public QDialog {
    Q_OBJECT
public:
    ImportDialog(Document* doc, const QString& path, const QString& text, QWidget* parent = nullptr);

private:
    void reparse();
    void accept() override;

    Document* m_doc;
    QString m_path, m_text;
    QPlainTextEdit* m_previewText;
    QComboBox* m_mode;
    QComboBox* m_type;
    QLineEdit* m_columns;
    QComboBox* m_target;
    QCheckBox* m_autoscale;
    QCheckBox* m_legends;
    QLabel* m_summary;
    int m_fileType = -1;  ///< set type named by an "@TYPE" line in the file, -1 if none
};

// ---------------------------------------------------------------- export

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(Document* doc, QWidget* parent = nullptr);
    static bool exportWithLastSettings(Document* doc, QWidget* parent);

private:
    void updateInfo();
    void accept() override;

    Document* m_doc;
    QLineEdit* m_path;
    QComboBox* m_format;
    QDoubleSpinBox* m_dpi;
    QCheckBox* m_transparent;
    QCheckBox* m_gray;
    QCheckBox* m_tight;
    QLabel* m_info;
};

// ---------------------------------------------------------------- arrange

class ArrangeDialog : public QDialog {
    Q_OBJECT
public:
    ArrangeDialog(Document* doc, QWidget* parent = nullptr);

private:
    void accept() override;
    Document* m_doc;
    QSpinBox* m_rows;
    QSpinBox* m_cols;
    QDoubleSpinBox* m_offset;
    QDoubleSpinBox* m_hgap;
    QDoubleSpinBox* m_vgap;
    QCheckBox* m_colMajor;
    QCheckBox* m_snake;
};

// ---------------------------------------------------------------- preferences

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

signals:
    void themeChanged();

private:
    void accept() override;
    QComboBox* m_theme;
    QCheckBox* m_native;
    QComboBox* m_preset;
    QSpinBox* m_autosave;
    QSpinBox* m_recent;
    QCheckBox* m_safe;
};

}  // namespace cactus::ui
