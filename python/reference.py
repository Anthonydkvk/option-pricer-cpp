"""Reference implementation of the C++ pricer in NumPy/SciPy.

Written independently of the C++ code, with the same conventions (see CLAUDE.md):
vega and rho per 1.0 change of sigma and r, theta per year, central finite differences
with h_S = 1e-4 * S, h_sigma = h_r = h_T = 1e-4.

    uv run reference.py      # prices and Greeks of the standard case, with timings
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass, replace
from typing import Callable

import numpy as np
from scipy.stats import norm


# ---------------------------------------------------------------------------
# Model (same fields as include/pricer/option.hpp)
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class Market:
    S: float
    r: float
    q: float
    sigma: float


@dataclass(frozen=True)
class Option:
    kind: str  # "call" or "put"
    style: str  # "european" or "american"
    K: float
    T: float


@dataclass(frozen=True)
class Greeks:
    delta: float
    gamma: float
    vega: float
    theta: float
    rho: float


def validate(option: Option, market: Market) -> None:
    for name, value in (("S", market.S), ("sigma", market.sigma), ("K", option.K), ("T", option.T)):
        if not value > 0:  # also rejects NaN
            raise ValueError(f"{name} must be > 0, got {value}")
    if option.kind not in ("call", "put"):
        raise ValueError(f"kind must be 'call' or 'put', got {option.kind!r}")
    if option.style not in ("european", "american"):
        raise ValueError(f"style must be 'european' or 'american', got {option.style!r}")


def payoff(option: Option, spot):
    """Works on a float or on a NumPy array of spots."""
    if option.kind == "call":
        return np.maximum(spot - option.K, 0.0)
    return np.maximum(option.K - spot, 0.0)


# ---------------------------------------------------------------------------
# Black-Scholes
# ---------------------------------------------------------------------------
def _d1_d2(option: Option, market: Market) -> tuple[float, float]:
    vol_sqrt_t = market.sigma * math.sqrt(option.T)
    d1 = (math.log(market.S / option.K)
          + (market.r - market.q + 0.5 * market.sigma**2) * option.T) / vol_sqrt_t
    return d1, d1 - vol_sqrt_t


def bs_price(option: Option, market: Market) -> float:
    validate(option, market)
    if option.style != "european":
        raise ValueError("Black-Scholes prices European options only")
    d1, d2 = _d1_d2(option, market)
    spot_disc = market.S * math.exp(-market.q * option.T)
    strike_disc = option.K * math.exp(-market.r * option.T)
    if option.kind == "call":
        return spot_disc * norm.cdf(d1) - strike_disc * norm.cdf(d2)
    return strike_disc * norm.cdf(-d2) - spot_disc * norm.cdf(-d1)


def bs_greeks(option: Option, market: Market) -> Greeks:
    validate(option, market)
    if option.style != "european":
        raise ValueError("Black-Scholes prices European options only")
    S, K, T, r, q, sigma = market.S, option.K, option.T, market.r, market.q, market.sigma
    d1, d2 = _d1_d2(option, market)
    df_q, df_r = math.exp(-q * T), math.exp(-r * T)
    gamma = df_q * norm.pdf(d1) / (S * sigma * math.sqrt(T))
    vega = S * df_q * norm.pdf(d1) * math.sqrt(T)
    time_decay = -S * df_q * norm.pdf(d1) * sigma / (2 * math.sqrt(T))
    if option.kind == "call":
        return Greeks(
            delta=df_q * norm.cdf(d1),
            gamma=gamma,
            vega=vega,
            theta=time_decay - r * K * df_r * norm.cdf(d2) + q * S * df_q * norm.cdf(d1),
            rho=K * T * df_r * norm.cdf(d2),
        )
    return Greeks(
        delta=-df_q * norm.cdf(-d1),
        gamma=gamma,
        vega=vega,
        theta=time_decay + r * K * df_r * norm.cdf(-d2) - q * S * df_q * norm.cdf(-d1),
        rho=-K * T * df_r * norm.cdf(-d2),
    )


# ---------------------------------------------------------------------------
# CRR binomial tree
# ---------------------------------------------------------------------------
def _crr_parameters(option: Option, market: Market, steps: int) -> tuple[float, float, float]:
    validate(option, market)
    if steps <= 0:
        raise ValueError(f"steps must be > 0, got {steps}")
    dt = option.T / steps
    u = math.exp(market.sigma * math.sqrt(dt))
    d = 1.0 / u
    p = (math.exp((market.r - market.q) * dt) - d) / (u - d)
    if not 0.0 < p < 1.0:
        raise ValueError(f"CRR probability p = {p} is outside ]0, 1[")
    return u, p, math.exp(-market.r * dt)


def crr_price(option: Option, market: Market, steps: int) -> float:
    """Vectorised: each level of the tree is computed in one NumPy operation."""
    u, p, discount = _crr_parameters(option, market, steps)
    american = option.style == "american"
    # Node j of level i (j up-moves out of i) has spot S * u^(2j - i).
    values = payoff(option, market.S * u ** (2.0 * np.arange(steps + 1) - steps))
    for i in range(steps - 1, -1, -1):
        values = discount * (p * values[1:] + (1.0 - p) * values[:-1])
        if american:
            spots = market.S * u ** (2.0 * np.arange(i + 1) - i)
            values = np.maximum(values, payoff(option, spots))
    return float(values[0])


def crr_price_loop(option: Option, market: Market, steps: int) -> float:
    """Same algorithm with plain Python loops, like the C++ code: used only for timing."""
    u, p, discount = _crr_parameters(option, market, steps)
    american = option.style == "american"
    call = option.kind == "call"
    K = option.K
    spot = [market.S * u ** (2 * j - steps) for j in range(steps + 1)]
    values = [max(s - K, 0.0) if call else max(K - s, 0.0) for s in spot]
    for i in range(steps - 1, -1, -1):
        for j in range(i + 1):
            continuation = discount * (p * values[j + 1] + (1.0 - p) * values[j])
            if american:
                spot[j] *= u
                exercise = max(spot[j] - K, 0.0) if call else max(K - spot[j], 0.0)
                values[j] = max(continuation, exercise)
            else:
                values[j] = continuation
    return values[0]


# ---------------------------------------------------------------------------
# Monte Carlo (European options, exact GBM simulation at maturity)
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class MonteCarloResult:
    price: float
    std_error: float
    ci_low: float
    ci_high: float


def mc_price(option: Option, market: Market, paths: int, seed: int,
             antithetic: bool = False) -> MonteCarloResult:
    """NumPy's generator (PCG64) differs from the C++ one (mt19937_64): the same seed gives
    different numbers, so C++ and Python agree only within a few standard errors."""
    validate(option, market)
    if option.style != "european":
        raise ValueError("Monte Carlo prices European options only")
    if paths <= 0 or (antithetic and paths % 2):
        raise ValueError(f"invalid number of paths: {paths}")
    rng = np.random.default_rng(seed)
    drift = (market.r - market.q - 0.5 * market.sigma**2) * option.T
    vol = market.sigma * math.sqrt(option.T)

    def discounted_payoffs(z: np.ndarray) -> np.ndarray:
        return payoff(option, market.S * np.exp(drift + vol * z))

    if antithetic:
        z = rng.standard_normal(paths // 2)
        samples = 0.5 * (discounted_payoffs(z) + discounted_payoffs(-z))
    else:
        samples = discounted_payoffs(rng.standard_normal(paths))
    df = math.exp(-market.r * option.T)
    price = df * samples.mean()
    std_error = df * samples.std(ddof=1) / math.sqrt(samples.size)
    z_95 = norm.ppf(0.975)
    return MonteCarloResult(price, std_error, price - z_95 * std_error, price + z_95 * std_error)


# ---------------------------------------------------------------------------
# Finite-difference Greeks (same formulas as src/greeks.cpp)
# ---------------------------------------------------------------------------
PricingFunction = Callable[[Option, Market], float]


def fd_greeks(price: PricingFunction, option: Option, market: Market,
              spot_relative: float = 1e-4, h_sigma: float = 1e-4,
              h_r: float = 1e-4, h_T: float = 1e-4) -> Greeks:
    h_S = spot_relative * market.S
    base = price(option, market)
    up_S = price(option, replace(market, S=market.S + h_S))
    down_S = price(option, replace(market, S=market.S - h_S))
    up_sigma = price(option, replace(market, sigma=market.sigma + h_sigma))
    down_sigma = price(option, replace(market, sigma=market.sigma - h_sigma))
    up_r = price(option, replace(market, r=market.r + h_r))
    down_r = price(option, replace(market, r=market.r - h_r))
    up_T = price(replace(option, T=option.T + h_T), market)
    down_T = price(replace(option, T=option.T - h_T), market)
    return Greeks(
        delta=(up_S - down_S) / (2 * h_S),
        gamma=(up_S - 2 * base + down_S) / h_S**2,
        vega=(up_sigma - down_sigma) / (2 * h_sigma),
        theta=-(up_T - down_T) / (2 * h_T),
        rho=(up_r - down_r) / (2 * h_r),
    )


# ---------------------------------------------------------------------------
# Timing helper
# ---------------------------------------------------------------------------
def timed(f: Callable[[], object]) -> tuple[object, float]:
    """Runs f() once, returns (result, elapsed microseconds)."""
    start = time.perf_counter()
    result = f()
    return result, (time.perf_counter() - start) * 1e6


# ---------------------------------------------------------------------------
# Demo: standard case of CLAUDE.md
# ---------------------------------------------------------------------------
STANDARD_MARKET = Market(S=100.0, r=0.05, q=0.0, sigma=0.2)
STEPS = 2001
TREE_SPOT_BUMP = 2e-2  # larger than the spacing between tree nodes (see README)
PATHS = 500_000
SEED = 42


def _check_reference_values() -> None:
    """Reference values of CLAUDE.md: the Python code must pass the same tests as the C++."""
    call = Option("call", "european", 100.0, 1.0)
    put = Option("put", "european", 100.0, 1.0)
    g = bs_greeks(call, STANDARD_MARKET)
    expected = [
        (bs_price(call, STANDARD_MARKET), 10.450584, 1e-6),
        (bs_price(put, STANDARD_MARKET), 5.573526, 1e-6),
        (g.delta, 0.636831, 1e-6),
        (g.gamma, 0.018762, 1e-6),
        (g.vega, 37.524035, 1e-5),
        (g.theta, -6.414028, 1e-5),
        (g.rho, 53.232482, 1e-5),
        (crr_price(Option("put", "american", 100.0, 1.0), STANDARD_MARKET, 5000), 6.0902, 1e-3),
    ]
    for actual, target, tolerance in expected:
        assert abs(actual - target) < tolerance, (actual, target)


def main() -> None:
    _check_reference_values()
    market = STANDARD_MARKET
    call = Option("call", "european", 100.0, 1.0)

    def bs(o: Option, m: Market) -> float:
        return bs_price(o, m)

    def crr(o: Option, m: Market) -> float:
        return crr_price(o, m, STEPS)

    def mc(o: Option, m: Market) -> float:
        return mc_price(o, m, PATHS, SEED).price

    bs_price(call, market)  # warm-up: the first SciPy call loads extra code

    rows = []
    price, t_price = timed(lambda: bs_price(call, market))
    greeks, t_greeks = timed(lambda: bs_greeks(call, market))
    rows.append(("Black-Scholes (analytic)", price, None, greeks, t_price, t_greeks))
    greeks, t_greeks = timed(lambda: fd_greeks(bs, call, market))
    rows.append(("Black-Scholes (finite diff.)", price, None, greeks, t_price, t_greeks))
    price, t_price = timed(lambda: crr(call, market))
    greeks, t_greeks = timed(lambda: fd_greeks(crr, call, market, spot_relative=TREE_SPOT_BUMP))
    rows.append((f"CRR tree NumPy ({STEPS} steps)", price, None, greeks, t_price, t_greeks))
    result, t_price = timed(lambda: mc_price(call, market, PATHS, SEED))
    greeks, t_greeks = timed(lambda: fd_greeks(mc, call, market))
    rows.append((f"Monte Carlo NumPy ({PATHS} paths)", result.price, result.std_error, greeks,
                 t_price, t_greeks))

    print("Python reference (NumPy/SciPy), European call, standard case\n")
    header = f"{'Method':<34}" + "".join(
        f"{name:>11}" for name in ("Price", "Std err", "Delta", "Gamma", "Vega", "Theta", "Rho"))
    header += f"{'Time price':>13}{'Time greeks':>13}"
    print(header)
    print("-" * len(header))
    for label, price, std_error, g, t_price, t_greeks in rows:
        std = f"{std_error:>11.6f}" if std_error is not None else f"{'-':>11}"
        print(f"{label:<34}{price:>11.6f}{std}{g.delta:>11.6f}{g.gamma:>11.6f}{g.vega:>11.6f}"
              f"{g.theta:>11.6f}{g.rho:>11.6f}{t_price:>13.1f}{t_greeks:>13.1f}")

    _, t_loop = timed(lambda: crr_price_loop(call, market, STEPS))
    print(f"\nCRR tree, pure Python loops ({STEPS} steps): {t_loop:.1f} us for one price")
    print("Times in microseconds, single run. Reference values of CLAUDE.md: all checks passed.")


if __name__ == "__main__":
    main()
