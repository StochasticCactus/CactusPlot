# Command-line options

```
cactusplot [options] [project.cplot | project.agr | data files…]
cactusbat  [options] [files…]
```

Without options, `cactusplot` opens the window. With `-hardcopy`, `-o`/`--export` or `-version`,
or when started as `cactusbat`, it runs without a window: it reads everything, writes the output
and exits. That also works on servers without a display. `cactusbat` behaves like Grace's
`gracebat`. The Linux packages install it; elsewhere use `cactusplot -hardcopy`.

**Options are processed in order**, as in Grace. Something like `-settype xydy` applies to the
data files that follow it, and `-graph 1` sends the following files to graph 1.

## Files

| Argument | Effect |
|---|---|
| `file.cplot`, `file.agr`, `file.xvgr`, `file.par` | Opens the project (replacing the current one). |
| `file.xvg`, `file.dat`, any other file | Reads it as data into the current graph. Grace commands in the file (`@…` lines) are run. Columns are used as described in [How columns are used](../guide/data.md#how-columns-are-used); with `-settype` before the file, exactly as Grace does. |
| `-` | Reads data from standard input. |

## Grace-compatible options

| Option | Effect |
|---|---|
| `-autoscale what` | What to autoscale after reading data: `x`, `y`, `xy` or `none` (overrides parameter files). |
| `-batch file` | Run a command file after everything else has been read. |
| `-block file` | Read a file as block data, to make sets from chosen columns with `-bxy`. |
| `-bxy 0:2:3` | Make a set from columns of the block data (in the order of the current set type). |
| `-datehint hint` | How to read ambiguous dates in data files: `iso`, `european`, `us`, `days`, `seconds` or `nohint`. |
| `-dpipe fd` | Read data from an open file descriptor. |
| `-fixed width height` | Fixed canvas size (accepted for compatibility). |
| `-free` | Free page layout (accepted for compatibility). |
| `-graph n` | Make graph n current (created if needed). |
| `-graphtype type` | Type of the current graph: `xy`, `chart`, `fixed`, `polar`, `pie` or `smith`. |
| `-hardcopy` | Print to the hardcopy device and exit, without a window. |
| `-hdevice device` | Hardcopy device: `PDF`, `SVG`, `EPS`, `PostScript`, `PNG`, `JPEG`, `TIFF`, `BMP`, `PNM`/`PPM`, `WebP`. Run `cactusplot -version` to list what your build supports. |
| `-legend load` | Turn the legend on and label sets with their file names. |
| `-log axes` | Logarithmic axes for the current graph: `x`, `y` or `xy`. |
| `-noask` | Don't ask before overwriting files (accepted; CactusPlot never asks in batch mode). |
| `-noprint` | In hardcopy mode, don't print at the end. |
| `-nosafe` | Allow scripts to write files (`saveall`, `write … file`, `print to`). Off by default. |
| `-safe` | Turn safe mode back on. |
| `-npipe file` | Read data from a named pipe. |
| `-nxy file` | Read `X Y1 Y2 …`: one set per Y column. |
| `-param file`, `-p file` | Load a parameter file (Grace commands). |
| `-pexec "command"` | Run one command. Can be repeated. |
| `-pipe` | Read data from standard input. |
| `-printfile file` | Output file for hardcopy. |
| `-remove` | Delete data files after reading them. |
| `-results file` | Write the results of transformations and fits to a file. |
| `-rvideo` | Swap black and white (reverse video). |
| `-saveall file` | Save the project to a file (`.cplot` or `.agr`) before exiting. |
| `-seed n` | Seed for the random number functions (`rand`, `rnorm`), for reproducible output. |
| `-settype type` | Set type for the next data files: `xy`, `xydy`, `xydxdy`, `bar`, `xyz`, … |
| `-source from` | Where the next data file comes from: `disk` or `pipe`. |
| `-timestamp` | Print a time stamp on the page. |
| `-viewport xmin ymin xmax ymax` | Viewport of the current graph. |
| `-wd directory` | Working directory for relative file names. |
| `-world xmin ymin xmax ymax` | Data range of the current graph. |
| `-version`, `-usage`, `-help` | Print the version, or the list of options. |
| `-mono`, `-install`, `-noinstall`, `-nosigcatch`, `-barebones` | Accepted and ignored, for old scripts. |

## CactusPlot options

| Option | Effect |
|---|---|
| `-o file`, `--export file` | Export to `file` and exit. The format comes from the extension. |
| `--style name` | Style preset: `Modern`, `Publication`, `Presentation`, `Dark`, `Minimal`, `"Journal (single column)"`, `"Grace Classic"`. Also restyles a Grace project that is opened. |
| `--palette name` | Colour palette for the sets, e.g. `"Okabe-Ito"`, `"Tableau 10"`, `viridis`-style names from `--list-palettes`. |
| `--dpi n` | Resolution of raster exports (default 300). |
| `--transparent` | Transparent background (PNG, TIFF, WebP, SVG, PDF). |
| `--tight` | Tight bounding box for EPS. |
| `--title text`, `--xlabel text`, `--ylabel text` | Set the title and axis labels of the current graph. `$math$` works. |
| `--list-presets`, `--list-palettes`, `--list-colormaps`, `--list-colors` | Print what is available and exit. |

Options with two dashes can also be written with one (`-style`), and vice versa.

## Examples

Plot a data file into a PDF:

```bash
cactusbat data.dat -o data.pdf
```

Several columns, log y axis, labelled, as a PNG at 600 dpi:

```bash
cactusbat -nxy spectra.dat -log y --xlabel 'Energy (eV)' --ylabel 'Counts' -o spectra.png --dpi 600
```

Data with error bars in the publication style:

```bash
cactusbat -settype xydy measurements.dat --style Publication -o fig1.pdf
```

Two data files in two graphs, side by side:

```bash
cactusbat -graph 0 a.dat -graph 1 b.dat -pexec "arrange(1, 2, 0.1, 0.25, 0.15)" -o ab.pdf
```

Run a script that writes its own output (scripts may only write files with `-nosafe`):

```bash
cactusbat -batch figure.par -nosafe
```

Convert an old Grace project to the native format and a PDF:

```bash
cactusbat old.agr -saveall old.cplot -o old.pdf
```

Read from a pipe:

```bash
python make_data.py | cactusbat -pipe -o out.svg
```

## Exit status

`0` on success. A non-zero status means a file could not be read or written, or a script called
`exit(n)` with a non-zero `n`. Errors and warnings are printed to standard error, including
commands that were not understood.
