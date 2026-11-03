# File formats

## Projects

| Extension | Format | Read | Write |
|---|---|---|---|
| `.cplot` | CactusPlot project | ✓ | ✓ (default for **Save**) |
| `.agr` | Grace project | ✓ | ✓ (**Save As ▸ Grace project**) |
| `.xvgr`, `.par` | older xmgr/ACE projects and Grace parameter files | ✓ | ✓ (Grace syntax) |

### `.cplot`

A `.cplot` file is a plain text file in Grace syntax, plus the
[CactusPlot extensions](command-language.md#cactusplot-extensions):

```
# CactusPlot project file
#
@version 50125
@cactusplot preset "Modern"
@page size 792, 612
...
@target G0.S0
@type xy
0 1
0.1 0.995
...
&
```

It is lossless: transparency, gradients, colormaps and every other setting are kept. Being text, it
works well with version control (git shows meaningful diffs) and can be edited in any editor. The
file is UTF-8.

### `.agr`

Grace projects open exactly as Grace shows them, in the *Grace Classic* style. CactusPlot reads
projects from Grace 5.x and from older xmgr versions; it understands their font numbering, colour
table and symbol numbers. The Grace examples render with the same geometry as `gracebat`.

Saving as `.agr` writes strict Grace syntax (Latin-1) that xmgrace 5.1 can open. Things Grace
cannot represent are converted:

- colours go into Grace's numbered colour table, without transparency;
- gradients become plain fills;
- the extra symbols become the nearest Grace symbol;
- colormaps, rounded legends and automatic legend placement are dropped.

**Save As** asks for confirmation before writing `.agr`. An `.agr` file you opened is saved back
as `.agr` by **Save**, as in Grace. If you have used CactusPlot-only features, use **Save As** and
choose `.cplot` to keep them.

## Data files

Data files are plain text tables. The rules are in
[Getting data in](../guide/data.md#data-file-format). In short:

- columns separated by spaces, tabs, commas or semicolons;
- `#` comment lines, with optional column names on the last one;
- `&` between blocks (each block is a set);
- `@` lines are Grace commands, such as `@TYPE xydy` or `@ s0 legend "A"`;
- optional dates, times and a quoted text label at the end of a line.

Common extensions are `.dat`, `.txt`, `.csv`, `.tsv`, `.xy`, `.nxy`, `.out` and `.xvg` (GROMACS),
but the extension does not matter: any text file can be read. Files may be UTF-8 or Latin-1.

### Writing data

- **Set editor:** select cells and copy (**Ctrl+C**). They paste into spreadsheets as columns.
- **Command:** `write s0 file "out.dat"` writes the set's columns, space-separated. Add
  `format "%.6g"` to control the number format. In scripts this needs `-nosafe`.
- **Saving as `.agr` or `.cplot`** includes the data of every set.

## Output formats

PDF, SVG, EPS, PostScript, PNG, JPEG, TIFF, BMP, PPM/PNM and WebP. See
[Exporting and printing](../guide/export.md).

## Settings

CactusPlot keeps its preferences, recent files and window positions in the usual place for
each system:

| System | Location |
|---|---|
| Linux | `~/.config/CactusPlot/CactusPlot.conf` |
| macOS | `~/Library/Preferences/org.cactusplot.CactusPlot.plist` |
| Windows | registry, `HKEY_CURRENT_USER\Software\CactusPlot\CactusPlot` |

The autosaved project is `autosave.cplot` in the application data folder
(`~/.local/share/CactusPlot/CactusPlot/` on Linux).
