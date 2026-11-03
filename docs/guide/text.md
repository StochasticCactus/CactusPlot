# Text and math

All text in a plot can contain formatting: titles, axis labels, legend entries, annotations and tick
label prefixes and suffixes. There are two ways to write it, and they can be mixed:

- **`$TeX$` math**, which is quick to write and familiar from LaTeX;
- **Grace escapes**, which give exact control and are what Grace projects contain.

## TeX math

Put math between dollar signs. Letters inside are italic, and digits and operators stay upright.

| You type | You get |
|---|---|
| `Time $t$ (s)` | Time *t* (s) |
| `$E = mc^2$` | *E* = *mc*² |
| `$x_i^{2}$`, `$x_{\mathrm{max}}$` | subscripts and superscripts; `\mathrm{}` keeps text upright |
| `$\alpha$, $\beta$, $\Delta E$, $\omega_0$` | Greek letters, lower and upper case |
| `Energy (kJ mol$^{-1}$)` | Energy (kJ mol⁻¹) |
| `$10^{-3}$` | 10⁻³ |
| `$\sqrt{x}$` | √x with a bar over x |
| `$\frac{a}{b}$` | a/b (written on one line) |
| `$\pm$ $\times$ $\cdot$ $\approx$ $\leq$ $\geq$ $\neq$ $\infty$ $\partial$ $\nabla$` | ± × · ≈ ≤ ≥ ≠ ∞ ∂ ∇ |
| `$\rightarrow$ $\leftrightarrow$ $\rightleftharpoons$` | → ↔ ⇌ |
| `$\AA$`, `$\degree$`, `$\hbar$`, `$\ell$`, `$\micro$` | Å ° ℏ ℓ µ |
| `$\sin x$`, `$\ln(k)$`, `$\exp$`, `$\max$` | function names stay upright |
| `$\mathit{text}$`, `$\text{text}$` | italic or upright text inside math |
| `\$` | a literal dollar sign |

Greek letters (`\alpha` … `\omega`, `\Gamma` … `\Omega`, and the `\var…` forms), relations,
arrows, set symbols, `\sum`, `\prod`, `\int` and `\langle`, `\rangle` are supported.
`\left`, `\right` and `\displaystyle` are accepted and ignored. This is a practical subset of TeX,
not a full TeX engine: there are no stacked fractions or matrices.

## Grace escapes

Grace text uses backslash codes. They work everywhere, and Grace projects use them.

| Escape | Effect |
|---|---|
| `\S` … `\N` | superscript … back to normal |
| `\s` … `\N` | subscript … back to normal |
| `\+`, `\-` | larger, smaller |
| `\z{1.5}`, `\Z{2}` | scale by a factor; set an absolute size |
| `\f{Times-Italic}`, `\f{2}`, `\f{}` | change font (by name or font number); `\f{}` restores |
| `\x` | Symbol font: `\xa` is α, `\xD` is Δ, `\xm` is µ |
| `\R{red}`, `\R{2}`, `\R{}` | text colour by name, Grace colour number, or back to normal |
| `\u` … `\U` | underline on … off |
| `\o` … `\O` | overline on … off |
| `\c` … `\C` | capitals on … off |
| `\q` … `\Q` | slant on … off |
| `\l{0.3}` | slant by a factor |
| `\v{0.5}`, `\v{}` | move up (negative: down); `\v{}` returns to the baseline |
| `\h{0.5}` | move right (negative: left) |
| `\t{a b c d}`, `\t{}` | transform glyphs with a 2×2 matrix; `\t{}` resets |
| `\r{30}` | rotate following text by 30° |
| `\m{1}` … `\M{1}` | remember a position, return to it (for stacking super- and subscripts) |
| `\n` | new line |
| `\#{41}` | character by hexadecimal code (`41` is A) |
| `\\` | a backslash |

Example: `Al\s2\NO\s3\N` gives Al₂O₃, and `\xD\f{}E = 5 kJ` gives ΔE = 5 kJ.

## Fonts

The 14 standard PostScript fonts that Grace uses are always available:

- Times-Roman, Times-Italic, Times-Bold, Times-BoldItalic;
- Helvetica, Helvetica-Oblique, Helvetica-Bold, Helvetica-BoldOblique;
- Courier, Courier-Oblique, Courier-Bold, Courier-BoldOblique;
- Symbol and ZapfDingbats.

They are drawn with the bundled TeX Gyre fonts (Termes for Times, Heros for Helvetica, Cursor for
Courier), which have the same letter widths. A figure therefore looks the same on Windows, macOS
and Linux, and the same as in Grace. Symbol and ZapfDingbats characters are mapped to the matching
Unicode characters.

Any font installed on your computer can be used too: type or choose its name in a **Font** box,
or write `\f{Fira Sans}`. If you share the project, other computers need that font as well; the
standard fonts are the safe choice.

## Unicode

You can type any Unicode character directly: `Å`, `µ`, `°`, `→`, `²`, Chinese, Japanese or
Cyrillic text. Characters missing from the current font are taken from another installed font.
