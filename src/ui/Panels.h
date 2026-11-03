// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - side panels: project tree, command console, data sheet.
#pragma once

#include <QAbstractTableModel>
#include <QStringList>
#include <QTreeWidget>
#include <QWidget>

#include "grace/Interpreter.h"

class QLineEdit;
class QPlainTextEdit;
class QTableView;
class QComboBox;
class QLabel;

namespace cactus::ui {

class Document;

// ---------------------------------------------------------------- project tree

class ProjectTree : public QTreeWidget {
    Q_OBJECT
public:
    explicit ProjectTree(Document* doc, QWidget* parent = nullptr);
    void rebuild();

signals:
    void editDataRequested(int graph, int set);
    void propertiesRequested();

protected:
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    void syncSelection();
    Document* m_doc;
    bool m_updating = false;
};

// ---------------------------------------------------------------- console

class Console : public QWidget {
    Q_OBJECT
public:
    explicit Console(Document* doc, QWidget* parent = nullptr);
    void appendMessage(const QString& text, bool error = false);
    void runCommand(const QString& cmd);

protected:
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    Document* m_doc;
    QPlainTextEdit* m_out;
    QLineEdit* m_in;
    QStringList m_history;
    int m_histPos = 0;
    expr::Context m_vars;
    fit::Options m_fit;
};

// ---------------------------------------------------------------- data sheet

class SetTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    SetTableModel(Document* doc, QObject* parent = nullptr);
    void setTarget(int graph, int set);
    int graph() const { return m_graph; }
    int set() const { return m_set; }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    void reload();

private:
    const DataSet* ds() const;
    Document* m_doc;
    int m_graph = -1, m_set = -1;
};

class DataSheet : public QWidget {
    Q_OBJECT
public:
    explicit DataSheet(Document* doc, QWidget* parent = nullptr);
    void showSet(int graph, int set);

protected:
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void fillCombo();
    void addRows();
    void deleteRows();
    void copySelection();
    void pasteData();
    Document* m_doc;
    QComboBox* m_setCombo;
    QTableView* m_view;
    SetTableModel* m_model;
    QLabel* m_stats;
};

}  // namespace cactus::ui
