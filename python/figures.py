"""Figures comparing the three pricing methods, the Greeks, American vs European and C++ vs Python.

Every C++ number is a real run of `pricer_cli --csv`; the PNG files are saved in docs/:

    prices_vs_spot.png              BS, CRR and Monte Carlo prices and their error, call and put
    greeks_vs_spot.png              the five Greeks: analytic BS vs finite differences CRR and MC
    american_vs_european.png        early-exercise premium of the American put (and call)
    monte_carlo_distribution.png    300 Monte Carlo runs with different seeds vs the exact price
    benchmark.png                   time for one price, C++ vs Python

    cmake --build build -j          # the C++ CLI must be built first (Release)
    cd python && uv run figures.py
"""

from __future__ import annotations

import argparse
import statistics
from dataclasses import replace
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # no window: we only write PNG files
import matplotlib.pyplot as plt
import numpy as np

import reference as ref
from compare import DEFAULT_CLI, run_cli
from convergence import AXIS, BLUE, GRID, INK, INK_SECONDARY, MUTED, ORANGE, AQUA, SURFACE

DOCS = Path(__file__).resolve().parent.parent / "docs"
YELLOW = "#eda100"  # fourth categorical slot of the same palette


def thousands(n: int) -> str:
    """1000000 -> '1 000 000' (French-style thousands separator, unambiguous in English too)."""
    return f"{n:,}".replace(",", " ")

MARKET = ref.STANDARD_MARKET
SPOTS = np.arange(60.0, 140.0 + 1e-9, 2.0)
GRID_STEPS = 501  # odd number of steps, enough for 1e-2 accuracy, fast for 41 spots
GRID_PATHS = 100_000
SUBTITLE = "S = 100 unless on the x-axis, K = 100, r = 5 %, sigma = 20 %, T = 1  (C++ pricer_cli)"


# ---------------------------------------------------------------------------
# Styling helpers (same look as convergence.png)
# ---------------------------------------------------------------------------
def style_axes(ax: plt.Axes, title: str, xlabel: str, ylabel: str) -> None:
    ax.set_facecolor(SURFACE)
    ax.set_title(title, loc="left", color=INK, fontsize=11, fontweight="bold", pad=8)
    ax.set_xlabel(xlabel, color=INK_SECONDARY)
    ax.set_ylabel(ylabel, color=INK_SECONDARY)
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.tick_params(colors=MUTED)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)


def legend(ax: plt.Axes, loc: str = "best") -> None:
    ax.legend(frameon=False, labelcolor=INK_SECONDARY, fontsize=9, loc=loc)


def save(fig: plt.Figure, name: str, subtitle: str = SUBTITLE) -> None:
    fig.suptitle(subtitle, color=INK_SECONDARY, fontsize=10, x=0.01, ha="left")
    fig.tight_layout()
    path = DOCS / name
    fig.savefig(path, dpi=150, facecolor=SURFACE)
    plt.close(fig)
    print(f"saved {path}")


def run_grid(cli: Path, option: ref.Option, steps: int = GRID_STEPS,
             paths: int = GRID_PATHS) -> list[dict]:
    """One CLI run per spot of SPOTS."""
    return [run_cli(cli, option, replace(MARKET, S=float(s)), steps, paths, ref.SEED)
            for s in SPOTS]


# ---------------------------------------------------------------------------
# 1. Prices of the three methods vs spot, and their error vs Black-Scholes
# ---------------------------------------------------------------------------
def figure_prices(cli: Path) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), facecolor=SURFACE,
                             gridspec_kw={"height_ratios": [3, 2]})
    for col, kind in enumerate(("call", "put")):
        option = ref.Option(kind, "european", 100.0, 1.0)
        runs = run_grid(cli, option)
        bs = np.array([r["bs_analytic"].price for r in runs])
        crr = np.array([r["crr"].price for r in runs])
        mc = np.array([r["mc"].price for r in runs])
        se = np.array([r["mc"].std_error for r in runs])
        payoff = ref.payoff(option, SPOTS)

        ax = axes[0, col]
        style_axes(ax, f"European {kind}: the three methods give the same price",
                   "spot S", "option price")
        ax.plot(SPOTS, payoff, color=MUTED, linewidth=1, linestyle="--",
                label="payoff at maturity (intrinsic value)")
        ax.plot(SPOTS, bs, color=BLUE, linewidth=2, label="Black-Scholes (exact)")
        ax.plot(SPOTS, crr, "o", color=ORANGE, markersize=5, markeredgecolor=SURFACE,
                label=f"CRR tree, {GRID_STEPS} steps")
        ax.plot(SPOTS, mc, "s", color=AQUA, markersize=4, markeredgecolor=SURFACE,
                label=f"Monte Carlo, {thousands(GRID_PATHS)} paths")
        legend(ax, "upper left" if kind == "call" else "upper right")

        ax = axes[1, col]
        style_axes(ax, "Zoom on the error: method - Black-Scholes", "spot S",
                   "price - BS price")
        ax.fill_between(SPOTS, -1.96 * se, 1.96 * se, color=AQUA, alpha=0.15, linewidth=0,
                        label="Monte Carlo 95 % band (± 1.96 standard errors)")
        ax.axhline(0.0, color=AXIS, linewidth=1)
        ax.plot(SPOTS, mc - bs, "s", color=AQUA, markersize=4, markeredgecolor=SURFACE,
                label="Monte Carlo error")
        ax.plot(SPOTS, crr - bs, "o-", color=ORANGE, markersize=4, linewidth=1,
                markeredgecolor=SURFACE, label="CRR error")
        legend(ax, "lower left" if kind == "call" else "lower right")
    save(fig, "prices_vs_spot.png")


# ---------------------------------------------------------------------------
# 2. The five Greeks vs spot: analytic vs finite differences
# ---------------------------------------------------------------------------
GREEK_LABELS = {
    "delta": ("Delta  (dV/dS)", "price change per +1 in S"),
    "gamma": ("Gamma  (d²V/dS²)", "Delta change per +1 in S"),
    "vega": ("Vega  (dV/dsigma)", "per +1.0 (100 %) in sigma"),
    "theta": ("Theta  (dV/dt)", "per year"),
    "rho": ("Rho  (dV/dr)", "per +1.0 (100 %) in r"),
}


def figure_greeks(cli: Path) -> None:
    option = ref.Option("call", "european", 100.0, 1.0)
    runs = run_grid(cli, option)
    fig, axes = plt.subplots(2, 3, figsize=(14, 8), facecolor=SURFACE)
    for ax, (name, (title, unit)) in zip(axes.flat, GREEK_LABELS.items()):
        style_axes(ax, title, "spot S", unit)
        for method, label, style, color in (
                ("bs_analytic", "Black-Scholes analytic", "-", BLUE),
                ("crr", f"CRR, finite differences", "o", ORANGE),
                ("mc", "Monte Carlo, finite differences", "s", AQUA)):
            values = [getattr(r[method].greeks, name) for r in runs]
            if style == "-":
                ax.plot(SPOTS, values, color=color, linewidth=2, label=label)
            else:
                ax.plot(SPOTS, values, style, color=color, markersize=4,
                        markeredgecolor=SURFACE, label=label)
        ax.axvline(option.K, color=AXIS, linewidth=1, linestyle=":")

    # Sixth panel: how to read the figure.
    text_ax = axes.flat[-1]
    text_ax.axis("off")
    handles, labels = axes.flat[0].get_legend_handles_labels()
    text_ax.legend(handles, labels, frameon=False, labelcolor=INK_SECONDARY, fontsize=10,
                   loc="upper left", title="European call", title_fontsize=10)
    text_ax.text(0.0, 0.45,
                 "Dotted line: strike K = 100.\n\n"
                 "Finite differences = bump & reprice:\n"
                 "Delta = (V(S+h) - V(S-h)) / 2h.\n\n"
                 "Gamma peaks at the money: that is where\n"
                 "Delta changes fastest. It is the noisiest\n"
                 "Greek for Monte Carlo (a second derivative\n"
                 "of a random price), even with common random\n"
                 f"numbers; {thousands(GRID_PATHS)} paths, seed {ref.SEED}.",
                 color=INK_SECONDARY, fontsize=9, va="top", transform=text_ax.transAxes)
    save(fig, "greeks_vs_spot.png")


# ---------------------------------------------------------------------------
# 3. American vs European
# ---------------------------------------------------------------------------
def figure_american(cli: Path) -> None:
    put_am = ref.Option("put", "american", 100.0, 1.0)
    put_eu = ref.Option("put", "european", 100.0, 1.0)
    call_am = ref.Option("call", "american", 100.0, 1.0)
    call_eu = ref.Option("call", "european", 100.0, 1.0)
    american = np.array([r["crr"].price for r in run_grid(cli, put_am, paths=2)])
    european = np.array([r["bs_analytic"].price for r in run_grid(cli, put_eu, paths=2)])
    call_premium = (np.array([r["crr"].price for r in run_grid(cli, call_am, paths=2)])
                    - np.array([r["crr"].price for r in run_grid(cli, call_eu, paths=2)]))
    intrinsic = ref.payoff(put_am, SPOTS)
    # Exercise region: the American price equals the immediate exercise value.
    exercise = np.isclose(american, intrinsic, atol=1e-6) & (intrinsic > 0)

    fig, (ax_price, ax_premium) = plt.subplots(1, 2, figsize=(12, 4.8), facecolor=SURFACE)
    style_axes(ax_price, "Put: the American is worth more than the European", "spot S",
               "option price")
    if exercise.any():
        boundary = SPOTS[exercise].max()
        ax_price.axvspan(SPOTS[0], boundary, color=YELLOW, alpha=0.12, linewidth=0,
                         label=f"exercise now is optimal (S <= {boundary:.0f})")
    ax_price.plot(SPOTS, intrinsic, color=MUTED, linewidth=1, linestyle="--",
                  label="immediate exercise value max(K - S, 0)")
    ax_price.plot(SPOTS, european, color=BLUE, linewidth=2, label="European put (Black-Scholes)")
    ax_price.plot(SPOTS, american, color=ORANGE, linewidth=2,
                  label=f"American put (CRR, {GRID_STEPS} steps)")
    legend(ax_price, "upper right")

    style_axes(ax_premium, "Early-exercise premium = American - European", "spot S",
               "premium")
    ax_premium.plot(SPOTS, american - european, color=ORANGE, linewidth=2, label="put")
    ax_premium.plot(SPOTS, call_premium, color=BLUE, linewidth=2,
                    label="call without dividend (always 0: never exercise early)")
    ax_premium.axhline(0.0, color=AXIS, linewidth=1)
    legend(ax_premium, "upper right")
    save(fig, "american_vs_european.png")


# ---------------------------------------------------------------------------
# 4. Monte Carlo is random: 300 runs with different seeds
# ---------------------------------------------------------------------------
def figure_monte_carlo(cli: Path, runs: int = 300, paths: int = 10_000) -> None:
    option = ref.Option("call", "european", 100.0, 1.0)
    results = [run_cli(cli, option, MARKET, 1, paths, seed)["mc"] for seed in range(runs)]
    prices = np.array([r.price for r in results])
    se = np.array([r.std_error for r in results])
    exact = run_cli(cli, option, MARKET, 1, 2, ref.SEED)["bs_analytic"].price
    covered = np.mean(np.abs(prices - exact) <= 1.96 * se)

    fig, (ax_hist, ax_ci) = plt.subplots(1, 2, figsize=(12, 4.8), facecolor=SURFACE)
    style_axes(ax_hist, f"{runs} Monte Carlo prices ({thousands(paths)} paths each)",
               "estimated price", "number of runs")
    ax_hist.hist(prices, bins=25, color=AQUA, edgecolor=SURFACE, linewidth=2,
                 label="Monte Carlo prices (seeds 0 to 299)")
    mean_se = se.mean()
    x = np.linspace(prices.min(), prices.max(), 200)
    width = (prices.max() - prices.min()) / 25
    density = np.exp(-0.5 * ((x - exact) / mean_se) ** 2) / (mean_se * np.sqrt(2 * np.pi))
    ax_hist.plot(x, density * runs * width, color=INK_SECONDARY, linewidth=1.5,
                 label="normal law predicted by the standard error")
    ax_hist.axvline(exact, color=BLUE, linewidth=2, label=f"Black-Scholes {exact:.4f}")
    ax_hist.set_ylim(top=ax_hist.get_ylim()[1] * 1.35)  # room for the legend above the bars
    legend(ax_hist, "upper left")

    shown = 40
    style_axes(ax_ci, f"95 % confidence intervals: {covered:.0%} contain the exact price",
               "estimated price", "run (seed)")
    for i in range(shown):
        hit = abs(prices[i] - exact) <= 1.96 * se[i]
        color = AQUA if hit else ORANGE
        ax_ci.plot([prices[i] - 1.96 * se[i], prices[i] + 1.96 * se[i]], [i, i], color=color,
                   linewidth=2, solid_capstyle="round")
        ax_ci.plot([prices[i]], [i], "o", color=color, markersize=4, markeredgecolor=SURFACE)
    ax_ci.axvline(exact, color=BLUE, linewidth=2)
    ax_ci.plot([], [], color=AQUA, linewidth=2, label="interval contains the exact price")
    ax_ci.plot([], [], color=ORANGE, linewidth=2, label="interval misses it")
    ax_ci.set_ylim(-1, shown + 4)
    legend(ax_ci, "upper left")
    save(fig, "monte_carlo_distribution.png",
         f"European call, S = K = 100, r = 5 %, sigma = 20 %, T = 1; first {shown} runs on the "
         "right  (C++ pricer_cli)")
    print(f"  {covered:.1%} of the {runs} intervals contain the exact price; "
          f"std of the prices {prices.std(ddof=1):.4f}, mean standard error {mean_se:.4f}")


# ---------------------------------------------------------------------------
# 5. Benchmark: time for one price, C++ vs Python
# ---------------------------------------------------------------------------
def median_time(f, repeats: int) -> float:
    return statistics.median(ref.timed(f)[1] for _ in range(repeats))


def figure_benchmark(cli: Path, repeats: int = 5) -> None:
    call = ref.Option("call", "european", 100.0, 1.0)
    put_am = ref.Option("put", "american", 100.0, 1.0)
    steps, paths, seed = ref.STEPS, ref.PATHS, ref.SEED

    def cpp(option: ref.Option, method: str) -> float:
        return statistics.median(run_cli(cli, option, MARKET, steps, paths, seed)[method].price_us
                                 for _ in range(repeats))

    ref.bs_price(call, MARKET)  # warm-up
    rows = [  # (label, C++ us or None, Python us)
        ("Black-Scholes", cpp(call, "bs_analytic"),
         median_time(lambda: ref.bs_price(call, MARKET), repeats)),
        (f"CRR European, {steps} steps\nPython = NumPy", cpp(call, "crr"),
         median_time(lambda: ref.crr_price(call, MARKET, steps), repeats)),
        (f"CRR European, {steps} steps\nPython = pure loops", None,
         median_time(lambda: ref.crr_price_loop(call, MARKET, steps), 1)),
        (f"CRR American put\n{steps} steps, NumPy", cpp(put_am, "crr"),
         median_time(lambda: ref.crr_price(put_am, MARKET, steps), repeats)),
        (f"Monte Carlo\n{thousands(paths)} paths, NumPy", cpp(call, "mc"),
         median_time(lambda: ref.mc_price(call, MARKET, paths, seed), repeats)),
    ]
    # The pure-loop row is compared with the same C++ CRR time as the NumPy row above it.
    cpp_crr = rows[1][1]

    fig, ax = plt.subplots(figsize=(11, 5.5), facecolor=SURFACE)
    style_axes(ax, "Time for one price (log scale, shorter is faster)",
               "microseconds (median of 5 runs)", "")
    ax.set_xscale("log")
    height = 0.36

    def label(x: float, y: float, text: str) -> None:
        ax.annotate(text, (x, y), xytext=(4, 0), textcoords="offset points", va="center",
                    fontsize=8, color=INK_SECONDARY)

    def micro(t: float) -> str:
        return f"{t:.1f} us" if t < 10 else f"{thousands(round(t))} us"

    for yi, (_, c, p) in enumerate(rows):
        if c is not None:
            ax.barh(yi - height / 2 - 0.01, c, height, color=BLUE,
                    label="C++ (Release)" if yi == 0 else None)
            label(c, yi - height / 2, micro(c))
        ax.barh(yi + height / 2 + 0.01, p, height, color=ORANGE,
                label="Python" if yi == 0 else None)
        ratio = p / (c if c is not None else cpp_crr)
        ratio_text = f"{ratio:.0f}x slower than C++" if ratio >= 1 else \
            f"{1 / ratio:.1f}x faster than C++"
        label(p, yi + height / 2, f"{micro(p)}   ({ratio_text})")
    ax.set_yticks(range(len(rows)), [r[0] for r in rows], color=INK_SECONDARY)
    ax.invert_yaxis()
    ax.grid(True, axis="x", which="major", color=GRID)
    ax.grid(False, axis="y")
    ax.set_xlim(right=ax.get_xlim()[1] * 30)  # room for the labels
    legend(ax, "upper right")
    save(fig, "benchmark.png", "Standard case, European call (and American put); one price, no "
         "Greeks; C++ times measured inside pricer_cli")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cli", type=Path, default=DEFAULT_CLI, help="path to pricer_cli")
    args = parser.parse_args()
    if not args.cli.exists():
        raise SystemExit(f"{args.cli} not found: build the C++ project first (cmake --build build)")
    DOCS.mkdir(parents=True, exist_ok=True)
    figure_prices(args.cli)
    figure_greeks(args.cli)
    figure_american(args.cli)
    figure_monte_carlo(args.cli)
    figure_benchmark(args.cli)


if __name__ == "__main__":
    main()
