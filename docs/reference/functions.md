# Functions in expressions

Expressions appear in many places: **Evaluate Expression**, the **Restrict** condition, fit
formulas, tick label transforms, the Commands window and scripts. They work on whole columns at
once: `sin(x)` gives the sine of every x value. A number combined with a column applies to every
element.

## Names

| Name | Meaning |
|---|---|
| `x`, `y` | Columns of the current set (the set being changed or evaluated). |
| `y1`, `y2`, `y3`, `y4` | Further columns: the third, fourth, … column (for example the error of an XYDY set is `y1`). |
| `s1.x`, `s1.y`, `g1.s0.y` | Columns of other sets. `s1` means set 1 of the current graph. |
| `s_.y` | The set created last. |
| `i` | The point index: 0, 1, 2, … |
| `a0` … `a9` | Fit parameters (they keep their fitted values after a fit). |
| `pi` | π |
| `$t` | The tick value, in tick label formulas only. |
| any other name | A variable you assigned: `tau = 2.5`. |
| `s0.y[3]` | One element (counting from 0). |

## Operators

| Operator | Meaning |
|---|---|
| `+ - * /` | arithmetic |
| `^` | power: `x^2` |
| `%` | remainder |
| `== != < <= > >=` | comparisons, giving 1 (true) or 0 (false); also written `eq ne lt le gt ge` |
| `&&`, `!` | and, not; also written `and`, `not` (for *or*, see below the table) |
| `c ? a : b` | `a` where `c` is true, `b` elsewhere |
| `( )` | grouping |

*Or* is written `||` or `or`. Numbers can be written `1.5`, `2e-3` or `.5`.

## Making columns

| Function | Result |
|---|---|
| `mesh(a, b, n)` | n evenly spaced values from a to b |
| `mesh(n)` | 0, 1, …, n − 1 |
| `rand`, `rand(n)` | a uniform random number in [0, 1); or n of them |
| `rnorm(mean, sd)` | normally distributed random numbers (one per element when used with columns) |
| `irand(n)` | random integers 0 … n − 1 |
| `rsum(v)` | running sum (cumulative sum) |

Random numbers are reproducible with `-seed n` on the command line.

## Whole-column functions

These return a single number:

| Function | Result |
|---|---|
| `min(v)`, `max(v)` | smallest, largest value |
| `imin(v)`, `imax(v)` | index of the smallest, largest value |
| `sum(v)` | sum |
| `avg(v)`, `mean(v)` | mean |
| `sd(v)` | standard deviation (n − 1) |
| `len(v)`, `length(v)` | number of values |
| `int(x, y)` | integral of y over x (trapezoid rule) |

`min(a, b)` and `max(a, b)` with two arguments compare element by element.

## Elementary functions

`abs`, `sqrt`, `cbrt`, `sqr` (square), `exp`, `ln` and `log` (natural logarithm), `log10`, `log2`,
`pow(a, b)`, `hypot(a, b)`,
`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2(y, x)`,
`sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`,
`floor`, `ceil`, `round`, `rint`, `trunc`, `int(x)` (truncate), `mod(a, b)`,
`sgn`/`sign`, `fac` (factorial), `deg` (radians to degrees), `rad` (degrees to radians),
`minof(a, b)`, `maxof(a, b)`, `heaviside(x)`, `isnan(x)`.

## Peak shapes

| Function | Result |
|---|---|
| `gauss(x, x0, sigma)` | normalised Gaussian centred at x0 |
| `lorentz(x, x0, gamma)` | normalised Lorentzian centred at x0, half-width γ |
| `voigt(gamma, sigma, x)` | normalised Voigt profile: a Lorentzian of half-width γ convolved with a Gaussian of width σ |

## Special functions

| Function | Meaning |
|---|---|
| `gamma(x)`, `lgamma(x)`, `rgamma(x)` | Γ(x), ln Γ(x), 1/Γ(x) |
| `psi(x)` | digamma |
| `beta(a, b)`, `lbeta(a, b)` | B(a, b), ln B(a, b) |
| `igam(a, x)`, `igamc(a, x)`, `igami(a, p)` | regularised incomplete gamma P and Q, inverse of Q |
| `incbet(a, b, x)`, `incbi(a, b, y)` | regularised incomplete beta, and its inverse |
| `erf(x)`, `erfc(x)` | error function and complement |
| `dawsn(x)` | Dawson's integral |
| `expn(n, x)` | exponential integral Eₙ |
| `ci(x)`, `si(x)`, `chi(x)`, `shi(x)` | cosine, sine, hyperbolic cosine and sine integrals |
| `fresnlc(x)`, `fresnls(x)` | Fresnel integrals C and S |
| `j0`, `j1`, `jn(n, x)`, `jv(v, x)` | Bessel functions of the first kind |
| `y0`, `y1`, `yn(n, x)`, `yv(v, x)` | Bessel functions of the second kind |
| `iv(v, x)`, `kv(v, x)`, `kn(n, x)` | modified Bessel functions I and K |
| `i0e`, `i1e`, `k0e`, `k1e` | exponentially scaled modified Bessel functions |
| `ai(x)`, `bi(x)` | Airy functions |
| `struve(v, x)` | Struve function |
| `ellpk(m)`, `ellpe(m)` | complete elliptic integrals K and E (parameter m) |
| `ellik(phi, m)`, `ellie(phi, m)` | incomplete elliptic integrals F and E |
| `hyperg(a, b, x)` | confluent hypergeometric ₁F₁ |
| `hyp2f1(a, b, c, x)` | Gauss hypergeometric ₂F₁ |
| `zeta(x, q)`, `zetac(x)` | Hurwitz zeta, and Riemann ζ(x) − 1 |
| `spence(x)` | dilogarithm |

The values are checked against SciPy to about 12 digits in the test suite.

## Probability distributions

| Function | Meaning |
|---|---|
| `norm(x)` | standard normal density |
| `ndtr(x)`, `ndtri(p)` | standard normal cumulative distribution, and its inverse |
| `chdtr(k, x)`, `chdtrc(k, x)`, `chdtri(k, p)` | χ² distribution: cumulative, complement, inverse of the complement |
| `fdtr(a, b, x)`, `fdtrc(a, b, x)`, `fdtri(a, b, p)` | F distribution |
| `gdtr(a, b, x)`, `gdtrc(a, b, x)` | gamma distribution |
| `pdtr(k, m)`, `pdtrc(k, m)`, `pdtri(k, p)` | Poisson distribution |
| `stdtr(k, t)`, `stdtri(k, p)` | Student's t distribution |

These follow the Cephes conventions that Grace also uses.

## Examples

```
y = 2*exp(-((x - 5)/1.2)^2)                      # a Gaussian peak
y = y / max(y)                                   # normalise
y = x > 0 ? log10(x) : 0                         # log where defined
y = (y - avg(y)) / sd(y)                         # standardise
y = jv(0, x)                                     # Bessel J0
y1 = sqrt(abs(y))                                # Poisson errors into the error column
s2.y = s0.y - s1.y                               # difference of two sets with the same x
```
