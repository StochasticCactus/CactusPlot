# Changing the appearance

All settings live in the **Appearance** window (**Window ▸ Appearance**, **Ctrl+Shift+P**). It
shows the settings of whatever is selected: the page, a graph, an axis, a set or an annotation.
To get there:

- **double-click** the element in the plot; or
- select it (click it, or click it in the Explorer) while the Appearance window is open; or
- use **Plot ▸ Plot Appearance…**, **Graph Appearance…**, **Set Appearance…** or
  **Axis Properties…**.

Changes apply immediately and can be undone (**Ctrl+Z**). Colour buttons open the
[colour picker](styles-colors.md#the-colour-picker).

## Units

- **Sizes of text, symbols and ticks** are relative: 1.0 is Grace's normal size, 2.0 twice as big.
  They scale with the page, so a figure keeps its proportions when the page size changes.
- **Line widths** are in Grace units: 1.0 is a thin line, a little under 1 pt on a Letter page.
- **Positions** are either in *world* coordinates (data values) or *viewport* coordinates (page
  units: the shorter side of the page is 1).

## Page

Click an empty part of the page outside the graphs, or use **Plot ▸ Plot Appearance…**.

| Section | Settings |
|---|---|
| **Page size** | **Preset**: Letter landscape and portrait, A4 landscape and portrait, Screen 4:3, Wide 16:9, Square, Journal 1 column (3.5 × 2.6 in), Journal 2 columns (7 × 4.3 in), or Custom. **Width** and **Height** in points (72 pt = 1 inch). |
| **Background** | **Fill the page background** (off gives a transparent background in PNG, SVG and PDF), **Colour**, **Gradient** (none, vertical, horizontal, radial) and **Gradient end** colour. |
| **Style** | Pick a **Preset** and click **Apply** to restyle the whole project. See [Styles](styles-colors.md#style-presets). |
| **Colours** | Recolour all sets with a palette. |
| **Typographic numbers** | On: tick labels use a real minus sign (−) and write powers as ×10ⁿ. Off: plain `-` and `1e+05`, as in Grace. |
| **Time stamp** | Print the date and time on the page, as Grace's time stamp does. |
| **Dates** | **Reference date**: the date that the number 0 stands for (as a Julian date). **Two-digit years**. See [Dates](axes.md#dates-and-times). |
| **Description** | Free text saved with the project. |

## Graph

Double-click the empty inside of a graph, or use **Plot ▸ Graph Appearance…**. There are three tabs.

### Graph tab

| Section | Settings |
|---|---|
| **Titles** | **Title** and **Subtitle** text (with `$math$` and Grace escapes), **Font**, **Title size**, **Subtitle size**, **Title colour**. |
| **Type and scales** | **Graph type** (XY, Chart, Polar, Smith, Fixed aspect, Pie), **Stacked (charts)**, **Bar gap**, **X scale** and **Y scale** (linear, logarithmic, reciprocal, logit), **Invert X**, **Invert Y**. |
| **World (data range)** | **X min**, **X max**, **Y min**, **Y max**, and the **Autoscale** button. |
| **Viewport (position on the page)** | **Left**, **Right**, **Bottom**, **Top** in page units. |
| Other | **Hidden** (not drawn or exported). **Size normalisation (znorm)** scales XYSIZE symbols and XYVMAP arrows. |

### Legend tab

| Setting | Meaning |
|---|---|
| **Show legend** | Turn the legend on or off (also **Ctrl+L**). Sets with an empty legend text are left out. |
| **Place automatically (avoid data)** | The legend goes to the corner or edge that covers the fewest data points. Turn it off to place it yourself, or simply drag the legend in the plot. |
| **Coordinates**, **X**, **Y** | Position of the legend's top-left corner, in world or viewport coordinates. |
| **Font**, **Text size**, **Text colour** | Legend text. |
| **Sample length** | Length of the line sample next to each entry. |
| **Row gap**, **Text gap** | Spacing between entries, and between the sample and the text. |
| **Reverse order** | List the sets from last to first. |
| **Box** | **Border**, **Border colour**, **Border width**, **Fill** colour (can be partly transparent) and **Corner radius**. |

### Frame tab

| Setting | Meaning |
|---|---|
| **Type** | **Closed** (a full box), **Half open** (left and bottom only), or one side left open: **Break top**, **Break bottom**, **Break left**, **Break right**. |
| **Line** | Colour, width and style of the frame. |
| **Plot area background** | Fill colour and pattern behind the data. |

## Axes

Double-click an axis, or use **Plot ▸ Axis Properties…**. A box at the top chooses which axis:
**X axis**, **Y axis**, **Alternate X axis** (top) or **Alternate Y axis** (right). The alternate
axes are off until you tick **Axis visible**. Scales, tick spacing and label formats are explained
in [Axes and ticks](axes.md).

### Axis tab

| Setting | Meaning |
|---|---|
| **Axis visible** | Draw this axis at all. |
| **Label** | The axis title, e.g. `Time $t$ (s)`, with **Font**, **Size**, **Colour**, **Orientation** (parallel or perpendicular to the axis) and **Side** (normal, opposite or both). |
| **Automatic placement** | Keep the label clear of the tick labels. Untick it to set **Offset along** and **Offset away** yourself. |
| **Axis line** | **Draw axis bar** (a line along the axis, separate from the frame), its **Colour** and **Width**. |
| **Place at zero of the other axis** | Draw the axis through 0 instead of along the edge (a "zero axis"). |
| **Offset** | Move the axis away from the frame, on the normal or the opposite side. |

### Ticks tab

| Setting | Meaning |
|---|---|
| **Draw ticks** | Ticks on or off. |
| **Major spacing** | Distance between major ticks, in data units. For log scales, a factor (10 means every decade). |
| **Minor ticks** | Number of minor ticks between two major ticks. |
| **Automatic count** | About how many major ticks automatic spacing should aim for. |
| **Choose spacing automatically** | Recompute nice tick spacing now. |
| **Round tick positions** | Put ticks at round numbers (0, 0.5, 1…) rather than starting at the axis minimum. |
| **Direction** | In, Out or Both. |
| **Sides** | Ticks on the normal side, the opposite side, or both. |
| **Major length**, **Minor length**, colours and widths | How ticks look. |
| **Grid** | **Major grid lines** and **Minor grid lines** across the graph, with colour, width and style. **Ctrl+G** toggles the major grid. |

### Labels tab (tick labels)

| Setting | Meaning |
|---|---|
| **Show tick labels** | Numbers on or off. |
| **Format** and **Precision** | How numbers are written: decimal, exponential, power, engineering, dates, degrees… See [Label formats](axes.md#tick-label-formats). |
| **Prefix**, **Suffix** | Text before or after each number, e.g. `$` or ` %`. |
| **Transform** | A formula applied to the value before printing, using `$t` for the tick value. For example `$t - 273.15` labels a kelvin axis in °C. |
| **Sign** | Normal, absolute value, or negated. |
| **Font**, **Size**, **Colour**, **Angle** | How the numbers look. |
| **Side**, **Skip**, **Stagger** | Which side; label only every n-th tick; alternate labels on two rows. |
| **Automatic offset**, **Offset along**, **Offset away** | Distance from the axis. |

## Sets

Double-click a curve, use **Plot ▸ Set Appearance…**, or right-click a curve ▸ **Properties**.

### Set tab

| Setting | Meaning |
|---|---|
| **Legend** | The set's name in the legend. Text, Grace escapes or `$math$`. |
| **Comment** | A note, e.g. the file the data came from. |
| **Set type** | How the columns are used (XY, XYDY, BAR…). See [Set types](../reference/set-types.md). |
| **Hidden** | Keep the set but don't draw it. |
| **Edit data…** | Opens the set editor for this set. |
| **Line** | **Connection** (none, straight, left/right/centre stairs, segments, three-segments), **Style** (solid, dashed, dotted…), **Width**, **Colour**. |
| **Drop lines** | A vertical line from each point down to the x axis. |
| **Draw baseline** | Draw the baseline used for fills and bars. |

### Symbols tab

| Setting | Meaning |
|---|---|
| **Symbol** | None, circle, square, diamond, triangles (four directions), plus, X, star, character, pentagon, hexagon, filled star, thin plus, thin X. |
| **Size** | 1.0 is the normal size. For BAR sets this is the bar width. |
| **Fill colour**, **Fill pattern** | Inside of the symbol. Pattern 0 is hollow, 1 is solid, 2–31 are Grace's hatch patterns. |
| **Edge colour**, **Edge width**, **Edge style** | Outline of the symbol. |
| **Draw every … th point** | Thin out symbols on dense data (the line still uses every point). |
| **Character code**, **Character font** | The character drawn by the *Char* symbol. |

### Fill tab

| Setting | Meaning |
|---|---|
| **Fill** | None, **As polygon** (close the curve on itself), or **To baseline** (shade the area under the curve). |
| **Baseline** | y = 0, set minimum or maximum, graph minimum or maximum, or set average. |
| **Colour**, **Pattern** | Fill colour (can be partly transparent) and pattern. |
| **Gradient**, **Gradient end** | A vertical, horizontal or radial colour gradient. |
| **Rule** | Winding or even-odd, for curves that cross themselves. |

### Errors tab

Error bars are drawn for set types with error columns (XYDX, XYDY, XYDXDY, BARDY, …).

| Setting | Meaning |
|---|---|
| **Show error bars** | On or off. |
| **Placement** | Both directions, positive only or negative only. |
| **Colour** | Usually the set's colour. |
| **Cap size**, **Cap width** | The short crossbar at the end. |
| **Riser width**, **Riser style** | The bar itself. |
| **Clip long risers (arrow)**, **Clip length** | Error bars longer than this are cut and end in an arrow. |

### Values tab (annotated values)

Prints a value next to each point.

| Setting | Meaning |
|---|---|
| **Annotate points with values** | On or off. |
| **Show** | X, Y, "X, Y", the point's text label, or Z (the third column). |
| **Format**, **Precision**, **Prefix**, **Suffix** | How numbers are written. |
| **Font**, **Size**, **Colour**, **Angle** | How the text looks. |
| **Offset X**, **Offset Y** | Distance from the point, in page units. |

### Colormap tab

For XYCOLOR sets: choose the **Colormap**, **Reverse** it, use an **Automatic range** or a fixed
**Minimum** and **Maximum**, and **Show colour bar**. **Third column is a Grace colour index**
reads the values as Grace's numbered colours instead (used by imported Grace projects).

## Annotations

Double-click a text, line, arrow, box or ellipse.

| Setting | Meaning |
|---|---|
| **Text** (text annotations) | The text, with `$math$` and Grace escapes (several lines allowed). **Font**, **Size**, **Colour**, **Angle**, **Alignment** (left, right, centre) and **Vertical** (baseline, bottom, top, middle). |
| **Line** (lines and arrows) | **Style**, **Width** and **Colour**. |
| **Arrow heads** | **Arrows**: none, at start, at end or both ends. **Shape** (line, filled, opaque), **Length**, **Width (d/L)** and **Inset (l/L)**. |
| **Border and fill** (boxes and ellipses) | **Border** style, **Border width**, **Border colour**, **Fill pattern**, **Fill colour** and **Rotation**. |
| **Position** | **Coordinates**: *World (moves with data)* ties the annotation to data values, so it follows zooming; *Viewport (fixed on page)* keeps it at the same place on the page. Then **X**, **Y** for text, or **X1**, **Y1**, **X2**, **Y2** for the two ends or corners. |

See [Annotations and regions](annotations.md).
