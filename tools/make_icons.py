#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Regenerate the application icons from resources/cactusplot.svg.
Requires: pip install cairosvg pillow"""
import io, os
import cairosvg
from PIL import Image

root = os.path.join(os.path.dirname(__file__), "..")
svg = open(os.path.join(root, "resources", "cactusplot.svg"), "rb").read()

def render(size):
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg, output_width=size, output_height=size))).convert("RGBA")

out = os.path.join(root, "packaging", "icons")
for s in (16, 24, 32, 48, 64, 128, 256, 512, 1024):
    render(s).save(os.path.join(out, f"cactusplot-{s}.png"))
render(256).save(os.path.join(root, "packaging", "windows", "cactusplot.ico"),
                 sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
render(1024).save(os.path.join(root, "packaging", "macos", "CactusPlot.icns"))
print("icons written")
