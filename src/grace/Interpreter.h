// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - interpreter for the Grace command/parameter language.
//
// One parser handles: .agr project files (and xmgr/ACE ascii projects),
// parameter files (-param), batch files (-batch), command-line -pexec
// strings and the interactive console. Data blocks (lines of numbers,
// terminated by '&') are read into the current target set.
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

#include "core/Model.h"
#include "math/Expr.h"
#include "math/Fit.h"

namespace cactus {

struct HardcopySettings {
    QString device = QStringLiteral("PNG");
    QString printFile;
    double dpi = 300.0;
    QString options;
    bool grayscale = false;
    bool transparent = false;
};

class Interpreter {
public:
    explicit Interpreter(Project* project);

    void setProject(Project* p) { m_p = p; }
    Project* project() const { return m_p; }

    /// Run a script/project text. Returns false if errors occurred (see errors()).
    bool run(const QString& text, const QString& sourceName = QString());
    /// Run a file (any of .agr, .par, batch). Relative paths use workingDir.
    bool runFile(const QString& path);
    /// Execute one line of commands (statements separated by ';').
    bool execute(const QString& line);

    enum class ReadMode { Single, NXY, Block };
    /// Read a data file into the given graph (-1: current graph).
    bool readData(const QString& path, ReadMode mode, SetType type, int graph = -1);
    bool readDataText(const QString& text, ReadMode mode, SetType type, int graph, const QString& sourceName);
    /// Form a set from the last block data, columns like "0:2" or "1:3:4" (Grace numbering, 0-based).
    bool formBlockSet(SetType type, const QString& colspec, int graph = -1);
    bool hasBlockData() const { return !m_block.isEmpty(); }

    int currentGraph() const { return m_curGraph; }
    /// The current graph, created in the project's style (like graph 0) if it does not exist yet.
    Graph& ensureCurrentGraph() { return graph(); }
    void setCurrentGraph(int g) { m_curGraph = g; }
    int lastCreatedSet() const { return m_lastSet; }

    HardcopySettings& hardcopy() { return m_hardcopy; }
    expr::Context& variables() { return m_vars; }
    fit::Options& fitOptions() { return m_fit; }

    bool safeMode = true;
    QString workingDir;
    int autoscaleOnRead = 3;  ///< 0 none, 1 x, 2 y, 3 xy
    /// A "@TYPE xydy" line inside a data file (as in GROMACS .xvg output) overrides the
    /// set type passed to readDataText(), and extra columns become extra sets of that type.
    bool fileTypeOverrides = true;
    double referenceDateHint = 0.0;

    QStringList errors() const { return m_errors; }
    QStringList warnings() const { return m_warnings; }
    void clearMessages() { m_errors.clear(); m_warnings.clear(); }

    // Hooks (all optional)
    std::function<void(const QString&)> messageHandler;              ///< results, WRITE output, info
    std::function<bool(const HardcopySettings&)> printHandler;       ///< PRINT
    std::function<void()> redrawHandler;                             ///< REDRAW / UPDATEALL
    std::function<void(int)> exitHandler;                            ///< EXIT
    std::function<bool(const QString&)> loadHandler;                 ///< LOAD "file"

    /// Evaluate an expression in the context of the current graph/set.
    bool evaluate(const QString& text, expr::Value* out, QString* error = nullptr, int graph = -1, int set = -1);
    /// Assign the result of an expression to a set column, e.g. ("s0.y", "s0.y*2").
    bool assign(const QString& lhs, const QString& rhs);

    /// Grace line-type numbering <-> model.
    static LineType lineTypeFromGrace(int v);
    static int lineTypeToGrace(LineType t);

    struct Tok;
    class Cursor;

private:

    bool statement(const QString& stmt);
    bool dispatch(Cursor& c);
    void error(const QString& msg);
    void warn(const QString& msg);
    void info(const QString& msg);

    // property parsers
    bool graphProps(Cursor& c, Graph& g);
    bool setProps(Cursor& c, DataSet& s, int setIndex);
    bool axisProps(Cursor& c, Axis& ax);
    bool tickProps(Cursor& c, TickProps& tp);
    bool legendProps(Cursor& c, Legend& L, const Graph& g);
    bool frameProps(Cursor& c, Frame& f);
    bool objectProps(Cursor& c, DrawObject& o, const QString& kind);
    bool regionProps(Cursor& c, Region& r);
    bool pageProps(Cursor& c);
    bool deviceCommand(Cursor& c);
    bool fitCommand(Cursor& c);
    bool functionCommand(const QString& name, const QStringList& args);
    bool setOperation(Cursor& c, const QString& op);

    // helpers
    bool parseColor(Cursor& c, QColor* out);
    bool parseFont(Cursor& c, QString* out);
    bool parseOnOff(Cursor& c, bool* out);
    double number(Cursor& c, bool* ok = nullptr);
    QString string(Cursor& c, bool* ok = nullptr);
    bool resolveSet(const QString& ref, int* graph, int* set, bool create = false);
    bool resolveGraph(const QString& ref, int* graph);
    Graph& graph();
    DataSet* setRef(const QString& ref, int* gi = nullptr, int* si = nullptr);
    int newSet(int graph, const QVector<double>& x, const QVector<double>& y, const QString& comment);
    void finishBlock();
    void addDataRow(const QString& line);
    bool resolveIdent(const QString& name, expr::Value* out, int graph, int set) const;
    void applyAutoscaleOnRead(int graph);
    bool runNonlinearFit(int graph, int set, int iterations);

    Project* m_p;
    int m_curGraph = 0;
    int m_lastSet = -1;
    QString m_source;

    // data block state
    bool m_haveTarget = false;
    int m_targetGraph = 0, m_targetSet = 0;
    SetType m_blockType = SetType::XY;
    QVector<QVector<double>> m_rows;
    QStringList m_rowStrings;
    bool m_implicitType = true;
    SetType m_nextSetType = SetType::XY;
    bool m_typeSeen = false;  ///< a TYPE statement without a target was executed

    // pending drawing object ("with string" ... "string def")
    bool m_haveObject = false;
    QString m_objectKind;
    DrawObject m_object;

    // block data (READ BLOCK)
    QVector<QVector<double>> m_block;
    QStringList m_blockStrings;

    HardcopySettings m_hardcopy;
    expr::Context m_vars;
    fit::Options m_fit;
    QStringList m_errors, m_warnings;
    int m_depth = 0;
    int m_version = 50125;  ///< project file version (affects legacy numbering)
};

}  // namespace cactus
