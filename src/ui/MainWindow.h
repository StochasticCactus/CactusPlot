// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - main application window.
#pragma once

#include <QMainWindow>
#include <QPointer>

#include "ui/PlotView.h"

class QLabel;
class QActionGroup;
class QMenu;
class QTimer;
class QDialog;

namespace cactus::ui {

class Document;
class Inspector;
class ProjectTree;
class Console;
class DataSheet;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    Document* document() const { return m_doc; }

    bool openFile(const QString& path);
    void setProject(const Project& p, const QString& path);
    void checkAutosaveRecovery();
    /// Open a secondary window by name (appearance, seteditor, commands, explorer, transform, fit); returns it.
    QWidget* openWindow(const QString& which);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void buildActions();
    void buildMenus();
    void buildToolbars();
    void buildDocks();
    void updateTitle();
    void updateRecentMenu();
    void addRecent(const QString& path);
    bool maybeSave();
    bool save();
    bool saveAs();
    void newProject(const QString& preset);
    void openDialog();
    void importDialog();
    void importClipboard();
    void exportDialog();
    void printPlot();
    void copyImage(bool svg);
    void addGraph(bool inset);
    void showContextMenu(const QPoint& pos);
    void applyPreset(const QString& name);
    void autosave();
    void showCommandReference();
    void showShortcuts();
    void about();
    void showWindow(QDialog* w);
    QDialog* makeToolWindow(const QString& title, QWidget* content, const QString& name, const QSize& size);
    void showAppearance(int kind = -1);
    void flash(const QString& msg, int ms = 4000);
    void applyTheme();
    QString autosavePath() const;

    Document* m_doc;
    PlotView* m_view;
    Inspector* m_inspector;
    ProjectTree* m_tree;
    Console* m_console;
    DataSheet* m_sheet;
    QDialog* m_inspectorDock;
    QDialog* m_treeDock;
    QDialog* m_consoleDock;
    QDialog* m_sheetDock;
    QLabel* m_locator;
    QLabel* m_zoomLabel;
    QTimer* m_flashTimer = nullptr;
    QMenu* m_recentMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QActionGroup* m_toolGroup = nullptr;
    QTimer* m_autosaveTimer;
    QHash<QString, QAction*> m_actions;
};

/// Entry point for the graphical application (called from main()).
}  // namespace cactus::ui
