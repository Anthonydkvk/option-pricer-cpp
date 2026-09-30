"""Convergence of the C++ CRR tree and Monte Carlo towards Black-Scholes (log-log plot).

Every point is a real run of `pricer_cli --csv`; the figure is saved to docs/convergence.png.

    cmake --build build -j          # the C++ CLI must be built first (Release)
    cd python && uv run convergence.py
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # no window: we only write a PNG file
import matplotlib.pyplot as plt
import numpy as np

import reference as ref
from compare import DEFAULT_CLI, run_cli

OUTPUT = Path(__file__).resolve().parent.parent / "docs" / "convergence.png"

# Colours: first three categorical slots of a colour-blind-safe palette, light mode.
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_SECONDARY = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"


def crr_errors(cli: Path, option: ref.Option, market: ref.Market, steps: list[int],
               bs: float) -> np.ndarray:
    # --paths 2: the CLI always runs Monte Carlo too, keep it negligible here.
    return np.array([abs(run_cli(cli, option, market, n, 2, ref.SEED)["crr"].price - bs)
                     for n in steps])


def mc_runs(cli: Path, option: ref.Option, market: ref.Market, paths: list[int],
            antithetic: bool) -> tuple[np.ndarray, np.ndarray]:
    """Returns (price, standard error) for each number of paths."""
    extra = ("--antithetic",) if antithetic else ()
    rows = [run_cli(cli, option, market, 1, n, ref.SEED, extra)["mc"] for n in paths]
    return np.array([r.price for r in rows]), np.array([r.std_error for r in rows])


def style_axes(ax: plt.Axes, title: str, xlabel: str) -> None:
    ax.set_facecolor(SURFACE)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_title(title, loc="left", color=INK, fontsize=12, fontweight="bold", pad=10)
    ax.set_xlabel(xlabel, color=INK_SECONDARY)
    ax.set_ylabel("absolute error vs Black-Scholes", color=INK_SECONDARY)
    ax.grid(True, which="major", color=GRID, linewidth=0.8)
    ax.tick_params(colors=MUTED, which="both")
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)


def label_end(ax: plt.Axes, x: float, y: float, text: str, color: str, dy: float = 0.0) -> None:
    """Direct label at the end of a series: text in ink, a coloured marker carries identity.
    `dy` shifts the text vertically (in points) when two labels would overlap."""
    ax.annotate(text, (x, y), xytext=(8, dy), textcoords="offset points", va="center",
                color=INK_SECONDARY, fontsize=9)
    ax.plot([x], [y], "o", color=color, markersize=6, markeredgecolor=SURFACE, zorder=4)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cli", type=Path, default=DEFAULT_CLI, help="path to pricer_cli")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    if not args.cli.exists():
        raise SystemExit(f"{args.cli} not found: build the C++ project first (cmake --build build)")

    market = ref.STANDARD_MARKET
    call = ref.Option("call", "european", 100.0, 1.0)
    bs = run_cli(args.cli, call, market, 1, 2, ref.SEED)["bs_analytic"].price

    # CRR: every N from 10 to 1000, at the money and out of the money. Out of the money, the
    # strike falls sometimes close to a node, sometimes between two nodes: the error oscillates.
    steps = list(range(10, 1001))
    otm_call = ref.Option("call", "european", 110.0, 1.0)
    bs_otm = run_cli(args.cli, otm_call, market, 1, 2, ref.SEED)["bs_analytic"].price
    err_atm = crr_errors(args.cli, call, market, steps, bs)
    err_otm = crr_errors(args.cli, otm_call, market, steps, bs_otm)

    # Monte Carlo: from 100 to 1 000 000 paths (even numbers, required by antithetic variates).
    paths = sorted({int(n) // 2 * 2 for n in np.geomspace(1e2, 1e6, 17)})
    price, std_error = mc_runs(args.cli, call, market, paths, antithetic=False)
    _, std_error_anti = mc_runs(args.cli, call, market, paths, antithetic=True)
    mc_error = np.abs(price - bs)

    fig, (ax_crr, ax_mc) = plt.subplots(1, 2, figsize=(12, 4.8), facecolor=SURFACE)

    # --- CRR -----------------------------------------------------------------
    style_axes(ax_crr, "CRR tree: error ~ 1/N", "number of steps N (every N from 10 to 1000)")
    ax_crr.plot(steps, err_otm, color=ORANGE, linewidth=1, label="K = 110 (out of the money)")
    ax_crr.plot(steps, err_atm, color=BLUE, linewidth=2, label="K = 100 (at the money)")
    ref_crr = 2.0 * err_atm[0] * steps[0] / np.array(steps, dtype=float)
    ax_crr.plot(steps, ref_crr, color=MUTED, linewidth=1, linestyle="--", label="slope 1/N")
    label_end(ax_crr, steps[-1], err_atm[-1], "K = 100", BLUE, dy=6)
    label_end(ax_crr, steps[-1], err_otm[-1], "K = 110", ORANGE, dy=-6)

    # --- Monte Carlo ---------------------------------------------------------
    style_axes(ax_mc, "Monte Carlo: error ~ 1/sqrt(N)", "number of paths N (seed 42)")
    ax_mc.plot(paths, std_error, color=BLUE, linewidth=2, label="standard error")
    ax_mc.plot(paths, std_error_anti, color=AQUA, linewidth=2,
               label="standard error, antithetic")
    ax_mc.plot(paths, mc_error, "o", color=ORANGE, markersize=6, markeredgecolor=SURFACE,
               label="|MC - BS|, one run")
    ref_mc = std_error[0] * np.sqrt(paths[0] / np.array(paths, dtype=float))
    ax_mc.plot(paths, ref_mc, color=MUTED, linewidth=1, linestyle="--", label="slope 1/sqrt(N)")
    label_end(ax_mc, paths[-1], std_error[-1], "standard error", BLUE)
    label_end(ax_mc, paths[-1], std_error_anti[-1], "antithetic", AQUA)

    for ax in (ax_crr, ax_mc):
        ax.legend(frameon=False, labelcolor=INK_SECONDARY, fontsize=9, loc="lower left")
        ax.margins(x=0.02)
        ax.set_xlim(right=ax.get_xlim()[1] * 4)  # room for the direct labels

    fig.suptitle("European calls, S = 100, r = 5 %, sigma = 20 %, T = 1; Monte Carlo with K = 100  (C++ pricer_cli)",
                 color=INK_SECONDARY, fontsize=10, x=0.01, ha="left")
    fig.tight_layout()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=150, facecolor=SURFACE)
    print(f"saved {args.output}")

    # Numbers behind the figure, for the README.
    print(f"\nBlack-Scholes: {bs:.10f}")
    for n in (100, 101, 500, 501, 1000):
        i = steps.index(n)
        print(f"CRR {n:>4} steps: error {err_atm[i]:.2e} (K=100), {err_otm[i]:.2e} (K=110)")
    print(f"MC at {paths[-1]} paths: |error| {mc_error[-1]:.2e}, "
          f"standard error {std_error[-1]:.2e}, antithetic {std_error_anti[-1]:.2e}")
    print(f"MC standard error ratio {paths[0]} -> {paths[-1]} paths: "
          f"{std_error[0] / std_error[-1]:.0f}x for {paths[-1] / paths[0]:.0f}x more paths")


if __name__ == "__main__":
    main()
