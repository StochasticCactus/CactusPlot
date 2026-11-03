# Data analysis

CactusPlot has all of Grace's data transformations, plus curve fitting with a live preview. Every
operation can be undone. In the dialogs, results go to a new set, so the original stays. Each
operation also has a command, so it can be scripted (see the
[Command language](../reference/command-language.md)).

## The Transformations window

**Data ▸ Transformations…** (**Ctrl+T**). Choose an operation on the left, the **Source set** at the
top, set the options and click **Apply**. A log at the bottom reports what was done.

- Operations that combine two sets (correlation, convolution, digital filter, interpolation at the
  points of another set) also ask for a **Second set**.
- **Replace the source set instead of creating a new set** overwrites the source. It can still be
  undone.

| Operation | Options | Command |
|---|---|---|
| **Evaluate expression** | Formulas for `x` and `y` (see [below](#evaluating-expressions)) | `s0.y = …` |
| **Histogram** | Number of bins, automatic or fixed range, cumulative, normalised (probability density), points at bin centres, draw as bars | `histogram(s0, mesh(0, 1, 21), off, off)`; the flags are cumulative and normalise |
| **Fourier transform** | Output: magnitude, phase, power, real part, imaginary part, power in dB. Window: none, triangular, Hanning, Welch, Hamming, Blackman, Parzen. X axis: index, frequency or period. Inverse transform. Complex input (x = real, y = imaginary). | `fft(s0, magnitude)`, `invfft(s0)` |
| **Running statistics** | Average, median, minimum, maximum or standard deviation over a window of n points | `runavg(s0, 5)`, `runmed`, `runmin`, `runmax`, `runstd` |
| **Differentiate** | Forward, backward or centred differences | `difference(s0, centered)` |
| **Integrate** | Cumulative integral (trapezoid rule). The total is printed in the log. | `integrate(s0)` |
| **Interpolation / splines** | Linear, cubic spline or Akima spline; a number of evenly spaced points, or the x values of a second set | `interpolate(s0, mesh(0, 10, 200), spline)` |
| **Regression** | Linear, polynomial (degree 1–10), exponential `y = A e^(Bx)`, power `y = A x^B`, logarithmic `y = A + B ln x`, inverse `y = 1/(A + Bx)`, saturation `y = A(1 − e^(−Bx))`. Output: the fitted curve, fitted values at the data x, or residuals. The coefficients and R² are printed. | `regress(s0, 2)` |
| **Correlation** | Cross-correlation of two sets (auto-correlation if both are the same) up to a maximum lag; covariance instead of correlation | `xcor(s0, s1, 50, off)` |
| **Convolution** | Linear convolution of two sets | `linconv(s0, s1)` |
| **Digital filter** | Filters the source with the y values of the second set as weights | — |
| **Geometric transform** | Rotate by an angle about a centre, scale and translate | `s0.x = …; s0.y = …` |
| **Sample points** | Keep every n-th point from a start index | — |
| **Prune** | Thin out dense data: drop points closer than a tolerance (interpolation, circle, ellipse or rectangle method; absolute or relative tolerances) | — |
| **Seasonal differences** | y[i] − y[i − period] | `seasonaldiff(s0, 12)` |
| **Cumulative sum** | Running total of y | `s1.y = rsum(s0.y)` |
| **Sort** | By X or by Y, ascending or descending (in place) | `sort s0 y descending` |
| **Reverse** | Reverse the order of points (in place) | `reverse s0` |
| **Restrict** | Keep the points where a condition is true, or inside or outside a region (in place) | `restrict(s0, y > 0)`, `restrict(s0, r0)` |
| **Feature extraction** | For every set in the graph, compute one number and make a new set of (set index, value). Features: Y minimum, maximum, average, standard deviation and median; number of points; X of the maximum and of the minimum; integral; frequency and period; zero crossing; rise and fall time; slope and Y intercept; set length; half-maximum width; barycentre X and Y | — |
| **Statistics** | Prints, for X and Y: minimum, maximum, mean, standard deviation and median. For Y also the sum, and the trapezoid integral of the set | `avg(s0.y)`, `sd(s0.y)`, … |

## Evaluating expressions

**Data ▸ Evaluate Expression / New Set…** (**Ctrl+R**) opens the Transformations window on
**Evaluate expression**. Write formulas for `x` and `y`:

- `x` and `y` are the columns of the source set; `s1.y` or `g1.s0.x` reach other sets;
- `y1` … `y4` are the extra columns (errors and so on);
- `i` is the point index (0, 1, 2, …).

Examples:

| Goal | x = | y = |
|---|---|---|
| Normalise to the maximum | `x` | `y / max(y)` |
| Subtract a baseline | `x` | `y - avg(s1.y)` |
| Convert ps to ns | `x / 1000` | `y` |
| Log of the data | `x` | `log10(y)` |
| Ratio of two sets with the same x | `x` | `y / s1.y` |
| A new function | `mesh(0, 10, 500)` | `jv(0, x)` (Bessel J₀) |
| Synthetic noisy data | `mesh(0, 1, 100)` | `2*x + rnorm(0, 0.1)` |

Operations work on whole columns at once, as in a spreadsheet column formula. Comparisons give 1
or 0, so `y * (x > 5)` zeroes everything left of x = 5, and `x > 5 ? y : 0` does the same.
All functions are in [Functions](../reference/functions.md).

## Curve fitting

**Data ▸ Curve Fitting…** (**Ctrl+F**), or right-click a curve ▸ **Fit Curve…**.

1. Choose the **Data set**.
2. Choose a **Model**, or write your own formula in **y =** using `x` and up to ten parameters
   `a0` … `a9`. Built-in models:

    | Model | Formula |
    |---|---|
    | Linear | `a0 + a1*x` |
    | Quadratic | `a0 + a1*x + a2*x^2` |
    | Cubic | `a0 + a1*x + a2*x^2 + a3*x^3` |
    | Exponential decay | `a0*exp(-x/a1) + a2` |
    | Exponential growth | `a0*exp(a1*x)` |
    | Gaussian peak | `a0*exp(-(x-a1)^2/(2*a2^2)) + a3` |
    | Lorentzian peak | `a0*a2^2/((x-a1)^2 + a2^2) + a3` |
    | Voigt peak | `a0*voigt(a2, a3, x - a1) + a4` |
    | Logistic (sigmoid) | `a0/(1 + exp(-(x - a1)/a2)) + a3` |
    | Power law | `a0*x^a1` |
    | Sine wave | `a0*sin(a1*x + a2) + a3` |
    | Michaelis–Menten | `a0*x/(a1 + x)` |
    | Hill | `a0*x^a2/(a1^a2 + x^a2)` |
    | Arrhenius | `a0*exp(-a1/x)` |

3. Starting values are guessed from the data when you choose a model or a set. **Guess initial
   values** repeats this. In the parameter table you can type your own **Initial** values, tick
   **Fixed** to keep a parameter constant, and give **Min** and **Max** bounds.
4. **Weights:** none; 1/y; 1/y²; or 1/dy², which uses the error column of XYDY sets, so points
   with small error bars count more.
5. Click **Fit**. The preview shows the fitted curve over the data. The table shows the
   **Result** and **± Std. error** for each parameter, and the report gives:
   - whether the fit converged, and after how many iterations;
   - the number of points and degrees of freedom;
   - χ², reduced χ², RMS residual, R² and the correlation coefficient.
6. **Create result set** adds a **smooth fitted curve**, the **fitted values at data x**, or the
   **residuals** to the graph. Choose which with **Result set**.

The fitter uses the Levenberg–Marquardt method with bounds. Its results are checked against the
certified values of the NIST statistical reference data sets.

In scripts:

```
fit formula "y = a0*exp(-a1*x)*cos(a2*x)"
fit with 3 parameters
a0 = 1; a1 = 0.2; a2 = 1.9
a1 constraints on; a1min = 0; a1max = 5
nonlfit(s1, 200)          # fit set 1 with at most 200 iterations; the curve becomes a new set
```

The report is printed in batch mode, and in the Commands window in the GUI.

## Editing sets

| Task | How |
|---|---|
| Sort by x or y | **Transformations ▸ Sort**, or `sort s0 x`, `sort s0 y descending` |
| Reverse the order | `reverse s0` |
| Join two sets into one | `join s0, s1` (s1 is appended to s0 and removed) |
| Split a set into pieces of n points | `split s0 100` |
| Delete points by index | `drop s0 10 20` deletes points 10 to 20; or **Edit points** tool, Ctrl+click |
| Copy, move or swap sets | `copy s0 to s1`, `move g0.s1 to g1.s0`, `swap s0 and s1` |
| Delete a set | `kill s3`, or right-click ▸ **Delete set** |
| Write a set to a text file | `write s0 file "out.dat"` (needs `-nosafe` in scripts) |

## Saving results

When CactusPlot runs from the command line, `-results results.txt` writes everything the
transformations and fits print (reports, integrals, coefficients) to a file.
