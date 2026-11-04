# Frequently asked questions and troubleshooting

## General

**Is CactusPlot a fork of Grace?**
No. It is written from scratch in C++ with Qt 6. It reads and writes Grace's file format and
understands Grace's command language, so your Grace files, scripts and habits carry over. It
contains no Grace code.

**Does it need XQuartz on macOS?**
No. It is a native macOS app for Apple silicon and Intel Macs.

**Does it work on Wayland?**
Yes. It runs natively on Wayland (GNOME, KDE Plasma, COSMIC) and on X11.

**Is it free?**
Yes. It is free software under the GNU GPL version 2 or later. You can use it for any purpose,
including commercial work and publications.

**How do I cite it?**
Cite the repository, `https://github.com/StochasticCactus/CactusGui`, with the version you used
(**Help ▸ About**).

## Plots

**My Grace project looks exactly like it did in Grace. How do I make it look modern?**
**Plot ▸ Style Preset ▸ Modern**. Grace projects deliberately open in the Grace look, so nothing
changes unexpectedly.

**The legend covers my data.**
The automatic placement tries the four corners and the middle of each edge, and picks the spot that
covers the fewest points. If all are busy, drag the legend somewhere else (this turns automatic
placement off), or make room with **Graph ▸ World** (raise *Y max*).

**My symbols are hollow / filled; I want the opposite.**
On the **Set ▸ Symbols** tab, set **Fill pattern** to 0 (hollow) or 1 (solid).

**How do I get ticks on all four sides?**
Axis page ▸ **Ticks** tab ▸ **Sides** ▸ *Both*, for the x and y axes. The **Publication** style
does this for you.

**How do I make a broken axis?**
Use two graphs (**Plot ▸ Add Graph**), set their world ranges to the two parts, and choose a
**Frame ▸ Type** with the touching sides open (*Break top* and *Break bottom*).

**How do I plot y against x from a table with many columns?**
Import with the **NXY** layout (one set per column), or **Choose columns** (`0:3` for columns 0 and 3).

**How do I make a log plot of data with zeros?**
Points with x ≤ 0 (or y ≤ 0) cannot be drawn on a log axis and are skipped. Use **Autoscale** after
switching to log, so that the range starts above zero.

**My exported PNG has a white background, I need transparency.**
Tick **Transparent background** in the Export dialog (or use `--transparent`).

**The fonts in my figure are too small in the paper.**
Make the page the size the figure will have in print (**Page ▸ Page size ▸ Journal, 1 column**),
then the text sizes in CactusPlot match the paper. The **Journal (single column)** style uses larger
text for figures that will be shrunk.

## Data

**Do I need to say whether my file has error bars?**
No. As in Grace, the file decides: a GROMACS `@TYPE xydy` line gives error bars, and so does a
header that names a column as an error (`# time counts error`). Everything else opens as ordinary
curves, one per column, so time series never get error bars by accident. See
[How columns are used](guide/data.md#how-columns-are-used).

**A file does not open.**
CactusPlot shows a message saying why: the file could not be read, or it contains no numbers. If a
file opens without any message but looks wrong, please report it with the file.

**My file is not read correctly.**
Open it with **File ▸ Import Data**: the dialog shows how many blocks, rows and columns were found.
The common causes are:

- a decimal comma (`1,5` instead of `1.5`), which is read as two columns;
- text in the middle of the data;
- dates in an unexpected order; start with `-datehint european` for day-first dates.

**Can I read Excel files?**
Not directly. Copy the cells in Excel or LibreOffice and use **Edit ▸ Import from Clipboard**, or save
as CSV.

**Can I read NetCDF or HDF5?**
Not yet. Export the variables to text first, for example with `ncdump`, Python or GROMACS tools.

## Scripts

**My batch script runs but writes no file.**
Scripts may not write files unless you pass `-nosafe`, for safety. CactusPlot prints a message when
a write is refused.

**A command in my script does nothing.**
Commands that are not understood are skipped with a warning on standard error
(`Warning: unsupported command ignored: …`). Check the spelling against the
[Command language](reference/command-language.md).

## Installation and start-up

**Which version do I have?**
Run `cactusplot -version`, or open **Help ▸ About**. Both show the version, the commit and the
build number, e.g. `CactusPlot 0.9.0 (commit d6381e8, build 15)`. Include this line in bug reports.

**I installed a newer `.deb`, but nothing changed.**
Early packages were all called version 0.9.0, so the package manager could keep the copy already
installed. Packages now carry the build number (for example `0.9.0-15`) and replace older ones. If
in doubt, reinstall: `sudo apt install --reinstall ./cactusplot_*.deb`, then check
`cactusplot -version`.

**Windows says "Windows protected your PC".**
The installer is not signed with a paid certificate. Click **More info ▸ Run anyway**.

**macOS says the app "cannot be opened because the developer cannot be verified".**
Right-click the app ▸ **Open** ▸ **Open**, once. See [Installation](install.md#macos-11-big-sur-and-later).

**The AppImage does not start.**
Install FUSE 2 (`sudo apt install libfuse2t64` on Ubuntu 24.04 and Mint 22, `libfuse2` on older
releases), or run it with `--appimage-extract-and-run`. Start it from a terminal to see error
messages.

**The window is empty or garbled on Linux.**
Start it from a terminal with `QT_QPA_PLATFORM=xcb cactusplot`, which forces X11 instead of
Wayland, and report the problem with the terminal output.

**The text in the program is too small or too large.**
CactusPlot follows your desktop's scaling. On Linux you can override it with
`QT_SCALE_FACTOR=1.25 cactusplot`.

## Reporting problems

Please open an issue at <https://github.com/StochasticCactus/CactusGui/issues> with:

- what you did and what happened;
- the output of `cactusplot -version`;
- your operating system;
- if possible, the file that shows the problem.
