# Command language

CactusPlot speaks Grace's command language. The same language is used:

- in the **Commands** window (**Ctrl+K**);
- in batch files (`-batch file`) and parameter files (`-param file`);
- with `-pexec "…"` on the command line;
- inside `.agr` and `.cplot` project files, which are just long lists of commands followed by data.

Batch files and projects written for Grace work unchanged. This page describes the language.
The functions that expressions can use are listed in [Functions](functions.md).

## Basics

- One statement per line, or several separated by `;`.
- Keywords are case-insensitive: `XAXIS LABEL "t"` and `xaxis label "t"` are the same.
- Text goes in double quotes: `title "My plot"`.
- `#` starts a comment line.
- In files, a leading `@` is allowed (and is what Grace writes): `@title "x"` = `title "x"`.
- Lines of numbers in a file are data. They go into the current target set, and `&` ends a block
  (see [File formats](file-formats.md)).
- `on`/`off`, `true`/`false` and `yes`/`no` are interchangeable.

## Graphs and sets

| Reference | Means |
|---|---|
| `g0`, `g1`, … | graph 0, graph 1, … |
| `s0`, `s1`, … | set 0, set 1, … of the current graph |
| `g1.s2` | set 2 of graph 1 |
| `s_` or `s$` | the set created last |

| Statement | Effect |
|---|---|
| `with g1` | Make graph 1 current; following graph and axis statements apply to it (created if needed). |
| `focus g1` | Same, as in Grace. |
| `g1 on`, `g1 off`, `g1 hidden true` | Use, remove or hide a graph. |
| `g1 type xy` | Graph type: `xy`, `chart`, `polar`, `smith`, `fixed`, `pie`. |
| `g1 stacked true` | Stack sets in a chart graph. |
| `kill g1`, `kill s3`, `kill sets` | Delete a graph, a set, or all sets of the current graph. |

## Data

### Creating data with expressions

```
s0.x = mesh(0, 10, 200)            # 200 points from 0 to 10
s0.y = sin(s0.x) * exp(-s0.x / 4)
s1.y = s0.y + rnorm(0, 0.05)       # columns of a new set take their length from the right-hand side
g1.s0.y = s0.y^2                   # sets in other graphs
s0.y[0] = 1                        # one element
s0.y1 = 0.1 * abs(s0.y)            # the third column (e.g. the error of an XYDY set)
```

In an assignment to a set, `x`, `y`, `y1` … `y4` without a set name refer to that set, so
`s0.y = y * 2` doubles set 0. The point index is `i`.

Variables hold numbers or whole columns:

```
define tau                         # declare a scalar (optional)
tau = 2.5
define v[100]                      # a vector of 100 zeros
v = mesh(1, 100, 100)
clear tau                          # remove a variable
```

### Reading files

| Statement | Effect |
|---|---|
| `read "file.dat"` | Read data, one set per block, using the current set type. |
| `read xydy "file.dat"` | Read with a given set type. |
| `read nxy "file.dat"` | Read `X Y1 Y2 …`, one set per Y column. |
| `read block "file.dat"` | Read a table for `block`. |
| `block xydy "0:2:3"` | Make a set from columns 0, 2 and 3 of the block. |
| `type xydy` | Set type for the next data read (also as `@TYPE xydy` in data files). |
| `target g0.s3` | Send the next data block to that set. |
| `autoscale onread xyaxes` | What to autoscale after reading: `none`, `xaxes`, `yaxes`, `xyaxes`. |

### Changing sets

| Statement | Effect |
|---|---|
| `copy s0 to s1`, `copy g0.s0 to g1.s0` | Copy a set with its style. |
| `move s0 to s5` | Move a set. |
| `swap s0 and s1` | Exchange two sets. |
| `sort s0 x`, `sort s0 y descending` | Sort points. |
| `reverse s0` | Reverse the order of points. |
| `join s0, s1` | Append s1 to s0 and remove s1. |
| `split s0 100` | Cut s0 into pieces of 100 points; the pieces after the first go to new sets. |
| `drop s0 10 20` | Delete points 10 to 20. |
| `restrict(s0, s0.x > 2)` | Keep only points where the condition is true. |
| `restrict(s0, r1)`, `restrict(s0, r1, on)` | Keep points inside (or, with `on`, outside) region 1. |
| `write s0` | Print the set's data. |
| `write s0 file "out.dat" format "%.6g"` | Write it to a file (needs `-nosafe` in scripts). |

## Transformations

Each makes a new set from its result, in the graph of the source set.

| Statement | Result |
|---|---|
| `runavg(s0, 5)`, `runmed`, `runmin`, `runmax`, `runstd` | Running average, median, minimum, maximum, standard deviation over 5 points. |
| `interpolate(s0, mesh(0, 1, 101), linear)` | Interpolate at the given x values: `linear`, `spline` or `aspline` (Akima). Optional 4th argument `on` keeps only points inside the data range. |
| `histogram(s0, mesh(0, 1, 21), off, off)` | Histogram of y with the given bin edges; the flags are *cumulative* and *normalise*. |
| `integrate(s0)` | Cumulative integral; the total is printed. |
| `difference(s0, forward)` | Derivative: `forward`, `backward` or `centered`. |
| `seasonaldiff(s0, 12)` | y[i] − y[i−12]. |
| `fft(s0, magnitude)`, `dft(…)` | Fourier transform: `magnitude`, `phase`, `power`, `real`, `imag`, `db`. |
| `invfft(s0)`, `invdft(s0)` | Inverse transform. |
| `xcor(s0, s1, 50, off)` | Cross-correlation up to lag 50 (`on`: covariance). |
| `linconv(s0, s1)` | Linear convolution. |
| `regress(s0, 1)` | Linear (1) or polynomial (2–10) regression; the coefficients are printed. |

## Curve fitting

```
fit formula "y = a0 + a1*exp(-x/a2)"
fit with 3 parameters
a0 = 0; a1 = 1; a2 = 5               # starting values
a2 constraints on; a2min = 0.1; a2max = 100
nonlfit(s0, 200)                     # fit s0, at most 200 iterations
```

The fitted curve becomes a new set and the report (values, standard errors, χ², R²) is printed. The
fitted values stay in `a0` … `a9`, so they can be used later: `s5.y = a1 * exp(-s5.x / a2)`.

## Graph settings

These apply to the current graph (`with gN` first if needed).

```
world 0, -1, 10, 1                 # xmin, ymin, xmax, ymax
view 0.15, 0.15, 1.15, 0.85        # position on the page
autoscale                          # also: autoscale xaxes / yaxes / s2
autoticks
title "Damped oscillator"
subtitle "run 42"
title size 1.4
legend on
legend 0.85, 0.8                   # position (viewport coordinates by default)
legend loctype world
legend box off
legend char size 0.9
frame type 1                       # 0 closed, 1 half open, 2–5 one side open
frame linewidth 1.5
arrange(2, 2, 0.1, 0.15, 0.2)      # rows, columns, margin, horizontal gap, vertical gap
```

## Axes

The axes are `xaxis`, `yaxis`, `altxaxis` and `altyaxis`. `xaxes` and `yaxes` set the scale for
both x (or both y) axes.

```
xaxes scale logarithmic            # normal, logarithmic, reciprocal, logit
yaxes invert on
altxaxis on
xaxis label "Time $t$ (s)"
xaxis label char size 1.2
xaxis tick major 2                 # major tick spacing
xaxis tick minor ticks 3           # minor ticks between majors
xaxis tick in                      # in, out, both
xaxis tick major size 1.0
xaxis tick major grid on
xaxis ticklabel format decimal     # see the formats in Axes and ticks
xaxis ticklabel prec 1
xaxis ticklabel append " %"
xaxis ticklabel prepend "$"
xaxis ticklabel angle 45
xaxis ticklabel formula "$t - 273.15"
xaxis ticklabel off
yaxis bar off                      # the axis line
xaxis type zero true               # draw through y = 0
```

Special ticks and labels:

```
xaxis tick spec type both
xaxis tick spec 2
xaxis tick major 0, 1.0
xaxis ticklabel 0, "A"
xaxis tick major 1, 2.0
xaxis ticklabel 1, "B"
```

## Set appearance

```
s0 type xydy
s0 legend "Sample A"
s0 comment "from run 42"
s0 hidden false
s0 line type 1                     # 0 none, 1 straight, 2 left stairs, 3 right stairs, 4 segments, 5 three-segments, 6 centre stairs
s0 line linestyle 1                # 0 none, 1 solid, 2–8 dashes and dots
s0 line linewidth 2
s0 line color "steelblue"
s0 symbol 1                        # see Set types, symbols and lines
s0 symbol size 0.8
s0 symbol color "black"
s0 symbol fill color "#0072b2"
s0 symbol fill pattern 1
s0 symbol skip 4                   # draw every 5th symbol
s0 fill type 2                     # 0 none, 1 as polygon, 2 to baseline
s0 fill color "skyblue"
s0 fill pattern 1
s0 baseline type 0
s0 dropline on
s0 errorbar on
s0 errorbar size 0.5
s0 errorbar riser linewidth 1
s0 avalue on                       # annotated values
s0 avalue type 2                   # 1 x, 2 y, 3 x,y, 4 string, 5 z
s0 avalue prec 1
s0 colormap "viridis"              # XYCOLOR sets
s0 colormap range 0, 1
s0 colormap reversed on
s0 colormap bar on
```

## Annotations

Set the properties, then `def` creates the object:

```
with string
    string on
    string loctype view            # view (page) or world (data) coordinates
    string 0.2, 0.8
    string color "black"
    string char size 1.2
    string rot 0
    string just 0                  # 0 left, 1 right, 2 centre
    string def "Peak $\alpha$"

with line
    line on
    line loctype world
    line g0
    line 1, 0.5, 2, 0.8
    line linewidth 1.5
    line arrow 2                   # 0 none, 1 start, 2 end, 3 both
    line arrow type 1              # 0 line, 1 filled, 2 opaque
line def

with box
    box on
    box loctype world
    box g0
    box 2, 0.2, 4, 0.6
    box fill color "yellow"
    box fill pattern 1
box def
```

`ellipse` works like `box`.

## Regions

```
r0 type polyi                      # inside a polygon; also polyo, above, below, left, right, horizi, horizo, verti, verto
r0 xy 1, 1
r0 xy 3, 1
r0 xy 2, 3
r0 link to g0
r0 on
```

For half-planes and bands, give a line instead of points: `r1 line 0, 0.5, 10, 0.5`.

## Page and output

```
page size 792, 612                 # points; 612, 792 for portrait
page background fill on
page background color "white"
timestamp on
hardcopy device "pdf"              # pdf, svg, eps, ps, png, jpeg, tiff, bmp, ppm, webp
device "png" dpi 300
print to "figure.pdf"
print                              # write it now (needs -nosafe in scripts)
saveall "figure.cplot"             # save the project (needs -nosafe in scripts)
```

## Colours and fonts

Colours can be names (`"tomato"`), hex (`"#0072b2"`, `"#0072b280"` with alpha),
`rgb(…)`/`rgba(…)`, or Grace colour numbers. More numbered colours can be defined:

```
map color 16 to (40, 40, 40), "charcoal"
map font 4 to "Helvetica", "Helvetica"
```

## CactusPlot extensions

`.cplot` files are Grace syntax with a few additions, which xmgrace would not understand:

| Statement | Meaning |
|---|---|
| `cactusplot preset "Modern"` | The project's style preset. |
| `cactusplot pretty on` | Typographic numbers (−, ×10ⁿ). |
| `cactusplot page gradient 1`, `cactusplot page gradient color "…"` | Page background gradient. |
| `s0 fill gradient 1`, `s0 fill gradient color "…"` | Gradient fills (1 vertical, 2 horizontal, 3 radial). |
| `legend auto on` | Place the legend automatically. |
| `legend box radius 0.01` | Rounded legend box. |
| `xaxis tick major gridcolor "…"`, `gridlinewidth`, `gridlinestyle` | Grid line styling. |
| `s0 colormap …` | Colormaps for XYCOLOR sets. |
| Colours with alpha | `"#rrggbbaa"` and `rgba(…)`. |
| Symbols 12–16 | Pentagon, hexagon, filled star, thin plus, thin X. |

## Control

| Statement | Effect |
|---|---|
| `sleep 2` | Wait 2 seconds. |
| `redraw` | Redraw (in the GUI). |
| `exit`, `exit(1)` | Stop the script (and the program in batch mode) with a status. |

## Safe mode

Scripts and project files cannot write files unless safe mode is off. This protects you from
projects downloaded from the internet. `saveall`, `write … file` and `print` to a file are refused in
safe mode. Turn it off with `-nosafe` on the command line, or under **Edit ▸ Preferences** for
the GUI.

## Errors

A statement that is not understood is skipped with a warning; the rest of the script still runs.
In batch mode warnings go to standard error; in the GUI they appear in the Commands window, or
as a note when a project is opened.
