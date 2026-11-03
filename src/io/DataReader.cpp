// SPDX-License-Identifier: GPL-2.0-or-later
#include "io/DataReader.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QTextStream>
#include <cmath>
#include <cstdio>

#include "plot/Ticks.h"

namespace cactus::datareader {

namespace {

bool parseNumber(const QString& tok, double* v)
{
    bool ok = false;
    *v = tok.toDouble(&ok);
    if (ok) return true;
    const QString l = tok.toLower();
    if (l == QLatin1String("nan") || l == QLatin1String("-nan")) { *v = NAN; return true; }
    if (l == QLatin1String("inf") || l == QLatin1String("+inf") || l == QLatin1String("infinity")) { *v = HUGE_VAL; return true; }
    if (l == QLatin1String("-inf") || l == QLatin1String("-infinity")) { *v = -HUGE_VAL; return true; }
    // Fortran style exponents: 1.0D+03
    if (l.contains(QLatin1Char('d'))) {
        QString t = l;
        t.replace(QLatin1Char('d'), QLatin1Char('e'));
        *v = t.toDouble(&ok);
        return ok;
    }
    return false;
}

}  // namespace

bool parseDate(const QString& token, double* value, double referenceDate, DateHint hint)
{
    if (hint == DateHint::None) return false;
    static const QRegularExpression iso(QStringLiteral(
        "^(-?\\d{1,4})-(\\d{1,2})-(\\d{1,2})(?:[T ](\\d{1,2}):(\\d{1,2})(?::(\\d{1,2}(?:\\.\\d*)?))?)?Z?$"));
    static const QRegularExpression slash(QStringLiteral(
        "^(\\d{1,4})/(\\d{1,2})/(\\d{1,4})(?:[T ](\\d{1,2}):(\\d{1,2})(?::(\\d{1,2}(?:\\.\\d*)?))?)?$"));
    static const QRegularExpression timeOnly(QStringLiteral("^(\\d{1,2}):(\\d{2})(?::(\\d{2}(?:\\.\\d*)?))?$"));
    int y = 0, m = 0, d = 0, hh = 0, mm = 0;
    double ss = 0;
    auto m1 = iso.match(token);
    if (m1.hasMatch()) {
        y = m1.captured(1).toInt();
        m = m1.captured(2).toInt();
        d = m1.captured(3).toInt();
        hh = m1.captured(4).toInt();
        mm = m1.captured(5).toInt();
        ss = m1.captured(6).toDouble();
    } else {
        auto m2 = slash.match(token);
        if (m2.hasMatch()) {
            const int a = m2.captured(1).toInt(), b = m2.captured(2).toInt(), c = m2.captured(3).toInt();
            if (m2.captured(1).size() == 4) { y = a; m = b; d = c; }
            else if (hint == DateHint::European) { d = a; m = b; y = c; }
            else { m = a; d = b; y = c; }
            if (y < 100) y += (y < 50 ? 2000 : 1900);
            hh = m2.captured(4).toInt();
            mm = m2.captured(5).toInt();
            ss = m2.captured(6).toDouble();
        } else {
            auto m3 = timeOnly.match(token);
            if (!m3.hasMatch()) return false;
            *value = (m3.captured(1).toInt() * 3600.0 + m3.captured(2).toInt() * 60.0 + m3.captured(3).toDouble()) / 86400.0;
            return true;
        }
    }
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    *value = ticks::calendarToJulian(y, m, d, hh, mm, ss) - referenceDate;
    return true;
}

bool parseLine(const QString& rawLine, QVector<double>* values, QString* label, double referenceDate, DateHint hint)
{
    values->clear();
    if (label) label->clear();
    QString line = rawLine;
    // trailing quoted string
    const int q = line.indexOf(QLatin1Char('"'));
    if (q >= 0) {
        const int q2 = line.lastIndexOf(QLatin1Char('"'));
        if (label && q2 > q) *label = line.mid(q + 1, q2 - q - 1);
        line = line.left(q);
    }
    // separators: whitespace, comma, semicolon, tab
    static const QRegularExpression sep(QStringLiteral("[\\s,;]+"));
    const QStringList toks = line.split(sep, Qt::SkipEmptyParts);
    for (const QString& t : toks) {
        double v;
        if (parseNumber(t, &v) || parseDate(t, &v, referenceDate, hint)) {
            values->push_back(v);
        } else {
            if (values->isEmpty()) return false;
            break;  // stop at the first non-numeric token (trailing comments)
        }
    }
    return !values->isEmpty();
}

bool looksLikeData(const QString& line)
{
    const QString t = line.trimmed();
    if (t.isEmpty()) return false;
    const QChar c = t[0];
    if (c.isDigit() || c == QLatin1Char('-') || c == QLatin1Char('+') || c == QLatin1Char('.')) {
        QVector<double> v;
        return parseLine(t, &v, nullptr);
    }
    const QString l = t.left(4).toLower();
    return l.startsWith(QLatin1String("nan")) || l.startsWith(QLatin1String("inf"));
}

QVector<DataTable> parseText(const QString& text, QStringList* commands, double referenceDate, DateHint hint)
{
    QVector<DataTable> out;
    DataTable cur;
    bool headerCandidate = true;
    QStringList pendingHeader;
    auto finish = [&]() {
        if (cur.rows() > 0) {
            if (cur.headers.isEmpty() && !pendingHeader.isEmpty()) cur.headers = pendingHeader;
            out.push_back(cur);
        }
        cur = DataTable();
        pendingHeader.clear();
        headerCandidate = true;
    };
    const QStringList lines = text.split(QLatin1Char('\n'));
    QVector<double> vals;
    QString label;
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        const QString t = line.trimmed();
        if (t.isEmpty()) continue;
        if (t.startsWith(QLatin1Char('&'))) { finish(); continue; }
        if (t.startsWith(QLatin1Char('@'))) {
            if (commands) commands->push_back(t.mid(1));
            continue;
        }
        if (t.startsWith(QLatin1Char('#')) || t.startsWith(QLatin1Char('!')) || t.startsWith(QLatin1String("//"))) {
            // "# x y z" style header comments are a common way to name columns
            if (headerCandidate && cur.rows() == 0) {
                const QString h = t.mid(1).trimmed();
                const QStringList parts = h.split(QRegularExpression(QStringLiteral("[\\t,;]+|\\s{2,}|\\s")), Qt::SkipEmptyParts);
                if (parts.size() >= 2) pendingHeader = parts;
            }
            continue;
        }
        if (!parseLine(t, &vals, &label, referenceDate, hint)) {
            // header line (non-numeric) before any data
            if (cur.rows() == 0) {
                pendingHeader = t.split(QRegularExpression(QStringLiteral("[\\t,;]+|\\s+")), Qt::SkipEmptyParts);
                for (QString& h : pendingHeader) h = h.remove(QLatin1Char('"')).trimmed();
            }
            continue;
        }
        headerCandidate = false;
        const int r = cur.rows();
        if (cur.cols.size() < vals.size()) {
            const int old = cur.cols.size();
            cur.cols.resize(vals.size());
            for (int c = old; c < vals.size(); ++c) cur.cols[c] = QVector<double>(r, NAN);
        }
        for (int c = 0; c < cur.cols.size(); ++c) cur.cols[c].push_back(c < vals.size() ? vals[c] : NAN);
        while (cur.strings.size() < r) cur.strings.push_back(QString());
        cur.strings.push_back(label);
    }
    finish();
    return out;
}

bool readFile(const QString& path, QString* text, QString* error)
{
    QByteArray bytes;
    if (path == QLatin1String("-")) {
        QFile in;
        if (!in.open(stdin, QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("cannot read standard input");
            return false;
        }
        bytes = in.readAll();
    } else {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("cannot open %1: %2").arg(path, f.errorString());
            return false;
        }
        bytes = f.readAll();
        if (path.endsWith(QLatin1String(".gz"), Qt::CaseInsensitive)) {
            if (error) *error = QStringLiteral("compressed files are not supported: %1").arg(path);
            return false;
        }
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString s = utf8.decode(bytes);
    if (utf8.hasError()) s = QString::fromLatin1(bytes);  // Grace files are Latin-1
    if (s.startsWith(QChar(0xFEFF))) s.remove(0, 1);
    *text = s;
    return true;
}

}  // namespace cactus::datareader
