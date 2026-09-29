#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "pricer/black_scholes.hpp"
#include "pricer/greeks.hpp"

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
