// SPDX-License-Identifier: GPL-2.0-or-later
// CactusPlot - ASCII data parsing (whitespace/CSV/TSV, comments, headers,
// dates, quoted per-point strings, '&' set separators).
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace cactus {

struct DataTable {
    QVector<QVector<double>> cols;  ///< column-major
    QStringList strings;            ///< optional trailing string per row
    QStringList headers;            ///< column names if a header line was found
    int rows() const { return cols.isEmpty() ? 0 : int(cols[0].size()); }
    int columns() const { return int(cols.size()); }
    bool hasStrings() const
    {
        for (const QString& s : strings) if (!s.isEmpty()) return true;
        return false;
    }
};

namespace datareader {

enum class DateHint { ISO, European, US, Days, Seconds, None };

/// Split a data line into numeric fields and an optional quoted string.
/// Returns false if the line contains no numeric data.
bool parseLine(const QString& line, QVector<double>* values, QString* label, double referenceDate = 0.0,
               DateHint hint = DateHint::ISO);

/// Parse a date/time token into a Julian date (minus referenceDate). Returns false if not a date.
bool parseDate(const QString& token, double* value, double referenceDate = 0.0, DateHint hint = DateHint::ISO);

/// Is the line a data line (starts with a number/date) rather than a command?
bool looksLikeData(const QString& line);

/// Parse a whole text into tables separated by '&' lines. Lines starting with
/// '#' or '!' are comments; lines starting with '@' are returned in `commands`.
QVector<DataTable> parseText(const QString& text, QStringList* commands = nullptr, double referenceDate = 0.0,
                             DateHint hint = DateHint::ISO);

/// Read a file (handles UTF-8 and Latin-1, and "-" for stdin).
bool readFile(const QString& path, QString* text, QString* error);

}  // namespace datareader
}  // namespace cactus
