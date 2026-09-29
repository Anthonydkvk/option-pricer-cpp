# option-pricer-cpp

A small, tested option pricing library in modern C++ (C++17). It prices European and American vanilla options with three methods and computes their Greeks:

| Method | European | American | Greeks |
|---|:-:|:-:|---|
| Black-Scholes closed form | ✓ | – | analytic + finite differences |
| Cox-Ross-Rubinstein binomial tree | ✓ | ✓ (early exercise) | finite differences |
| Monte Carlo (exact GBM simulation) | ✓ | – | finite differences with common random numbers |

No dependencies besides the C++ standard library. GoogleTest is fetched automatically by CMake for the tests.

> **Work in progress.** This README is updated at every step. Current status:
>
> | Step | Content | Status |
> |---|---|---|
> | 0 | CMake project, GoogleTest | done |
> | 1 | Option model, Black-Scholes price and analytic Greeks | done |
> | 2 | CRR binomial tree (European and American) | done |
> | 3 | Monte Carlo (standard error, 95 % CI, antithetic variates) | done |
> | 4 | Generic finite-difference Greeks | done |
> | 5 | Command-line demo with timings | planned |
> | 6 | Python reference (NumPy/SciPy) and benchmark | planned |
> | 7 | CI (GCC and Clang) | planned |

## Build and test

Requirements: CMake ≥ 3.20 and a C++17 compiler (GCC or Clang).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Our own targets compile with `-Wall -Wextra -Wpedantic -Werror`.

## Usage

```cpp
#include "pricer/binomial.hpp"
#include "pricer/black_scholes.hpp"
#include "pricer/greeks.hpp"
#include "pricer/monte_carlo.hpp"

using namespace pricer;

const MarketParams market{/*S=*/100.0, /*r=*/0.05, /*q=*/0.0, /*sigma=*/0.2};
const Option call{OptionType::Call, ExerciseStyle::European, /*K=*/100.0, /*T=*/1.0};
const Option american_put{OptionType::Put, ExerciseStyle::American, 100.0, 1.0};

double bs   = black_scholes_price(call, market);                  // 10.450584
Greeks g    = black_scholes_greeks(call, market);                 // analytic
double crr  = binomial_price(american_put, market, 5000);         // 6.090219
auto   mc   = monte_carlo_price(call, market, 500'000, /*seed=*/42);  // mc.price, mc.std_error, mc.ci_low, mc.ci_high

// Greeks of any pricing method, by bump & reprice:
const PricingFunction tree = [](const Option& o, const MarketParams& m) {
    return binomial_price(o, m, 2001);
};
Greeks tree_greeks = finite_difference_greeks(tree, american_put, market, BumpSizes{2e-2});
```

Invalid inputs (S, K, σ, T ≤ 0, steps or paths ≤ 0, American option passed to Black-Scholes or Monte Carlo) throw `std::invalid_argument`.

## Models

**Black-Scholes** (continuous dividend yield $q$):

$$
d_1 = \frac{\ln(S/K) + (r - q + \tfrac{1}{2}\sigma^2)T}{\sigma\sqrt{T}}, \qquad d_2 = d_1 - \sigma\sqrt{T}
$$

$$
C = S e^{-qT} N(d_1) - K e^{-rT} N(d_2), \qquad P = K e^{-rT} N(-d_2) - S e^{-qT} N(-d_1)
$$

**CRR binomial tree**: $u = e^{\sigma\sqrt{\Delta t}}$, $d = 1/u$, $p = \dfrac{e^{(r-q)\Delta t} - d}{u - d}$.
Backward induction on a single vector (O(N²) time, O(N) memory). For American options each node takes $\max(\text{continuation}, \text{exercise})$.

**Monte Carlo**: exact simulation of the terminal spot, so there is no time-discretisation bias:

$$
S_T = S \exp\!\Big(\big(r - q - \tfrac{1}{2}\sigma^2\big)T + \sigma\sqrt{T}\,Z\Big), \qquad Z \sim \mathcal{N}(0, 1)
$$

The price is the discounted mean payoff. The standard error is $e^{-rT} s / \sqrt{N}$, and the 95 % confidence interval is price ± 1.96 standard errors. Antithetic variates pair each $Z$ with $-Z$; the standard error is computed on the pair averages, because the two halves of a pair are not independent.

**Finite-difference Greeks** (central differences, one generic function for every method):

$$
\Delta \approx \frac{P(S+h) - P(S-h)}{2h}, \qquad \Gamma \approx \frac{P(S+h) - 2P(S) + P(S-h)}{h^2}
$$

The same formula is used for Vega (bump σ), Rho (bump r) and Theta (bump T, with a minus sign: time passing shortens the maturity). Default bumps: $h_S = 10^{-4} S$, $h_\sigma = h_r = h_T = 10^{-4}$.

## Conventions

- Vega and Rho are per unit change (1.0 = 100 %) of σ and r. Divide by 100 to get the change per 1 %.
- Theta is per year of calendar time. Divide by 365 to get it per day.
- Monte Carlo uses `std::mt19937_64` with an explicit seed: the same seed always gives the same result.

## Results

Standard case: S = 100, K = 100, r = 5 %, q = 0, σ = 20 %, T = 1 year.
All numbers below come from a real run of this library (Apple Clang 17, Release build).

**European call**

| Method | Price | Δ | Γ | Vega | Θ (per year) | ρ |
|---|---:|---:|---:|---:|---:|---:|
| Black-Scholes, analytic | 10.4506 | 0.6368 | 0.0188 | 37.5240 | -6.4140 | 53.2325 |
| Black-Scholes, finite differences | 10.4506 | 0.6368 | 0.0188 | 37.5240 | -6.4140 | 53.2325 |
| CRR, 2001 steps (h_S = 2 % S) | 10.4515 | 0.6365 | 0.0185 | 37.5292 | -6.4144 | 53.2286 |
| Monte Carlo, 500 000 paths, seed 42 | 10.4744 | 0.6380 | 0.0188 | 37.6423 | -6.4303 | 53.3208 |

Monte Carlo call: standard error 0.0209, 95 % CI [10.4335, 10.5153], which contains the exact price.

**European put**

| Method | Price | Δ | Γ | Vega | Θ (per year) | ρ |
|---|---:|---:|---:|---:|---:|---:|
| Black-Scholes, analytic | 5.5735 | -0.3632 | 0.0188 | 37.5240 | -1.6579 | -41.8905 |
| CRR, 2001 steps (h_S = 2 % S) | 5.5744 | -0.3635 | 0.0185 | 37.5292 | -1.6582 | -41.8943 |
| Monte Carlo, 500 000 paths, seed 42 | 5.5566 | -0.3625 | 0.0188 | 37.4217 | -1.6521 | -41.8021 |

**American put (CRR)**

| Steps | Price | Δ | Γ | Vega | Θ (per year) | ρ |
|---|---:|---:|---:|---:|---:|---:|
| 2001 | 6.0911 | -0.4116 | 0.0229 | 37.4937 | -2.2380 | -30.2277 |
| 5000 | 6.0902 | | | | | |

The early-exercise premium is about 0.52 (6.0902 − 5.5735). The American call without dividends (10.4515 at 2001 steps) equals the European call: early exercise is never optimal.

## Things that went wrong, and why

**Tree Gamma with a tiny bump is meaningless.** With a fixed number of steps, the CRR price is a weighted sum of $\max(S u^k - K, 0)$ terms, so it is *piecewise linear* in S. Its true Gamma is 0 between nodes and infinite where a node crosses the strike. With S = K and an even number of steps, a node sits exactly on the strike, and the default bump $h_S = 10^{-4} S$ returns Γ ≈ 3.35 at 500 steps (the exact value is 0.0188). The fix is a bump larger than the spacing between kinks ($\approx 2 S\sigma\sqrt{\Delta t}$, about 0.9 at 2001 steps) and an odd number of steps. A test (`DefaultSpotBumpIsTooSmallForTreeGamma`) documents the problem.

**Monte Carlo Greeks need common random numbers.** Each Monte Carlo price has noise of about one standard error (≈ 0.02 here). Divided by $2h_S = 0.02$, independent noise in the bumped prices gives a useless Delta. Reusing the same seed for all nine repricings cancels most of it; the test suite checks both cases. Gamma stays noisy even then, because only the paths ending within ±h of the strike contribute.

## Project layout

```
include/pricer/   public headers (option, normal, black_scholes, binomial, monte_carlo, greeks)
src/              implementations
tests/            one GoogleTest file per module
apps/             command-line demo (step 5)
```

## Limitations and possible extensions

- American options by Monte Carlo (Longstaff-Schwartz regression).
- Implied volatility (Newton-Raphson with a bisection fallback).
- Discrete dividends, local volatility or Heston.
- Parallel Monte Carlo (`std::thread`), control variates, quasi-random (Sobol) numbers.
- Tree Greeks read directly from the first nodes of the tree instead of bump & reprice.
- Finite-difference PDE solver (Crank-Nicolson).
