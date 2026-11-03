# Axes and ticks

Each graph has four axes: **x** (bottom), **y** (left), and the **alternate x** (top) and
**alternate y** (right). The alternate axes are off by default. Settings are on the axis pages of
the Appearance window, described in [Changing the appearance](appearance.md#axes). This page
explains the concepts.

## Scales

**Plot ▸ X Scale** and **Plot ▸ Y Scale**, or **Graph ▸ Type and scales** in the Appearance window.

| Scale | Use |
|---|---|
| **Linear** | The default. |
| **Logarithmic** | Data spanning several orders of magnitude. Values ≤ 0 are not drawn. Ticks fall on powers of ten, with minor ticks at 2, 3, … 9. |
| **Reciprocal** | Plots 1/x, as in Arrhenius plots of rate against temperature. |
| **Logit** | For fractions between 0 and 1 (probabilities, conversions); spreads out the ends near 0 and 1. |

**Invert X** and **Invert Y** flip an axis, e.g. for wavenumbers or depth. Commands:
`xaxes scale logarithmic`, `yaxes invert on`. From the command line: `-log y`.

## Tick spacing

Ticks are chosen automatically: about six major ticks at round values (1, 2, 2.5 or 5 times a power
of ten), with minor ticks between. When you zoom or autoscale, the spacing is recomputed.

To set it yourself, use the **Ticks** tab: **Major spacing** (in data units) and **Minor ticks**
(how many between two majors). **Automatic count** changes how many major ticks the automatic
spacing aims for. In commands:

```
xaxis tick major 0.5
xaxis tick minor ticks 4
autoticks                 # back to automatic spacing for the current graph
```

### Ticks at chosen places

For ticks with your own positions and labels, for example days of the week or sample names,
use special ticks. In commands (also in batch files):

```
xaxis tick spec type both      # both = special ticks and labels; ticks = special ticks, automatic labels
xaxis tick spec 3              # three special ticks
xaxis tick major 0, 1          # tick 0 at x = 1
xaxis ticklabel 0, "Mon"
xaxis tick major 1, 3
xaxis ticklabel 1, "Wed"
xaxis tick major 2, 5
xaxis ticklabel 2, "Fri"
```

`xaxis tick minor 3, 2` adds a minor tick (number 3) at x = 2. `xaxis tick spec type none` returns to
automatic ticks.

## Tick label formats

The **Labels** tab's **Format** and **Precision** decide how numbers are written. With
**Typographic numbers** on (the default outside the Grace style), negative numbers get a real minus
sign and powers are written as ×10ⁿ. Here is each format with precision 2:

| Format | Value | Label |
|---|---|---|
| `general` | 15320.5 | 1.5×10⁴ (shortest form; small and medium numbers are written plainly) |
| `decimal` | 15320.5 | 15320.50 |
| `exponential` | 15320.5 | 1.53×10⁴ |
| `scientific` | 15320.5 | 1.53×10⁴ |
| `power` | 1000 | 10³ (for log axes: writes the value as a power of ten) |
| `engineering` | 15320.5 | 15.32 k (SI prefixes: µ, m, k, M, G, …) |
| `computing` | 15320.5 | 14.96K (powers of 1024: K, M, G…) |
| `ddmmyy` | 18 May 2024 | 18-05-2024 |
| `mmddyy` | 18 May 2024 | 05-18-2024 |
| `yymmdd` | 18 May 2024 | 2024-05-18 |
| `mmyy` | 18 May 2024 | 05-2024 |
| `mmdd` | 18 May 2024 | 05-18 |
| `monthday` | 18 May 2024 | May-18 |
| `daymonth` | 18 May 2024 | 18-May |
| `months` | 18 May 2024 | May |
| `monthsy` | 18 May 2024 | May-2024 |
| `monthl` | 18 May 2024 | May (full month name) |
| `dayofweek` | 18 May 2024 | Sat |
| `dayofweekl` | 18 May 2024 | Saturday |
| `dayofyear` | 18 May 2024 | 139 |
| `hms` | 14:30:15 | 14:30:15 |
| `mmddhms` | 17 May 2024, 14:30:15 | 05-17 14:30:15 |
| `mmddyyhms` | 17 May 2024, 14:30:15 | 05-17-2024 14:30:15 |
| `yymmddhms` | 17 May 2024, 14:30:15 | 2024-05-17 14:30:15 |
| `degreeslon` | −23.4567 | 23.46°W |
| `degreesmmlon` | −23.4567 | 23°27.40'W |
| `degreesmmsslon` | −23.4567 | 23°27'24.12"W |
| `mmsslon` | −23.4567 | 27'24.12"W |
| `degreeslat`, `degreesmmlat`, `degreesmmsslat`, `mmsslat` | −23.4567 | as above, with N/S |

Other options on the Labels tab:

- **Prefix** and **Suffix** add text, e.g. a suffix of ` %` or `°C`.
- **Transform** applies a formula to the value before writing it; `$t` is the value. For example,
  `$t - 273.15` labels a kelvin axis in degrees Celsius, and `$t * 1000` shows seconds as
  milliseconds. The ticks stay where they are.
- **Sign** writes absolute values (useful for butterfly plots) or negated values.
- **Angle**, **Skip** (label every n-th tick) and **Stagger** (two rows) help with crowded labels.

In commands: `xaxis ticklabel format engineering`, `xaxis ticklabel prec 1`,
`xaxis ticklabel append " %"`, `xaxis ticklabel formula "$t - 273.15"`, `xaxis ticklabel angle 45`.

## Dates and times

Dates are stored as numbers of days. A data file with dates in it (see
[Data file format](data.md#data-file-format)) is converted to days, and an axis with a date
format writes them back as dates.

- **Reference date** (**Page ▸ Dates**): the date that the value 0 stands for, given as a Julian
  day. With the reference date at 0, values are plain Julian days, as in Grace.
- **Two-digit years**: write `24` instead of `2024`. The Grace style uses two digits.
- Date-only formats round to the nearest day, as in Grace. A tick at 14:30 on 17 May is labelled
  18 May; use a format with the time (`mmddhms`) when the hour matters.
- In expressions, convert values yourself if needed. For example, `s0.x = s0.x * 24` turns days
  into hours.

## Alternate axes

The alternate x axis is at the top of the graph and the alternate y axis on the right. Switch one on
with **Axis visible** after choosing it at the top of the axis page, or with `altxaxis on`. They
use the same data range as the main axes, but can have their own ticks, formats and labels. This
gives a second unit for the same data, e.g. eV below and nm above, with the **Transform** formula
on the top axis's labels.

## Zero axes and offsets

- **Place at zero of the other axis** draws the axis through 0 of the other axis, so it crosses
  the plot instead of running along its edge.
- **Offset** moves an axis away from the frame. Many journals ask for axes that are slightly
  detached from the plot.

## Grid lines

**Plot ▸ Toggle Grid Lines** (**Ctrl+G**) switches the major grid on both axes. The **Ticks** tab
has separate major and minor grids, with colour, width and line style. The Modern style uses a
light grey major grid; Publication and Grace Classic have none.
