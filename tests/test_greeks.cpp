#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "pricer/binomial.hpp"
#include "pricer/black_scholes.hpp"
#include "pricer/greeks.hpp"
#include "pricer/monte_carlo.hpp"

using namespace pricer;

namespace {

const MarketParams standard_market{100.0, 0.05, 0.0, 0.2};

Option european(const OptionType type, const double K = 100.0, const double T = 1.0) {
    return Option{type, ExerciseStyle::European, K, T};
}

// Écart relatif, avec un plancher pour les grecques proches de 0.
void expect_relative_near(const double actual, const double expected, const double tolerance,
                          const char* name) {
    const double scale = std::max(std::abs(expected), 1e-2);
    EXPECT_LT(std::abs(actual - expected) / scale, tolerance)
        << name << ": actual=" << actual << " expected=" << expected;
}

void expect_greeks_near(const Greeks& actual, const Greeks& expected, const double tolerance) {
    expect_relative_near(actual.delta, expected.delta, tolerance, "delta");
    expect_relative_near(actual.gamma, expected.gamma, tolerance, "gamma");
    expect_relative_near(actual.vega, expected.vega, tolerance, "vega");
    expect_relative_near(actual.theta, expected.theta, tolerance, "theta");
    expect_relative_near(actual.rho, expected.rho, tolerance, "rho");
}

const PricingFunction bs_price = [](const Option& o, const MarketParams& m) {
    return black_scholes_price(o, m);
};

}  // namespace

// ---------------------------------------------------------------------------
// Black-Scholes : différences finies ≈ formules analytiques (1e-4 relatif)
// ---------------------------------------------------------------------------
TEST(FiniteDifferenceGreeks, MatchBlackScholesAnalyticStandardCase) {
    for (const OptionType type : {OptionType::Call, OptionType::Put}) {
        const Option opt = european(type);
        expect_greeks_near(finite_difference_greeks(bs_price, opt, standard_market),
                           black_scholes_greeks(opt, standard_market), 1e-4);
    }
}

TEST(FiniteDifferenceGreeks, MatchBlackScholesAnalyticAcrossStrikesAndMarkets) {
    const MarketParams with_dividend{100.0, 0.03, 0.02, 0.3};
    for (const MarketParams& m : {standard_market, with_dividend}) {
        for (const OptionType type : {OptionType::Call, OptionType::Put}) {
            for (const double K : {80.0, 100.0, 120.0}) {
                const Option opt = european(type, K, 0.5);
                expect_greeks_near(finite_difference_greeks(bs_price, opt, m),
                                   black_scholes_greeks(opt, m), 1e-4);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Arbre CRR
// ---------------------------------------------------------------------------
// À nombre de pas fixé, le prix CRR est linéaire par morceaux en S (somme de max(S u^k - K, 0)
// pondérés) : son vrai Gamma vaut 0 entre deux nœuds et est infini quand un nœud croise le strike.
// Avec S = K et un nombre de pas pair, un nœud tombe pile sur K : le pas par défaut (h_S = 0.01)
// mesure ce coude au lieu de la courbure lissée.
TEST(FiniteDifferenceGreeks, DefaultSpotBumpIsTooSmallForTreeGamma) {
    const PricingFunction crr_500 = [](const Option& o, const MarketParams& m) {
        return binomial_price(o, m, 500);
    };
    const Option put = european(OptionType::Put);
    EXPECT_GT(finite_difference_greeks(crr_500, put, standard_market).gamma, 1.0);  // BS : 0.0188
}

// Remède : un pas h_S plus grand que l'écart entre deux nœuds (≈ S σ sqrt(T / steps) ≈ 0.45 ici)
// et un nombre de pas impair (aucun nœud exactement sur le strike).
TEST(FiniteDifferenceGreeks, BinomialEuropeanCloseToBlackScholes) {
    const PricingFunction crr = [](const Option& o, const MarketParams& m) {
        return binomial_price(o, m, 2001);
    };
    BumpSizes tree_bumps;
    tree_bumps.spot_relative = 2e-2;
    for (const OptionType type : {OptionType::Call, OptionType::Put}) {
        const Option opt = european(type);
        expect_greeks_near(finite_difference_greeks(crr, opt, standard_market, tree_bumps),
                           black_scholes_greeks(opt, standard_market), 2e-2);
    }
}

TEST(FiniteDifferenceGreeks, BinomialAmericanPutIsConsistent) {
    const PricingFunction crr = [](const Option& o, const MarketParams& m) {
        return binomial_price(o, m, 2001);
    };
    BumpSizes tree_bumps;
    tree_bumps.spot_relative = 2e-2;
    const Option american{OptionType::Put, ExerciseStyle::American, 100.0, 1.0};
    const Greeks g = finite_difference_greeks(crr, american, standard_market, tree_bumps);
    EXPECT_GT(g.delta, -1.0);
    EXPECT_LT(g.delta, 0.0);
    EXPECT_GT(g.gamma, 0.0);
    EXPECT_GT(g.vega, 0.0);
    EXPECT_LT(g.rho, 0.0);
    // L'exercice anticipé rend le put américain plus sensible au spot que l'européen.
    EXPECT_LT(g.delta, black_scholes_greeks(european(OptionType::Put), standard_market).delta);
}

// ---------------------------------------------------------------------------
// Monte Carlo : common random numbers (même seed pour les 9 prix)
// ---------------------------------------------------------------------------
TEST(FiniteDifferenceGreeks, MonteCarloWithCommonRandomNumbersCloseToBlackScholes) {
    const PricingFunction mc = [](const Option& o, const MarketParams& m) {
        return monte_carlo_price(o, m, 500'000, 42).price;
    };
    for (const OptionType type : {OptionType::Call, OptionType::Put}) {
        const Option opt = european(type);
        const Greeks fd = finite_difference_greeks(mc, opt, standard_market);
        const Greeks bs = black_scholes_greeks(opt, standard_market);
        expect_relative_near(fd.delta, bs.delta, 1e-2, "delta");
        expect_relative_near(fd.vega, bs.vega, 1e-2, "vega");
        expect_relative_near(fd.theta, bs.theta, 1e-2, "theta");
        expect_relative_near(fd.rho, bs.rho, 1e-2, "rho");
        // Gamma MC : seules les trajectoires qui finissent à ±h du strike contribuent -> très bruité.
        expect_relative_near(fd.gamma, bs.gamma, 1e-1, "gamma");
    }
}

// Sans common random numbers, chaque prix bumpé a son propre bruit (erreur std ≈ 0.02) :
// divisé par 2 h_S = 0.02, ce bruit rend le Delta inutilisable.
TEST(FiniteDifferenceGreeks, MonteCarloWithoutCommonRandomNumbersIsUseless) {
    std::uint64_t seed = 0;
    const PricingFunction mc_new_seed = [&seed](const Option& o, const MarketParams& m) {
        return monte_carlo_price(o, m, 500'000, ++seed).price;
    };
    const Option call = european(OptionType::Call);
    const double delta = finite_difference_greeks(mc_new_seed, call, standard_market).delta;
    EXPECT_GT(std::abs(delta - black_scholes_greeks(call, standard_market).delta), 0.1);
}

// ---------------------------------------------------------------------------
// Entrées invalides
// ---------------------------------------------------------------------------
TEST(FiniteDifferenceGreeks, RejectsNonPositiveBumps) {
    const Option call = european(OptionType::Call);
    BumpSizes zero_spot;
    zero_spot.spot_relative = 0.0;
    EXPECT_THROW(finite_difference_greeks(bs_price, call, standard_market, zero_spot),
                 std::invalid_argument);
    BumpSizes negative_sigma;
    negative_sigma.sigma = -1e-4;
    EXPECT_THROW(finite_difference_greeks(bs_price, call, standard_market, negative_sigma),
                 std::invalid_argument);
}

TEST(FiniteDifferenceGreeks, RejectsBumpLargerThanMaturity) {
    // T - h_T <= 0 : le prix bumpé vers le bas est invalide.
    const Option short_call = european(OptionType::Call, 100.0, 1e-4);
    EXPECT_THROW(finite_difference_greeks(bs_price, short_call, standard_market),
                 std::invalid_argument);
}

TEST(FiniteDifferenceGreeks, RejectsInvalidInputs) {
    EXPECT_THROW(finite_difference_greeks(bs_price, european(OptionType::Call, -1.0), standard_market),
                 std::invalid_argument);
    EXPECT_THROW(finite_difference_greeks(bs_price, european(OptionType::Call),
                                          MarketParams{0.0, 0.05, 0.0, 0.2}),
                 std::invalid_argument);
}
