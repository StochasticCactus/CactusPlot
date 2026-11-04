# Tutorial: your first plot

This walk-through takes about five minutes. You will load measurements with error bars, label the
axes, fit a curve, and export a PDF for a paper.

## 1. Get some data in

Select the table below and copy it (**Ctrl+C**, or **⌘C** on a Mac). It is a decay measured at nine
times, with the uncertainty of each point in the third column.

```
# time   counts   error
0        98.1     4.2
1        61.0     3.5
2        37.9     2.9
3        23.6     2.4
4        14.0     1.9
5         9.1     1.6
6         5.3     1.3
7         3.6     1.1
8         2.0     0.9
```

Start CactusPlot and choose **Edit ▸ Import from Clipboard** (**Ctrl+Shift+V**). The import dialog
shows a preview. Because the third column is called "error", CactusPlot suggests the set type
**XYDY** (X, Y and the error in Y). Without such a name, extra columns would become extra curves.
Click **Import**.

The points appear with error bars, and the axes are scaled to the data. The legend says "counts":
the names in the `#` header line are used for legends.

> **Tip.** You can also drag a data file onto the window, or use **File ▸ Open** (**Ctrl+O**). Files
> from GROMACS (`.xvg`) open directly, with their labels and error bars; see
> [GROMACS files](guide/gromacs.md).

## 2. Label the plot

**Double-click the x axis** (the numbers along the bottom). The **Appearance** window opens on that
axis. In the **Label** box type:

```
Time $t$ (h)
```

The text between dollar signs is typeset as math, so the *t* is italic. Now click the y axis in the
plot: the Appearance window follows the selection. Give it the label `Counts $N$`.

To add a title, double-click the empty area inside the graph. The Appearance window shows the
graph's settings; type a title in **Titles ▸ Title**.

Every change shows immediately and can be undone with **Ctrl+Z**.

## 3. Fit a curve

Choose **Data ▸ Curve Fitting** (**Ctrl+F**).

1. **Data set** is the set you imported.
2. Pick **Exponential decay** from **Model**. The formula `a0*exp(-x/a1) + a2` appears in the
   **y =** box, and starting values are guessed from the data (**Guess initial values** does this
   again at any time).
3. Set **Weights** to **1/dy² (XYDY sets)**, so points with smaller error bars count more.
4. Click **Fit**. The preview on the right of the dialog draws the fitted curve over the data. The
   table shows each parameter with its standard error, and the report gives χ², reduced χ² and R².
5. Click **Create result set**. The fitted curve is added to the graph as a new set.

You can type any formula into the **y =** box, using `x` and the parameters `a0` … `a9`. Tick
**Fixed** to hold a parameter constant, or fill in **Min** and **Max** to bound it.

## 4. Make it look the way you want

The plot uses the **Modern** style, which is the default. To try another look, open
**Plot ▸ Style Preset** and choose a style:

- **Publication**: serif fonts, ticks on all sides, no grid;
- **Presentation**: big text and thick lines for slides;
- **Grace Classic**: exactly like xmgrace.

**Plot ▸ Colour Palette** recolours all sets at once. To change one set, double-click its curve;
the **Set** page of the Appearance window has its colour, symbol, line width and error bar settings.

## 5. Export

Choose **File ▸ Export** (**Ctrl+E**), pick **PDF** and a file name, and click **Export**. PDF and
SVG keep text as text and lines as vectors, so they stay sharp at any size. For LaTeX you can also
use EPS with a tight bounding box.

While you are adjusting a figure for a paper, **File ▸ Export Again** (**Ctrl+Shift+E**) rewrites the
same file with the same settings, without asking.

## 6. Save

**File ▸ Save** (**Ctrl+S**) writes a `.cplot` project, which holds the data and every setting.
**Save As** can also write a Grace `.agr` project, which xmgrace can open.

## Where next

- [The main window](guide/window.md) explains every button and menu.
- [Data analysis](guide/analysis.md) covers histograms, FFT, smoothing, interpolation and more.
- [Command language](reference/command-language.md) shows how to script all of this, for example
  to make the same figure for fifty data files.
