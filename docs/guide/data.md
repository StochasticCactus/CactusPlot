# Getting data in

## Ways to load data

| How | What happens |
|---|---|
| **Drag files onto the window** | Projects open. Data files load directly, as in xmgrace (see [How columns are used](#how-columns-are-used)). |
| **File ▸ Open** (Ctrl+O) | Same as dragging. |
| **File ▸ Import Data** (Ctrl+I) | Always shows the import dialog, so you choose the layout. |
| **Edit ▸ Import from Clipboard** (Ctrl+Shift+V) | Imports a table copied from a spreadsheet, a text editor or a web page. |
| **Command line** | `cactusplot data.dat`, `cactusplot -nxy table.dat`, `cactusplot -settype xydy err.dat`; see [Command line](../reference/command-line.md). |
| **Commands window or scripts** | `read "data.dat"`, `read xydy "err.dat"`, `read nxy "table.dat"`, or create data with expressions: `s0.x = mesh(0, 10, 200); s0.y = sin(s0.x)`. |
| **Set editor** | Type or paste numbers into a new set. |

## Data file format

CactusPlot reads plain text tables, the kind most programs can write:

```
# time   signal   error         <- optional header: names become legends
0.0      1.00     0.05
0.5      0.82     0.04
1.0      0.67     0.04
&                               <- "&" ends a block; the next block is a new set
0.0      2.00     0.10
...
```

- **Separators:** spaces, tabs, commas or semicolons.
- **Comments:** lines starting with `#`, `!` or `//` are skipped. If the last comment line before
  the data has several words (as in `# time signal error`), they are taken as column names.
- **Headers:** a non-numeric first line (`time,signal,error` from a spreadsheet) is also read as
  column names.
- **Numbers:** `1.5`, `-2e-3`, Fortran style `1.5D+02`, `nan`, `inf`.
- **Dates and times:** ISO dates (`2024-05-01`, `2024-05-01T12:30:00`), `2024/05/01`, and
  `05/01/2024` (month first). For day-first dates (`01/05/2024`), start CactusPlot with
  `-datehint european`. A date followed by a time (`2024-05-01 12:30:00`) is one value, and times
  of day (`12:30:15`) are read too. Dates become numbers (days); when the first column holds dates,
  the x axis is labelled with dates automatically. See [Axes and ticks](axes.md#dates-and-times).
- **Labels:** a quoted string at the end of a line (`3 4 "peak A"`) is kept as that point's label.
  It can be shown next to the point (annotated values) or used for pie slices.
- **Blocks:** a line with `&` separates blocks. Each block becomes its own set.
- **Grace commands:** lines starting with `@` are Grace commands (titles, labels, set types…).
  They are run when the file is read, as in xmgrace. This is how GROMACS `.xvg` files carry their
  labels; see [GROMACS files](gromacs.md).

## How columns are used

When you open a data file (double-click, drag, **File ▸ Open** or `cactusplot file`), CactusPlot
decides how to use its columns the way Grace does, plus two conveniences:

| The file has | You get |
|---|---|
| `@TYPE xydy` (GROMACS and Grace files) | error bars, from the third column |
| a header naming a column as an error: `# time counts error`, `x,y,sd`, `t signal sigma` | error bars from that column |
| two columns | one curve |
| more columns, without any of the above | one curve per column, all against the first column |
| a date and time in the first column (`2024-03-01 12:00:00`) | a time series, with dates on the x axis |

So a time series never gets error bars by accident. To choose differently, use **File ▸ Import
Data**, or `-settype xydy` / `-nxy` on the command line. Header names that count as errors include
`error`, `err`, `sd`, `std`, `stdev`, `sigma`, `σ`, `sem`, `uncertainty`, `dy`, `±`, and names
ending in `_err` or `_sd`.

## The import dialog

The dialog shows the beginning of the file and how many blocks, rows and columns were found.

| Setting | Meaning |
|---|---|
| **Layout: one set per data block** | Each block becomes one set. Its columns are used in the order of the set type: X, Y, then error columns and so on. |
| **Layout: X, then one set per Y column (NXY)** | The first column is X and every other column becomes its own set. Use this for tables such as `x y1 y2 y3`. |
| **Layout: choose columns** | Pick columns by number, starting at 0, in the order of the set type. Example: `0:2:3` makes a set from columns 0 (X), 2 (Y) and 3 (dY). |
| **Set type** | How the columns are used: XY, XYDY (error bars in y), BAR, XYZ, … See [Set types](../reference/set-types.md). The suggestion follows the file: its `@TYPE` line, or column names such as `error` (see below); otherwise XY, with several columns as several curves. |
| **Into** | The current graph, a new graph, or a graph you choose. |
| **Autoscale the graph** | Fit the axes to the data after importing. |
| **Use column headers / file name as legends** | Name new sets after their columns, or after the file. |

## Creating data from formulas

**Data ▸ Evaluate Expression / New Set…** (**Ctrl+R**) creates or changes a set from formulas:

```
x = mesh(0, 4*pi, 400)
y = sin(x) * exp(-x/5)
```

`mesh(a, b, n)` makes `n` evenly spaced numbers from `a` to `b`. Other sets are reached as `s1.y`,
or `g1.s0.x` for a set in another graph. All functions are listed in
[Functions](../reference/functions.md). The same works in the Commands window:
`s3.x = mesh(0, 1, 101); s3.y = s3.x^2`.

## Working with sets

- **Rename:** the legend text is the set's name. Change it on the **Set** page of the Appearance
  window, or with `s0 legend "Sample A"`.
- **Hide, duplicate, delete:** right-click the curve, or the set in the Explorer.
- **Edit values:** the set editor (**Ctrl+D**), or the **Edit points** tool (**P**) to drag points
  on the plot.
- **Copy and move between graphs:** in the Commands window, `copy g0.s1 to g1.s0`,
  `move g0.s1 to g1.s0`, `swap s0 and s1`. To remove a set, use `kill s3`.
- **Sort, reverse, join, split and drop points:** see [Data analysis](analysis.md#editing-sets).

## Large data

Sets with millions of points work. When a line is drawn, points that would move it by less than
0.1 pt are skipped. That cannot be seen, but it keeps big sets fast on screen and makes exported
files smaller. If the screen still gets slow, **Data ▸ Transformations ▸ Prune** or **Sample points**
make a lighter copy for exploring.
