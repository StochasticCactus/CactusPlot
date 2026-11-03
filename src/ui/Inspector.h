// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - context sensitive property inspector (live editing, undoable).
#pragma once

#include <QWidget>

class QStackedWidget;
class QLabel;
class QComboBox;
class QTabWidget;
class QPushButton;

namespace cactus::ui {

class Document;
class Form;

class Inspector : public QWidget {
    Q_OBJECT
public:
    explicit Inspector(Document* doc, QWidget* parent = nullptr);
    void refresh();

signals:
    void editDataRequested(int graph, int set);
    void applyPresetRequested(const QString& name);

private:
    void onSelection();
    QWidget* buildPage();
    QWidget* buildGraph();
    QWidget* buildAxis();
    QWidget* buildSet();
    QWidget* buildText();
    QWidget* buildLine();
    QWidget* buildShape();

    Document* m_doc;
    QLabel* m_title;
    QStackedWidget* m_stack;
    QComboBox* m_axisChooser = nullptr;
    QList<Form*> m_forms;
    QWidget* m_pageW = nullptr;
    QWidget* m_graphW = nullptr;
    QWidget* m_axisW = nullptr;
    QWidget* m_setW = nullptr;
    QWidget* m_textW = nullptr;
    QWidget* m_lineW = nullptr;
    QWidget* m_shapeW = nullptr;
    QWidget* m_emptyW = nullptr;
    QTabWidget* m_graphTabs = nullptr;
    QPushButton* m_editData = nullptr;
    int m_g = 0, m_s = 0, m_a = 0, m_o = 0;
};

}  // namespace cactus::ui
