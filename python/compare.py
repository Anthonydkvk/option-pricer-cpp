"""Compares the C++ pricer with the Python reference: accuracy and computation time.

Runs `pricer_cli --csv` for a few options, recomputes every line with reference.py, and prints
the differences and the timings.

    cmake --build build -j          # the C++ CLI must be built first (Release)
    cd python && uv run compare.py
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import subprocess
from dataclasses import dataclass
from pathlib import Path

import reference as ref

DEFAULT_CLI = Path(__file__).resolve().parent.parent / "build" / "apps" / "pricer_cli"
GREEK_NAMES = ("delta", "gamma", "vega", "theta", "rho")
# Same algorithm, same double precision: only rounding differs (order of the operations).
DETERMINISTIC_TOLERANCE = 1e-8


@dataclass
class Row:
    """One line of results: a price, its Greeks, and the timings in microseconds."""
    price: float
    std_error: float | None
    greeks: ref.Greeks
    price_us: float
    greeks_us: float


def run_cli(cli: Path, option: ref.Option, market: ref.Market, steps: int, paths: int,
            seed: int) -> dict[str, Row]:
    args = [str(cli), "--csv",
            "--S", repr(market.S), "--K", repr(option.K), "--r", repr(market.r),
            "--q", repr(market.q), "--sigma", repr(market.sigma), "--T", repr(option.T),
            "--type", option.kind, "--style", option.style,
            "--steps", str(steps), "--paths", str(paths), "--seed", str(seed)]
    output = subprocess.run(args, check=True, capture_output=True, text=True).stdout
    rows = {}
    for line in csv.DictReader(io.StringIO(output)):
        rows[line["method"]] = Row(
            price=float(line["price"]),
            std_error=float(line["std_error"]) if line["std_error"] else None,
            greeks=ref.Greeks(*(float(line[name]) for name in GREEK_NAMES)),
            price_us=float(line["price_us"]),
            greeks_us=float(line["greeks_us"]),
        )
    return rows


def run_python(option: ref.Option, market: ref.Market, steps: int, paths: int,
               seed: int) -> dict[str, Row]:
    """Same methods and parameters as apps/cli.cpp."""
    rows = {}

    def crr(o: ref.Option, m: ref.Market) -> float:
        return ref.crr_price(o, m, steps)

    def mc(o: ref.Option, m: ref.Market) -> float:
        return ref.mc_price(o, m, paths, seed).price

    if option.style == "european":
        ref.bs_price(option, market)  # warm-up
        price, t_price = ref.timed(lambda: ref.bs_price(option, market))
        greeks, t_greeks = ref.timed(lambda: ref.bs_greeks(option, market))
        rows["bs_analytic"] = Row(price, None, greeks, t_price, t_greeks)
        greeks, t_greeks = ref.timed(lambda: ref.fd_greeks(ref.bs_price, option, market))
        rows["bs_fd"] = Row(price, None, greeks, t_price, t_greeks)

    price, t_price = ref.timed(lambda: crr(option, market))
    greeks, t_greeks = ref.timed(
        lambda: ref.fd_greeks(crr, option, market, spot_relative=ref.TREE_SPOT_BUMP))
    rows["crr"] = Row(price, None, greeks, t_price, t_greeks)

    if option.style == "european":
        result, t_price = ref.timed(lambda: ref.mc_price(option, market, paths, seed))
        greeks, t_greeks = ref.timed(lambda: ref.fd_greeks(mc, option, market))
        rows["mc"] = Row(result.price, result.std_error, greeks, t_price, t_greeks)
    return rows


def max_abs_diff(a: ref.Greeks, b: ref.Greeks) -> float:
    return max(abs(getattr(a, name) - getattr(b, name)) for name in GREEK_NAMES)


def print_accuracy(cpp: dict[str, Row], py: dict[str, Row]) -> bool:
    print(f"  {'Method':<13}{'C++ price':>14}{'Python price':>14}{'|diff| price':>14}"
          f"{'max |diff| Greeks':>19}   Verdict")
    all_ok = True
    for method, c in cpp.items():
        p = py[method]
        diff = abs(c.price - p.price)
        greeks_diff = max_abs_diff(c.greeks, p.greeks)
        if c.std_error is None:
            ok = diff < DETERMINISTIC_TOLERANCE and greeks_diff < DETERMINISTIC_TOLERANCE
            verdict = "OK (identical)" if ok else "MISMATCH"
        else:
            # Different random generators: the two estimates are independent, so their
            # difference has a standard error of sqrt(se_cpp² + se_py²).
            combined = math.hypot(c.std_error, p.std_error)
            n_se = diff / combined
            ok = n_se < 3.0
            verdict = f"{'OK' if ok else 'MISMATCH'} ({n_se:.2f} std errors, random numbers differ)"
        all_ok &= ok
        print(f"  {method:<13}{c.price:>14.8f}{p.price:>14.8f}{diff:>14.2e}{greeks_diff:>19.2e}"
              f"   {verdict}")
    return all_ok


def print_timings(cpp: dict[str, Row], py: dict[str, Row]) -> None:
    print(f"  {'Method':<13}{'C++ price':>12}{'Python':>12}{'ratio':>9}"
          f"{'C++ Greeks':>14}{'Python':>12}{'ratio':>9}")
    for method, c in cpp.items():
        p = py[method]
        print(f"  {method:<13}{c.price_us:>12.1f}{p.price_us:>12.1f}{p.price_us / c.price_us:>8.1f}x"
              f"{c.greeks_us:>14.1f}{p.greeks_us:>12.1f}{p.greeks_us / c.greeks_us:>8.1f}x")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cli", type=Path, default=DEFAULT_CLI, help="path to pricer_cli")
    parser.add_argument("--steps", type=int, default=ref.STEPS)
    parser.add_argument("--paths", type=int, default=ref.PATHS)
    parser.add_argument("--seed", type=int, default=ref.SEED)
    args = parser.parse_args()
    if not args.cli.exists():
        raise SystemExit(f"{args.cli} not found: build the C++ project first (cmake --build build)")

    market = ref.STANDARD_MARKET
    cases = [
        ref.Option("call", "european", 100.0, 1.0),
        ref.Option("put", "european", 100.0, 1.0),
        ref.Option("put", "american", 100.0, 1.0),
    ]
    print(f"C++ ({args.cli.name}) vs Python reference: S={market.S} r={market.r} q={market.q} "
          f"sigma={market.sigma}, {args.steps} tree steps, {args.paths} MC paths, seed {args.seed}")
    all_ok = True
    for option in cases:
        cpp = run_cli(args.cli, option, market, args.steps, args.paths, args.seed)
        py = run_python(option, market, args.steps, args.paths, args.seed)
        print(f"\n=== {option.style.capitalize()} {option.kind}, K={option.K}, T={option.T} ===")
        print("Accuracy")
        all_ok &= print_accuracy(cpp, py)
        print("Time (microseconds, single run; ratio = Python / C++)")
        print_timings(cpp, py)

    call = cases[0]
    _, t_loop = ref.timed(lambda: ref.crr_price_loop(call, market, args.steps))
    _, t_numpy = ref.timed(lambda: ref.crr_price(call, market, args.steps))
    t_cpp = run_cli(args.cli, call, market, args.steps, 2, args.seed)["crr"].price_us
    print(f"\nCRR tree, one price, {args.steps} steps (European call):")
    print(f"  pure Python loops {t_loop:>12.1f} us   ({t_loop / t_cpp:.0f}x C++)")
    print(f"  NumPy vectorised  {t_numpy:>12.1f} us   ({t_numpy / t_cpp:.0f}x C++)")
    print(f"  C++               {t_cpp:>12.1f} us")

    print("\nAll checks passed." if all_ok else "\nSOME CHECKS FAILED.")
    if not all_ok:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
