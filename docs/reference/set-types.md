# Set types, symbols and lines

## Set types

The name in the first column is what you write in commands (`s0 type xydy`, `read xydy "f.dat"`,
`-settype xydy`) and what appears in `.agr` files. Columns are given in file order.

| Type | Columns | How it is drawn |
|---|---|---|
| `xy` | X Y | Line and/or symbols. |
| `xydx` | X Y dX | Horizontal error bars of ±dX. |
| `xydy` | X Y dY | Vertical error bars of ±dY. |
| `xydxdx` | X Y dX+ dX− | Horizontal error bars, different on each side. |
| `xydydy` | X Y dY+ dY− | Vertical error bars, different on each side. |
| `xydxdy` | X Y dX dY | Error bars in both directions. |
| `xydxdxdydy` | X Y dX+ dX− dY+ dY− | Asymmetric error bars in both directions. |
| `bar` | X Y | A bar from the baseline to Y. The bar width is the symbol size. |
| `bardy` | X Y dY | Bars with error bars. |
| `bardydy` | X Y dY+ dY− | Bars with asymmetric error bars. |
| `xyhilo` | X High Low Open Close | A vertical line from Low to High with ticks for Open (left) and Close (right): stock charts. |
| `xyz` | X Y Z | Points; Z can be printed next to them as an annotated value. |
| `xyr` | X Y Radius | A circle of the given radius (in x units) around each point. |
| `xysize` | X Y Size | Symbols scaled by Size (divided by the graph's *znorm*). |
| `xycolor` | X Y Value | Symbols coloured by Value through a colormap, with an optional colour bar. In Grace projects the value is a colour number instead. |
| `xycolpat` | X Y Colour Pattern | A colour number and fill pattern for each point; used for pie slices. |
| `xyvmap` | X Y VX VY | An arrow from each point in the direction (VX, VY), scaled by *znorm*: vector fields. |
| `xyboxplot` | X Median Q1 Q3 Min Max | A box from Q1 to Q3 with a line at the median and whiskers to Min and Max. |

Changing the type of an existing set keeps the columns both types share. New columns start at zero,
and columns the new type does not use are removed. Undo (**Ctrl+Z**) brings them back.

## Symbols

| Number | Symbol | Number | Symbol |
|---|---|---|---|
| 0 | none | 9 | X (cross) |
| 1 | circle | 10 | star (asterisk) |
| 2 | square | 11 | character (choose the character and font) |
| 3 | diamond | 12 | pentagon * |
| 4 | triangle up | 13 | hexagon * |
| 5 | triangle left | 14 | filled star * |
| 6 | triangle down | 15 | thin plus * |
| 7 | triangle right | 16 | thin X * |
| 8 | plus | | |

\* CactusPlot additions. When saving as `.agr`, they are written as the closest Grace symbol.

In the modern styles, symbols are filled with the set colour and get a thin darker edge. Grace's
style draws hollow symbols.

## Line connection types (`s0 line type n`)

| Number | Type | Drawing |
|---|---|---|
| 0 | none | symbols only |
| 1 | straight | straight lines between points |
| 2 | left stairs | horizontal then vertical (step at the end of each interval) |
| 3 | right stairs | vertical then horizontal (step at the start) |
| 4 | segments | separate segments between points 0–1, 2–3, … |
| 5 | three-segments | segments between points 0–1–2, 3–4–5, … |
| 6 | centre stairs | step half-way between points |

## Line styles (`linestyle n`)

0 is no line, 1 is solid, and 2–8 are Grace's dotted and dashed patterns. The style menus in the
Appearance window show each pattern.

## Fill (`s0 fill type n`, `s0 baseline type n`)

| Fill type | Meaning |
|---|---|
| 0 | no fill |
| 1 | as polygon: the curve is closed on itself |
| 2 | to baseline: the area between the curve and the baseline |

| Baseline type | Baseline |
|---|---|
| 0 | y = 0 |
| 1 | the set's minimum |
| 2 | the set's maximum |
| 3 | the graph's minimum (bottom of the plot) |
| 4 | the graph's maximum (top of the plot) |
| 5 | the set's average |

## Fill patterns

Pattern 0 is no fill and 1 is solid. Patterns 2–31 are Grace's hatchings and dot screens,
reproduced bit for bit. The pattern menus show them.

## Annotated values (`s0 avalue type n`)

| Number | Shows |
|---|---|
| 0 | nothing |
| 1 | X |
| 2 | Y |
| 3 | X, Y |
| 4 | the point's text label |
| 5 | Z (the third column) |
