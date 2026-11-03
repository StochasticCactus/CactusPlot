# Exporting and printing

## Export

**File ▸ Export…** (**Ctrl+E**).

| Setting | Meaning |
|---|---|
| **Format** | One of the formats below. |
| **File** | Where to write. **Browse…** chooses a folder and name; the extension follows the format. |
| **Resolution** | For raster formats only, in dots per inch. The pixel size is shown below the settings. 300 dpi suits print, 600 dpi line art, 150 dpi slides and the web. |
| **Transparent background** | Leave the page background empty (PNG, TIFF, WebP, SVG and PDF). |
| **Greyscale** | Convert all colours to grey, for journals that print in black and white. |
| **Tight bounding box (EPS)** | Crop the EPS to the drawn content instead of the whole page. |

| Format | Kind | Best for |
|---|---|---|
| **PDF** | vector | papers, LaTeX (pdflatex, lualatex), sharing. Fonts are embedded. |
| **SVG** | vector | web pages, editing in Inkscape or Illustrator, PowerPoint and Word (Office 2019 and later). |
| **EPS** | vector | LaTeX with dvips, journals that ask for EPS. Text is converted to outlines, so it looks right everywhere. |
| **PostScript** | vector | printing, older workflows. |
| **PNG** | raster, lossless | slides, web, Word, quick sharing. |
| **JPEG** | raster, lossy | photos; not recommended for plots (lines get blurry). |
| **TIFF** | raster | journals that require TIFF. |
| **BMP**, **PPM** | raster | old programs. |
| **WebP** | raster | web pages. |

TIFF and WebP are offered only if your Qt installation includes their image plugins (the Windows,
macOS and AppImage builds do).

**File ▸ Export Again** (**Ctrl+Shift+E**) writes the same file with the same settings without
asking. Use it when you are adjusting a figure while viewing it in a LaTeX document.

## Size of the figure

The exported figure has the size of the **page**: **Page ▸ Page size** in the Appearance window.
Choose the size the figure will have in print and set the fonts for that size, rather than shrinking
a big page later; that way the text in the figure matches the text of the paper.

| Target | Page size preset | Notes |
|---|---|---|
| One journal column | **Journal, 1 column (3.5 × 2.6 in)** | Use the **Journal (single column)** style, or check that text is at least 7 pt. |
| Full page width | **Journal, 2 columns (7 × 4.3 in)** | |
| Slides | **Wide 16:9** with the **Presentation** style | |
| Square figure | **Square** | |
| Exact size | **Custom**, width and height in points | 72 pt = 1 in = 25.4 mm |

## Copying to other programs

- **Edit ▸ Copy Plot as Image** (**Ctrl+Shift+C**) puts the figure on the clipboard both as an image
  at 300 dpi and as SVG. Programs that understand SVG (Inkscape, recent Office) paste the vector
  version; others paste the image.
- **Edit ▸ Copy Plot as SVG** also puts the SVG code on the clipboard as text, for pasting into a
  web page or an editor.

## Printing

**File ▸ Print…** (**Ctrl+P**) prints through your system's print dialog. On Linux and macOS the
dialog can also print to a PDF file.

## Exporting from the command line

```bash
cactusplot project.cplot -o figure.pdf               # format from the extension
cactusplot data.dat -o figure.png --dpi 600 --transparent
cactusbat -batch make_figure.par -nosafe            # the script chooses device and file
cactusplot project.agr -hardcopy -hdevice EPS -printfile figure.eps --tight
```

See [Command line](../reference/command-line.md).

## LaTeX tips

- With **pdflatex** or **lualatex**, export PDF and use `\includegraphics{figure.pdf}`.
- With **latex + dvips**, export EPS with **Tight bounding box**.
- For fonts that match a Times-based paper, use the **Publication** style (serif) or set the fonts
  to Times-Roman. For Computer Modern or Latin Modern, install the font and choose it by name.
- Write labels with `$...$` math so symbols match the paper: `$\lambda$ (nm)`,
  `$k_\mathrm{B}T$`.
