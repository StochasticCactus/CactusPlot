# Moving from Grace / xmgrace

CactusPlot was designed so that Grace users can switch without converting anything.

## What keeps working

| Grace | CactusPlot |
|---|---|
| `xmgrace file.agr` | `cactusplot file.agr` (also `.xvgr`, `.par` and xmgr/ACE files) |
| `xmgrace rmsd.xvg` (GROMACS output) | `cactusplot rmsd.xvg`, or double-click/drag the file. Titles, labels and legends are kept, `@TYPE xydy` shows error bars, and each column becomes a set without `-nxy`. In the modern styles the xmgrace page layout in the file (`view`, legend position) is replaced by the automatic one; the Grace Classic style keeps it |
| `gracebat -hardcopy ...` | `cactusbat ...` or `cactusplot -hardcopy ...` (no display needed) |
| `-nxy`, `-block`/`-bxy`, `-settype`, `-batch`, `-param`, `-pexec`, `-pipe`, `-hdevice`, `-printfile`, `-saveall`, `-world`, `-viewport`, `-log`, `-autoscale`, `-graph`, `-legend load`, `-rvideo`, ... | same options, processed in the same order |
| Batch/parameter files and the command language | same syntax, including expressions, `nonlfit`, `runavg`, `interpolate`, `histogram`, `restrict`, `arrange`, `read block`, `write` |
| Font numbers, colour indices, patterns 0–31, symbols 0–11, line styles 0–8 | identical (Grace's 32 fill patterns are reproduced bit for bit); symbols 12–16 are added |
| Typesetting escapes `\f{} \x \S \s \N \R{} \u \o \t{} \r{} \#{}` | supported |
| Devices PostScript, EPS, PDF, SVG, PNG, JPEG, PNM | supported. MIF is mapped to SVG. TIFF, BMP and WebP are added |

The examples shipped with Grace render with matching geometry. CactusPlot's test suite compares them
side by side with `gracebat` output.

## Where are Grace's menus?

| In xmgrace | In CactusPlot |
|---|---|
| *Plot ▸ Plot appearance* | **Plot ▸ Plot Appearance…**, or double-click the page |
| *Plot ▸ Graph appearance* | **Plot ▸ Graph Appearance…**, or double-click inside a graph |
| *Plot ▸ Set appearance* | **Plot ▸ Set Appearance…**, or double-click a curve |
| *Plot ▸ Axis properties* | **Plot ▸ Axis Properties…**, or double-click an axis |
| *Plot ▸ Arrange graphs* | **Plot ▸ Arrange Graphs…** |
| *Data ▸ Data set operations* | right-click a curve or use the **Explorer**; `copy`, `move`, `swap`, `kill`, `sort`, `join`, `split` in the Commands window |
| *Data ▸ Transformations ▸ …* | **Data ▸ Transformations…** (all of them in one window) |
| *Data ▸ Transformations ▸ Non-linear curve fitting* | **Data ▸ Curve Fitting…** |
| *Data ▸ Transformations ▸ Evaluate expression* | **Data ▸ Evaluate Expression / New Set…** |
| *Data ▸ Import ▸ ASCII* | **File ▸ Import Data…** |
| *Edit ▸ Data sets* (spreadsheet) | **Data ▸ Edit Set (Spreadsheet)…** |
| *Edit ▸ Regions* | **Data ▸ Regions** |
| *Window ▸ Commands* | **Window ▸ Commands…** (**Ctrl+K**) |
| *Window ▸ Explorer* | **Window ▸ Explorer** |
| *File ▸ Print setup* / *Print* | **File ▸ Export…** / **Print…** |
| Toolbar buttons (AS, Z, z, ←, →, …) | the button column on the left, plus the mouse wheel for zooming |

## Differences

- **Look.** Grace projects open in the *Grace Classic* style, so they look as before. *Plot ▸ Style
  Preset ▸ Modern* (or `--style Modern` on the command line) restyles them in one click.
- **Undo.** Every change can be undone. Grace has no undo.
- **Dialogs.** Grace's appearance dialogs are combined in one **Appearance** window, which follows
  the selection and applies changes immediately (no *Apply* button). Grace's *Data ▸
  Transformations* items are grouped in one window.
- **Native format.** New projects are saved as `.cplot`. This is Grace syntax plus extensions:
  - RGBA colours;
  - colormaps for XYCOLOR sets;
  - gradients;
  - rounded and auto-placed legends;
  - extra symbols;
  - grid styling.

  **Save As ▸ Grace project (`.agr`)** writes strict Grace syntax that xmgrace can read. Colours are
  then mapped to a colour table and the extensions are dropped.
- **Dates.** Modern styles print four-digit years. Grace Classic and `date wrap on` print two digits.
- **Safe mode.** As in Grace, file-writing commands in scripts are refused unless `-nosafe` is given.

## Not implemented

- NetCDF input (`-netcdf`): convert the data to text first.
- Real-time input polling (`-timer`): re-run the batch instead.
- Printing directly to a printer from batch mode: write a PDF and print it.

## More

- [Command language](reference/command-language.md): everything Grace scripts can use, and what CactusPlot adds
- [Command-line options](reference/command-line.md)
- [GROMACS files](guide/gromacs.md)
