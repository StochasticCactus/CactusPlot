# Building, architecture and contributing

## Building

### Requirements

- CMake 3.19 or newer and Ninja (or make);
- a C++20 compiler: GCC 11+, Clang 14+, Apple Clang 15+ or Visual Studio 2022;
- Qt 6.4 or newer, with the modules Core, Gui, Widgets, Svg and PrintSupport.

Nothing else: no Boost, no QML, no QtCharts. Fonts and colour tables are in the repository.

### Linux

```bash
# Ubuntu 24.04, Debian 12, Mint 22, Pop!_OS 24.04
sudo apt install build-essential cmake ninja-build git qt6-base-dev qt6-svg-dev libgl1-mesa-dev
```
```bash
# Fedora
sudo dnf install gcc-c++ cmake ninja-build git qt6-qtbase-devel qt6-qtsvg-devel
```
```bash
# Arch, Manjaro
sudo pacman -S base-devel cmake ninja git qt6-base qt6-svg
```

Then:

```bash
git clone https://github.com/StochasticCactus/CactusGui.git
```
```bash
cd CactusGui && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```
```bash
cmake --build build
```
```bash
./build/src/cactusplot
```

`sudo cmake --install build` installs it with its desktop file, icons, MIME types, man page and
the `cactusbat` link.

### macOS

```bash
brew install cmake ninja qt
```
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
```
```bash
cmake --build build && open build/src/CactusPlot.app
```

`cmake --install build --prefix dist` runs `macdeployqt` and produces a self-contained `CactusPlot.app`;
`cpack` makes a `.dmg`.

### Windows

Install Visual Studio 2022 (with "Desktop development with C++"), CMake, and Qt 6 for MSVC (from the
Qt online installer). In a *Developer PowerShell for VS 2022*:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build
cmake --install build --prefix dist      # runs windeployqt
```

### Options

| Option | Default | Effect |
|---|---|---|
| `CACTUS_BUILD_GUI` | ON | OFF builds a batch-only `cactusplot` (no Qt Widgets needed). |
| `CACTUS_BUILD_TESTS` | ON | Build the test programs. |
| `CACTUS_DEPLOY_QT` | ON on Windows and macOS | Copy the Qt libraries next to the program at install time. |

### Tests

```bash
ctest --test-dir build --output-on-failure
```

On a machine without a display, set `QT_QPA_PLATFORM=offscreen`. The tests are:

- `test_math`: expressions, special functions (checked against SciPy), FFT, interpolation,
  fitting (NIST StRD reference data), regressions, tick generation and label formats;
- `test_grace`: Grace project reading and writing, the interpreter, data files (including GROMACS
  `.xvg`), exports, the command line and colours;
- `test_gui`: drives the real window, covering wheel zoom, undo, selecting curves, the Commands
  window, dialogs and opening files.

`build/tests/cactus-gallery` renders a gallery of plot types and styles, which is useful for checking
visual changes.

### Packages

| Package | How |
|---|---|
| `.deb`, `.rpm`, `.tar.gz` | `cd build && cpack -G DEB` (or `RPM`, `TGZ`) |
| AppImage | See the `appimage` job in `.github/workflows/build.yml` (linuxdeploy with the Qt plugin). |
| Windows installer and zip | `cpack -G NSIS` and `cpack -G ZIP` (needs NSIS). |
| macOS `.dmg` | `cpack -G DragNDrop` |
| Arch | `packaging/arch/PKGBUILD` |

#### Flatpak

```bash
flatpak install flathub org.kde.Platform//6.8 org.kde.Sdk//6.8
```
```bash
flatpak-builder --user --install --force-clean build-flatpak packaging/flatpak/org.cactusplot.CactusPlot.yml
```
```bash
flatpak run org.cactusplot.CactusPlot
```

## Continuous integration and releases

`.github/workflows/build.yml` builds and tests on every push:

- Ubuntu 24.04 with the system Qt, building the `.deb` and `.rpm` packages;
- the AppImage, built on Ubuntu 22.04 for older distributions;
- macOS, a universal (arm64 + x86_64) `.dmg`;
- Windows, the installer and a portable zip.

The built packages are attached to each run as artifacts. Pushing a tag such as `v0.9.0` also
creates a GitHub release with all packages.

`.github/workflows/docs.yml` checks that this documentation builds. On the `main` branch of a public
repository it also publishes the documentation as a website with GitHub Pages. To enable that,
choose **Settings ▸ Pages ▸ Source: GitHub Actions** in the repository once.

## Architecture

```
src/
  core/      the data model: Project, Graph, Axis, DataSet, …; style presets; colour library
  math/      expression engine, special functions, transformations, fitting
  plot/      scales, tick placement and label formats, fonts, rich text ($TeX$ and Grace escapes)
  render/    PlotRenderer (draws a Project with QPainter), PostScript/EPS engine, export
  io/        text data reader
  grace/     interpreter for the Grace command language, .agr/.cplot reading and writing
  app/       main(), command-line processing (gracebat-compatible)
  ui/        main window, plot view, Appearance window, dialogs, set editor, console, themes
resources/   fonts (TeX Gyre), icons, application icon
packaging/   desktop files, icons, installers, Flatpak, PKGBUILD
tools/       scripts that generate the colour tables and icons
```

The main design choices:

- **The model is plain data.** `Project` and everything in it are copyable value types (Qt's
  implicitly shared containers make copies cheap). The GUI's undo stack stores snapshots of the
  project, so every change, by mouse, dialog or script, is undoable without special code.
- **One renderer for everything.** The screen, PDF, SVG, PNG and printing all draw with
  `PlotRenderer` through `QPainter`. PostScript and EPS use a small `QPaintEngine` of our own that
  writes PostScript. What you see is what you export.
- **One language for everything.** Project files, batch files, the Commands window and the GUI's
  analysis dialogs all go through the same `Interpreter`. A `.cplot` file is a script that rebuilds
  the project.
- **Grace units.** Sizes are stored as Grace does: positions in viewport units (the short page side is
  1), text and symbol sizes and line widths relative to the page. This keeps `.agr` files exact and
  lets figures scale with the page.
- **No platform code.** Everything is Qt, so the same code runs on Windows, macOS and Linux (X11 and
  Wayland).

## Contributing

Bug reports, ideas and pull requests are welcome at
<https://github.com/StochasticCactus/CactusGui>.

- Keep changes focused, and add a test when you fix a bug or add a feature (the existing tests show
  how to drive the interpreter, the renderer and the GUI).
- Run `ctest` before sending a pull request; CI runs the same tests on all three systems.
- Code style: C++20, four-space indentation, the existing naming (`m_member`, `camelCase` functions),
  and `clang-format` is not enforced. Prefer clear code to clever code.
- New source files start with `// SPDX-License-Identifier: GPL-2.0-or-later`. By contributing you
  agree that your work is distributed under the GPL version 2 or later.
- Text the user sees should be short, plain and in British English, like the rest of the program.
