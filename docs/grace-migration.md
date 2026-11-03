# Moving from Grace / xmgrace

CactusPlot was designed so that Grace users can switch without converting anything.

## What keeps working

| Grace | CactusPlot |
|---|---|
| `xmgrace file.agr` | `cactusplot file.agr` (also `.xvgr`, `.par` and xmgr/ACE files) |
| `gracebat -hardcopy ...` | `cactusbat ...` or `cactusplot -hardcopy ...` (no display needed) |
| `-nxy`, `-block`/`-bxy`, `-settype`, `-batch`, `-param`, `-pexec`, `-pipe`, `-hdevice`, `-printfile`, `-saveall`, `-world`, `-viewport`, `-log`, `-autoscale`, `-graph`, `-legend load`, `-rvideo`, ... | same options, processed in the same order |
| Batch/parameter files and the command language | same syntax, including expressions, `nonlfit`, `runavg`, `interpolate`, `histogram`, `restrict`, `arrange`, `read block`, `write` |
| Font numbers, colour indices, patterns 0–31, symbols 0–11, line styles 0–8 | identical (Grace's 32 fill patterns are reproduced bit for bit) |
| Typesetting escapes `\f{} \x \S \s \N \R{} \u \o \t{} \r{} \#{}` | supported |
| Devices PostScript, EPS, PDF, SVG, PNG, JPEG, PNM | supported. MIF is mapped to SVG. TIFF, BMP and WebP are added |

The examples shipped with Grace render with matching geometry. CactusPlot's test suite compares them
side by side with `gracebat` output.

## Differences

- **Look.** Grace projects open in the *Grace Classic* style, so they look as before. *Plot ▸ Style
  Preset ▸ Modern* (or `--style Modern` on the command line) restyles them in one click.
- **Undo.** Every change can be undone. Grace has no undo.
- **Dialogs.** The many Grace dialogs are replaced by one live inspector. Grace's *Data ▸
  Transformations* items are grouped in one dialog.
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
