# CactusPlot user manual

## 1. Concepts

A **project** is a **page** that holds one or more **graphs**. Each graph has a data range (its
*world*), a position on the page (its *viewport*), four axes (x, y and the alternate x/y axes), a
legend, a frame and any number of **sets**. A set is a column table whose layout is given by its
**set type**:

| Set type | Columns | Drawn as |
|---|---|---|
| XY | X, Y | line and/or symbols |
| XYDX, XYDY, XYDXDY | + symmetric errors | error bars |
| XYDXDX, XYDYDY, XYDXDXDYDY | + separate positive/negative errors | asymmetric error bars |
| BAR, BARDY, BARDYDY | X, Y (+ errors) | bars (width = symbol size) |
| XYHILO | X, High, Low, Open, Close | hi-lo-open-close |
| XYBOXPLOT | X, Median, Q1, Q3, Min, Max | box plots |
| XYZ | X, Y, Z | Z shown as annotated values |
| XYR | X, Y, Radius | circles (radius in data units) |
| XYSIZE | X, Y, Size | symbols scaled by Size / znorm |
| XYCOLOR | X, Y, Value | symbols coloured through a colormap (or a Grace colour index) |
| XYCOLPAT | X, Y, Colour, Pattern | colour and pattern per point (pie charts) |
| XYVMAP | X, Y, VX, VY | arrows (vector fields), length scaled by znorm |

Coordinates of annotations and legends are in *world* (data) or *viewport* units. Viewport units are
relative to the page: the shorter page side has length 1. Text sizes and line widths are relative to
the page size too, so a figure scales consistently when the page size changes.

## 2. The window

- **Plot canvas** (centre). Click an element to select it; the inspector shows its properties.
- **Project** tree (left). Lists graphs, axes, legends, sets and annotations. Tick boxes show and hide
  them. Double-click a set to edit its data. Right-click for more actions.
- **Properties** inspector (right). Every change applies immediately and can be undone with **Ctrl+Z**.
- **Data** sheet (bottom). Spreadsheet view of the selected set. Edit cells, add or delete rows, and copy
  or paste blocks to and from other programs.
- **Console** (bottom tab). Runs Grace commands and expressions (see section 6).
- **Tools** (left toolbar). The table below lists them.

| Tool | Key | Use |
|---|---|---|
| Select | V | select, drag legends and annotations, drag inside a graph to pan |
| Pan | H | drag to pan the graph under the cursor |
| Zoom box / X / Y | Z, Shift+X, Shift+Y | drag a rectangle to zoom; click to zoom in ×2 |
| Crosshair | C | read coordinates and the nearest data point |
| Edit points | P | drag points; Ctrl+click deletes; Shift+click adds a point |
| Text, Line, Arrow, Box, Ellipse | T, L, A, B, E | add annotations |
| Region | R | click the polygon vertices, double-click to finish |

Mouse wheel: zoom the graph under the cursor (Shift: x only, Alt: y only). Ctrl+wheel zooms the
page view. Middle-button drag pans with any tool.

## 3. Importing data

**File ▸ Import Data** (Ctrl+I), drag and drop, or **Edit ▸ Import from Clipboard** (Ctrl+Shift+V)
opens a preview with three layouts:

1. **One set per data block.** Columns follow the chosen set type; blocks are separated by `&` lines.
2. **NXY.** The first column is X; every further column becomes a set.
3. **Choose columns.** Pick zero-based columns, e.g. `0:2:3` for X, Y and dY.

The importer accepts the following:

- whitespace, comma, semicolon and tab separators;
- `#` comment lines, with header names reused as legends;
- `nan` and `inf`;
- Fortran exponents such as `1.0D+03`;
- ISO dates (`2024-05-01T12:00:00`), US and European dates, and times;
- a quoted label at the end of a line, which can be shown as annotated values or pie labels.

## 4. Styling

- **Style presets** (*Plot ▸ Style Preset*, or the Page inspector) restyle the whole project at once
  and keep data, text and structure:
  - **Modern** is the default.
  - **Publication** uses serif fonts, inward ticks and no grid.
  - **Presentation** uses large text and thick lines.
  - **Dark**, **Minimal**, and **Journal** (for figures shrunk to one column).
  - **Grace Classic** reproduces xmgrace's look.
- **Colour palettes** (*Plot ▸ Colour Palette*) recolour every set from a palette, for example
  Okabe–Ito, Tol, Tableau, ColorBrewer or Glasbey.
- **Colour buttons** open a picker with five parts:
  - palettes and colormap strips;
  - the searchable list of about 1800 named colours;
  - an HSV/RGB editor with alpha and a screen colour picker;
  - recently used colours;
  - an opacity slider.
- **Text** anywhere (titles, labels, legends, annotations) accepts:
  - `$...$` math: `$\alpha_i^2$`, `$x^{-1}$`, `$\sqrt{x}$`, `$\Delta E$`, `$10^{-3}$`,
    `$\mathrm{d}x$`;
  - Grace escapes: `\S` superscript, `\s` subscript, `\N` normal, `\x` Symbol font, `\f{Times-Bold}`,
    `\R{red}`, `\u…\U` underline, `\o…\O` overline, `\+`/`\-` size, `\t{a b c d}` transform,
    `\r{angle}` rotation, `\#{b0}` character code.

## 5. Analysis

- **Data ▸ Evaluate Expression** (Ctrl+R) creates or replaces sets from formulas. For example, `x = mesh(0, 10, 500)`
  with `y = jv(0, x)`, or `y = s0.y - avg(s0.y)`.
- **Data ▸ Transformations** (Ctrl+T) offers the following transformations:
  - histograms;
  - Fourier transforms (magnitude, phase, power or dB, with 7 windows);
  - running average, median, min, max and standard deviation;
  - differentiation and integration;
  - linear, cubic-spline and Akima interpolation;
  - regressions (linear, polynomial up to degree 10, exponential, power, logarithmic, inverse,
    saturation);
  - correlation and convolution;
  - digital filters;
  - geometric transforms;
  - sampling and pruning;
  - seasonal differences;
  - cumulative sums;
  - sort, reverse and restrict (by expression or region);
  - feature extraction;
  - statistics.
- **Data ▸ Curve Fitting** (Ctrl+F) offers:
  - a library of models (polynomials, exponentials, Gaussian, Lorentzian, Voigt, logistic, power law,
    sine, Michaelis–Menten, Hill, Arrhenius) or any formula in `x` and `a0` … `a9`;
  - automatic initial guesses;
  - fixed or bounded parameters;
  - weights (1/y, 1/y², 1/σ²);
  - a live preview;
  - a report with standard errors, χ², reduced χ², R² and correlation;
  - a result set holding the fitted curve, values at the data points, or residuals.

The fitting engine is checked against the NIST StRD certified results.

## 6. Command language and scripting

The console, batch files (`-batch`), parameter files (`-param`) and `.agr`/`.cplot` projects all use
the same language. Statements may be separated by `;`. A few examples:

```
s0.x = mesh(0, 2*pi, 200); s0.y = sin(s0.x)
s1.y = s0.y^2 + rnorm(0, 0.05)
read xydy "measurements.dat"
autoscale
xaxes scale logarithmic
xaxis label "Frequency $f$ (Hz)"
s0 line color "tomato"; s0 line linewidth 2
fit formula "y = a0*exp(-a1*x)"; fit with 2 parameters; a0 = 1; a1 = 0.5
nonlfit(s1, 100)
arrange(2, 2, 0.1, 0.15, 0.2)
hardcopy device "pdf"; print to "figure.pdf"; print
```

**Help ▸ Command Reference** lists the statements and all functions. In safe mode, which is the
default for files, commands that write files (`saveall`, `write … file`) are refused. Pass `-nosafe`
to allow them.

## 7. Command line

`cactusplot [options] [files]` opens the GUI. `cactusbat` (or `-hardcopy`, `-o file`, `--export file`)
runs without a GUI and needs no display.

All Grace options are supported, processed in order:

- `-nxy`, `-block`/`-bxy`, `-settype`, `-graph`, `-graphtype`;
- `-autoscale`, `-world`, `-viewport`, `-log`, `-legend load`;
- `-batch`, `-param`, `-pexec`, `-pipe`, `-npipe`, `-dpipe`;
- `-hardcopy`, `-hdevice`, `-printfile`, `-saveall`, `-results`;
- `-rvideo`, `-timestamp`, `-seed`, `-wd`, `-nosafe`, `-noprint`, …

CactusPlot adds these options:

- `--style`, `--palette`;
- `--title`, `--xlabel`, `--ylabel`;
- `--dpi`, `--transparent`, `--tight`;
- `--list-presets`, `--list-palettes`, `--list-colormaps`, `--list-colors`.

Run `cactusplot -help` for the full list.

## 8. Exporting

**File ▸ Export** (Ctrl+E) writes the formats below. **Export Again** (Ctrl+Shift+E) repeats the
last export, which is handy while iterating with LaTeX.

- PDF and SVG (text stays text);
- EPS/PS (fonts converted to outlines, optional tight bounding box);
- PNG, JPEG, TIFF, BMP, PPM and WebP at any resolution, with optional transparency.

**Edit ▸ Copy Plot** puts both a 300 dpi image and SVG on the clipboard.

## 9. Files

- `.cplot` is the native project format: plain text in Grace syntax plus extensions, so it is
  lossless and diff-friendly.
- `.agr` is the Grace project format, read and written.
- The **Preferences** (Ctrl+,) store the following settings:
  - theme;
  - native or Fusion widget style;
  - default style for new plots;
  - autosave interval;
  - number of recent files;
  - script safe mode.
