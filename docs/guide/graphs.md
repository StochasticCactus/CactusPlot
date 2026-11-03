# Graphs and sets

## How a figure is built

```
Project (one page)
 ├── Graph 0 ── axes, legend, frame, title
 │    ├── Set 0  (columns X, Y, …)
 │    ├── Set 1
 │    └── …
 ├── Graph 1 (another panel, or an inset)
 ├── annotations (text, lines, arrows, boxes, ellipses)
 └── regions R0–R4
```

Graphs are numbered G0, G1, …; sets inside a graph are S0, S1, …. In commands, `s1` is set 1 of
the current graph and `g1.s0` is set 0 of graph 1. The **current graph** is the one you clicked last.
New data goes into it, and its number is shown in the Explorer.

### World and viewport

Every graph has two rectangles:

- **World:** the data range shown. For example x from 0 to 10 and y from −1 to 1. It changes when
  you zoom, pan or autoscale. **Graph ▸ World** in the Appearance window, or
  `world 0, -1, 10, 1` in a command.
- **Viewport:** where the graph's frame sits on the page, in page units. The shorter side of the
  page is 1 long; on a landscape page, x runs from 0 to about 1.29 and y from 0 to 1.
  **Graph ▸ Viewport**, or `view 0.15, 0.15, 1.15, 0.85`.

Text sizes and line widths are relative to the page, too. If you change the page size, the
figure scales with it and keeps its proportions.

## Set types

A set's **type** says what its columns mean. Change it on the **Set** page of the Appearance window,
with `s0 type xydy`, or choose it when importing.

| Type | Columns | Drawn as |
|---|---|---|
| XY | X, Y | line and/or symbols |
| XYDX, XYDY, XYDXDY | X, Y + error(s) | error bars (symmetric) |
| XYDXDX, XYDYDY, XYDXDXDYDY | X, Y + separate plus and minus errors | error bars (asymmetric) |
| BAR, BARDY, BARDYDY | X, Y (+ errors) | vertical bars |
| XYHILO | X, High, Low, Open, Close | high-low-open-close bars (stock charts) |
| XYBOXPLOT | X, Median, Q1, Q3, Min, Max | box-and-whisker plots |
| XYZ | X, Y, Z | points, with Z available as an annotated value |
| XYR | X, Y, R | circles of radius R (in data units) |
| XYSIZE | X, Y, S | symbols whose size is proportional to S |
| XYCOLOR | X, Y, V | symbols coloured by V through a colormap, with an optional colour bar |
| XYCOLPAT | X, Y, C, P | a colour index and pattern for each point (mainly for pie charts) |
| XYVMAP | X, Y, VX, VY | arrows: vector fields |

Details for every type, plus the symbol and line types, are in
[Set types, symbols and lines](../reference/set-types.md).

### Colour-mapped scatter plots

An XYCOLOR set colours each point by its third column. On the **Set ▸ Values** part of the
Appearance window, choose:

- the **Colormap** (viridis, cividis, the cmocean and Crameri maps, and 150 more);
- **Reverse**, to flip the colormap;
- **Automatic range**, or your own **Minimum** and **Maximum**;
- **Show colour bar**.

The same in commands:

```
s0 type xycolor
s0 colormap "viridis"
s0 colormap bar on
```

## Graph types

**Plot ▸ Graph Type** or **Graph ▸ Type and scales ▸ Graph type** in the Appearance window.

| Graph type | Use |
|---|---|
| **XY** | The usual x–y plot. |
| **Chart** | Bar charts. Bars of several sets are placed side by side at each X. Tick **Stacked (charts)** to stack them instead; **Bar gap** sets the space between groups. Error bars follow the bars. |
| **Fixed aspect** | Like XY, but one unit in x is as long as one unit in y. Use it for maps, trajectories and circles that must look round. |
| **Polar** | X is the angle in radians, Y is the radius. The world y range sets the radius of the outer circle; x from 0 to 2π gives a full circle, and a smaller range gives a sector. |
| **Smith** | Smith chart for reflection coefficients: X and Y are the real and imaginary parts. |
| **Pie** | The first set is drawn as a pie. X holds the slice values and Y how far each slice is pulled out (0 = not at all, 0.1 = by 10 % of the radius). Colours come from the style's palette; with XYCOLPAT you choose a colour and pattern per slice. Turn on annotated values to label slices with values, percentages or the point labels. |

## Several graphs on a page

- **Plot ▸ Add Graph** adds an empty graph in the style of the current one and stacks all graphs
  in one column. Use **Arrange Graphs** to change the layout.
- **Plot ▸ Arrange Graphs…** lays graphs out in a grid. It asks for:
  - **Rows** and **Columns**;
  - **Page margin**;
  - **Horizontal gap** and **Vertical gap** (as fractions of a graph's size);
  - **Fill column by column**;
  - **Snake order**.

  Graphs are added if the grid has more cells than there are graphs.
- In commands: `arrange(2, 2, 0.1, 0.15, 0.2)` for 2 rows, 2 columns, margin 0.1, gaps 0.15 and
  0.2.
- Move sets between graphs with `move g0.s1 to g1.s0`, or copy them with `copy`.
- Click a graph to make it current. The **Explorer** shows all graphs. Its right-click menu
  duplicates, hides or deletes a graph.

> **Tip.** Graphs with titles need bigger gaps, because a title sits above the graph's frame.
> For panels of a figure, leave the titles empty and label the panels with text annotations
> such as "(a)" and "(b)".

## Insets

**Plot ▸ Add Inset Graph** puts a small graph in the top-right corner of the current one, with
slightly smaller text and an opaque background. Move and resize it by changing its viewport:
**Graph ▸ Viewport** in the Appearance window. Draw the zoomed part of the
data in it by copying the set (`copy g0.s0 to g1.s0`) and setting the inset's world range.

## Hiding and locking

- A **hidden** graph (**Graph ▸ Hidden**) is not drawn and not exported, but it is kept.
- Hidden sets (untick them in the Explorer) are kept, but not drawn and not autoscaled.

## Autoscale

**Plot ▸ Autoscale** (**Ctrl+Shift+A**) fits the world range to the visible sets, with round
numbers at the ends. **Autoscale X** and **Autoscale Y** do one axis only; **Autoscale All
Graphs** does every graph. In commands: `autoscale`, `autoscale xaxes`, `autoscale yaxes`,
`autoscale s1` (fit to one set).
