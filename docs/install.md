# Installation

CactusPlot is a single program, `cactusplot`. It needs no other software: on Windows and macOS, and
in the Linux AppImage, everything it needs is bundled. The `.deb` and `.rpm` packages use the Qt 6
libraries that your distribution provides.

Ready-made packages are on the [Releases page](https://github.com/StochasticCactus/CactusPlot/releases).

## Windows 10 and 11

- **Installer:** download `CactusPlot-<version>-win64.exe` and run it. It adds a Start menu entry
  and lets you open `.cplot`, `.agr` and `.xvg` files with CactusPlot.
- **Portable:** download the `.zip`, unpack it anywhere and run `bin\cactusplot.exe`.

Windows SmartScreen may warn about an "unrecognised app", because the program is not signed with a
paid certificate. Click **More info ▸ Run anyway**.

## macOS 11 (Big Sur) and later

### Homebrew

```bash
brew install --cask stochasticcactus/tap/cactusplot
```

This installs **CactusPlot** in Applications and the `cactusplot` and `cactusbat` commands, and
takes care of the first-launch warning described below. Update with `brew upgrade --cask cactusplot`.

### Disk image

Download `CactusPlot-macOS-universal.dmg`, open it and drag **CactusPlot** to **Applications**.
The same app runs natively on Apple silicon and Intel Macs. It is a normal macOS app:
**XQuartz is not needed.**

The app is not notarised by Apple, so the first launch is blocked. Either:

- right-click (or Control-click) CactusPlot in Applications and choose **Open**, then **Open** again; or
- run this once in Terminal:

```bash
xattr -dr com.apple.quarantine /Applications/CactusPlot.app
```

To use the command line on a Mac, call the program inside the bundle, for example:

```bash
/Applications/CactusPlot.app/Contents/MacOS/CactusPlot -hardcopy data.dat -o figure.pdf
```

## Linux

### AppImage (any distribution)

The AppImage runs on Ubuntu 22.04 and newer and on Linux Mint 21 and newer. It also runs on Debian
12, current Fedora, openSUSE, Arch, Pop!_OS and other distributions of the same age, on X11 and on
Wayland (GNOME, KDE Plasma, COSMIC, Cinnamon and others).

1. Download `CactusPlot-x86_64.AppImage`.
2. Make it executable: right-click ▸ **Properties** ▸ **Permissions** ▸ tick **Allow executing file
   as program**. In a terminal, run `chmod +x CactusPlot-x86_64.AppImage` instead.
3. Double-click it, or run `./CactusPlot-x86_64.AppImage`.

If nothing happens, your system lacks FUSE 2, which AppImages need. Install it:

```bash
sudo apt install libfuse2t64      # Ubuntu 24.04, Mint 22, Pop!_OS 24.04
```
```bash
sudo apt install libfuse2         # Ubuntu 22.04, Mint 21
```
```bash
sudo dnf install fuse-libs        # Fedora
```

You can also run it without FUSE: `./CactusPlot-x86_64.AppImage --appimage-extract-and-run`.

### Ubuntu, Debian, Linux Mint, Pop!_OS (apt)

The package works on Ubuntu 24.04, Linux Mint 22, Pop!_OS 24.04, Debian 13 and later. On older
releases, use the AppImage.

Add the CactusPlot package repository once:

```bash
sudo curl -fsSL -o /usr/share/keyrings/cactusplot.gpg https://stochasticcactus.github.io/CactusPlot/apt/cactusplot.gpg
```
```bash
sudo curl -fsSL -o /etc/apt/sources.list.d/cactusplot.sources https://stochasticcactus.github.io/CactusPlot/apt/cactusplot.sources
```

Then install it like any other package:

```bash
sudo apt update && sudo apt install cactusplot
```

New versions arrive with your normal system updates (`sudo apt upgrade`). This adds a menu entry,
opens `.cplot`, `.agr` and `.xvg` files by double-click, and installs two commands: `cactusplot` and
`cactusbat` (the batch version, like `gracebat`). Remove it with `sudo apt remove cactusplot`; to
remove the repository too, delete the two files downloaded above.

Without the repository, you can also download the `.deb` from the Releases page and install it with
`sudo apt install ./cactusplot_<version>_amd64.deb`. It is then not updated automatically.

### Fedora, openSUSE (`.rpm`)

```bash
sudo dnf install ./cactusplot-<version>.x86_64.rpm
```

### Arch Linux and Manjaro

A `PKGBUILD` is in `packaging/arch/`:

```bash
git clone https://github.com/StochasticCactus/CactusPlot.git
```
```bash
cd CactusPlot/packaging/arch && makepkg -si
```

### Flatpak

A Flatpak manifest is in `packaging/flatpak/`. It is not on Flathub yet. To build it yourself, see
[Development](development.md#flatpak).

## Building from source

If there is no package for your system, building takes a few minutes. See
[Development](development.md#building).

## Checking the installation

```bash
cactusplot -version
```

This prints the version, the Qt version, and the output formats your build can write.
