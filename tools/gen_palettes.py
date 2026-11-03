#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate src/core/color/palettes_data.inc.

The generated tables are committed to the repository, so this script is only
needed when the palette collection changes. It requires:

    pip install matplotlib cmocean cmcrameri colorcet

Sources and licences (see THIRD_PARTY.md):
  * matplotlib colormaps / CSS4 / xkcd / Tableau colours  (matplotlib licence, xkcd: CC0)
  * ColorBrewer palettes via matplotlib                    (Apache-2.0)
  * cmocean                                                (MIT)
  * Crameri Scientific Colour Maps via cmcrameri          (MIT)
  * colorcet / CET perceptually uniform maps & Glasbey    (CC-BY 4.0)
  * X11 rgb.txt                                            (X11/MIT)
  * Okabe-Ito, Paul Tol, IBM, Solarized, Nord             (published colour values)
"""
import os
import sys

import matplotlib
import matplotlib.colors as mcolors
import cmocean
import cmcrameri.cm as cmc
import colorcet as cc

SAMPLES = 128
OUT = os.path.join(os.path.dirname(__file__), "..", "src", "core", "color", "palettes_data.inc")


def rgb_hex(c):
    r, g, b = [max(0, min(255, int(round(v * 255)))) for v in mcolors.to_rgb(c)]
    return (r << 16) | (g << 8) | b


GRACE16 = [
    ("white", 0xFFFFFF), ("black", 0x000000), ("red", 0xFF0000), ("green", 0x00FF00),
    ("blue", 0x0000FF), ("yellow", 0xFFFF00), ("brown", 0xBC8F8F), ("grey", 0xDCDCDC),
    ("violet", 0x9400D3), ("cyan", 0x00FFFF), ("magenta", 0xFF00FF), ("orange", 0xFFA500),
    ("indigo", 0x7221BC), ("maroon", 0x670748), ("turquoise", 0x40E0D0), ("green4", 0x008B00),
]

named = []  # (name, rgb, source)
seen = set()


def add_named(name, value, source):
    key = (name.lower(), source)
    if key in seen:
        return
    seen.add(key)
    named.append((name, value, source))


for n, v in GRACE16:
    add_named(n, v, "Grace")
for n, v in sorted(mcolors.CSS4_COLORS.items()):
    add_named(n, rgb_hex(v), "CSS")
for n, v in mcolors.TABLEAU_COLORS.items():
    add_named(n, rgb_hex(v), "Tableau")
rgbtxt = "/usr/share/X11/rgb.txt"
if os.path.exists(rgbtxt):
    for line in open(rgbtxt):
        if line.startswith("!") or not line.strip():
            continue
        parts = line.split()
        r, g, b = int(parts[0]), int(parts[1]), int(parts[2])
        name = " ".join(parts[3:])
        if " " in name:
            continue  # keep the CamelCase/lower-case single-word variant
        add_named(name, (r << 16) | (g << 8) | b, "X11")
for n, v in sorted(mcolors.XKCD_COLORS.items()):
    add_named(n.replace("xkcd:", ""), rgb_hex(v), "xkcd")

# ---------------------------------------------------------------- qualitative
qual = []  # (name, group, [rgb...])


def hexlist(s):
    return [int(h.strip().lstrip("#"), 16) for h in s.split()]


qual.append(("CactusPlot Modern", "CactusPlot", hexlist(
    "#0072B2 #D55E00 #009E73 #CC79A7 #E69F00 #56B4E9 #8C564B #F0E442 #7F7F7F #000000")))
qual.append(("CactusPlot Dark", "CactusPlot", hexlist(
    "#56B4E9 #FF9F43 #2ED8A3 #FF6B9A #F9D65C #B39DFF #FF6F5E #9AD0F5 #C9C9C9")))
qual.append(("Okabe-Ito", "Colour-blind safe", hexlist(
    "#E69F00 #56B4E9 #009E73 #F0E442 #0072B2 #D55E00 #CC79A7 #000000")))
qual.append(("Tol Bright", "Colour-blind safe", hexlist("#4477AA #EE6677 #228833 #CCBB44 #66CCEE #AA3377 #BBBBBB")))
qual.append(("Tol Vibrant", "Colour-blind safe", hexlist("#EE7733 #0077BB #33BBEE #EE3377 #CC3311 #009988 #BBBBBB")))
qual.append(("Tol Muted", "Colour-blind safe", hexlist(
    "#CC6677 #332288 #DDCC77 #117733 #88CCEE #882255 #44AA99 #999933 #AA4499 #DDDDDD")))
qual.append(("Tol Light", "Colour-blind safe", hexlist(
    "#77AADD #EE8866 #EEDD88 #FFAABB #99DDFF #44BB99 #BBCC33 #AAAA00 #DDDDDD")))
qual.append(("Tol High Contrast", "Colour-blind safe", hexlist("#004488 #DDAA33 #BB5566 #000000")))
qual.append(("IBM", "Colour-blind safe", hexlist("#648FFF #785EF0 #DC267F #FE6100 #FFB000")))
qual.append(("Grace", "Classic", [v for _, v in GRACE16[1:]]))
for name in ["tab10", "tab20", "tab20b", "tab20c"]:
    cm = matplotlib.colormaps[name]
    qual.append(("Tableau " + name[3:], "Tableau / D3", [rgb_hex(c) for c in cm.colors]))
for name in ["Set1", "Set2", "Set3", "Pastel1", "Pastel2", "Paired", "Accent", "Dark2"]:
    cm = matplotlib.colormaps[name]
    qual.append((name, "ColorBrewer", [rgb_hex(c) for c in cm.colors]))
qual.append(("Solarized", "Themes", hexlist("#B58900 #CB4B16 #DC322F #D33682 #6C71C4 #268BD2 #2AA198 #859900")))
qual.append(("Nord Aurora", "Themes", hexlist("#BF616A #D08770 #EBCB8B #A3BE8C #B48EAD #5E81AC #88C0D0 #4C566A")))
qual.append(("Dracula", "Themes", hexlist("#FF5555 #FFB86C #F1FA8C #50FA7B #8BE9FD #BD93F9 #FF79C6 #6272A4")))
qual.append(("Gruvbox", "Themes", hexlist("#CC241D #98971A #D79921 #458588 #B16286 #689D6A #D65D0E #928374")))
for name in ["glasbey", "glasbey_light", "glasbey_dark", "glasbey_cool", "glasbey_warm"]:
    qual.append((name.replace("_", " ").title(), "Glasbey (colorcet)", [rgb_hex(c) for c in cc.palette[name]]))
for name in sorted(k for k in cmc.cmaps if k.endswith("S") and not k.endswith("_r")):
    cm = cmc.cmaps[name]
    cols = cm.colors if hasattr(cm, "colors") else [cm(i / 99) for i in range(100)]
    qual.append((name, "Crameri categorical", [rgb_hex(c) for c in cols][:100]))

# ---------------------------------------------------------------- colormaps
cmaps = []  # (name, category, source, [rgb * SAMPLES])


def sample(cm):
    return [rgb_hex(cm(i / (SAMPLES - 1))[:3]) for i in range(SAMPLES)]


mpl_groups = {
    "Perceptual": ["viridis", "plasma", "inferno", "magma", "cividis", "turbo"],
    "Sequential": ["Greys", "Purples", "Blues", "Greens", "Oranges", "Reds", "YlOrBr", "YlOrRd", "OrRd",
                   "PuRd", "RdPu", "BuPu", "GnBu", "PuBu", "YlGnBu", "PuBuGn", "BuGn", "YlGn",
                   "binary", "gray", "bone", "pink", "spring", "summer", "autumn", "winter", "cool",
                   "Wistia", "hot", "afmhot", "gist_heat", "copper"],
    "Diverging": ["PiYG", "PRGn", "BrBG", "PuOr", "RdGy", "RdBu", "RdYlBu", "RdYlGn", "Spectral",
                  "coolwarm", "bwr", "seismic"],
    "Cyclic": ["twilight", "twilight_shifted", "hsv"],
    "Miscellaneous": ["jet", "rainbow", "nipy_spectral", "gist_rainbow", "gist_earth", "terrain", "ocean",
                      "gist_stern", "gnuplot", "gnuplot2", "CMRmap", "cubehelix", "brg", "flag", "prism"],
}
for cat, names in mpl_groups.items():
    for n in names:
        src = "ColorBrewer" if n[0].isupper() and n not in ("CMRmap", "Wistia") else "matplotlib"
        cmaps.append((n, cat, src, sample(matplotlib.colormaps[n])))

cmo_div = {"balance", "delta", "curl", "diff", "tarn", "topo"}
for n in sorted(k for k in cmocean.cm.cmap_d if not k.endswith("_r") and not k.endswith("_i")):
    cat = "Cyclic" if n == "phase" else ("Diverging" if n in cmo_div else "Sequential")
    cmaps.append(("cmo." + n, cat, "cmocean", sample(cmocean.cm.cmap_d[n])))

for n in sorted(k for k in cmc.cmaps if not k.endswith("_r") and not k.endswith("S")):
    if n.endswith("O"):
        cat = "Cyclic"
    elif n in ("bam", "berlin", "broc", "cork", "lisbon", "roma", "tofino", "vik", "vanimo", "managua",
               "bukavu", "fes", "oleron"):
        cat = "Diverging"
    else:
        cat = "Sequential"
    cmaps.append((n, cat, "Crameri", sample(cmc.cmaps[n])))

cc_div = {"bkr", "bky", "bwy", "coolwarm", "cwr", "gwv", "bjy"}
cc_cyc = {"colorwheel", "cyclic_isoluminant"}
for n in sorted(cc.palette_n.keys()):
    if n.startswith("glasbey") or n == "gouldian" and False:
        continue
    cat = "Diverging" if n in cc_div else ("Cyclic" if n in cc_cyc else "Sequential")
    cmaps.append(("cet." + n, cat, "colorcet", sample(mcolors.LinearSegmentedColormap.from_list(n, cc.palette_n[n]))))

# ---------------------------------------------------------------- emit
def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


with open(OUT, "w") as f:
    f.write("// Generated by tools/gen_palettes.py -- do not edit by hand.\n")
    f.write("// SPDX-License-Identifier: GPL-2.0-or-later (data: see THIRD_PARTY.md)\n\n")
    f.write("static const NamedColorEntry kNamedColors[] = {\n")
    for n, v, s in named:
        f.write("    {%s, 0x%06X, %s},\n" % (cstr(n), v, cstr(s)))
    f.write("};\n\n")

    flat = []
    entries = []
    for n, g, cols in qual:
        entries.append((n, g, len(flat), len(cols)))
        flat.extend(cols)
    f.write("static const quint32 kPaletteColors[] = {\n")
    for i in range(0, len(flat), 8):
        f.write("    " + ", ".join("0x%06X" % c for c in flat[i:i + 8]) + ",\n")
    f.write("};\n\n")
    f.write("static const PaletteEntry kPalettes[] = {\n")
    for n, g, off, cnt in entries:
        f.write("    {%s, %s, %d, %d},\n" % (cstr(n), cstr(g), off, cnt))
    f.write("};\n\n")

    f.write("static const int kColormapSamples = %d;\n" % SAMPLES)
    f.write("static const quint32 kColormapData[] = {\n")
    for n, cat, src, cols in cmaps:
        f.write("    // %s\n" % n)
        for i in range(0, len(cols), 8):
            f.write("    " + ", ".join("0x%06X" % c for c in cols[i:i + 8]) + ",\n")
    f.write("};\n\n")
    f.write("static const ColormapEntry kColormaps[] = {\n")
    for i, (n, cat, src, cols) in enumerate(cmaps):
        f.write("    {%s, %s, %s, %d},\n" % (cstr(n), cstr(cat), cstr(src), i * SAMPLES))
    f.write("};\n")

print("named colours:", len(named), "palettes:", len(qual), "colormaps:", len(cmaps), file=sys.stderr)
