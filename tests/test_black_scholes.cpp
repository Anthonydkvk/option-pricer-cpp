#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "pricer/black_scholes.hpp"

using namespace pricer;

namespace {

// Cas standard de CLAUDE.md : S=100, K=100, r=0.05, q=0, sigma=0.2, T=1.
const MarketParams standard_market{100.0, 0.05, 0.0, 0.2};
const Option standard_call{OptionType::Call, ExerciseStyle::European, 100.0, 1.0};
const Option standard_put{OptionType::Put, ExerciseStyle::European, 100.0, 1.0};

}  // namespace

// ---------------------------------------------------------------------------
// Valeurs de référence (CLAUDE.md)
// ---------------------------------------------------------------------------
TEST(BlackScholes, ReferencePrices) {
    EXPECT_NEAR(black_scholes_price(standard_call, standard_market), 10.450584, 1e-6);
    EXPECT_NEAR(black_scholes_price(standard_put, standard_market), 5.573526, 1e-6);
}

TEST(BlackScholes, ReferenceCallGreeks) {
    const Greeks g = black_scholes_greeks(standard_call, standard_market);
    EXPECT_NEAR(g.delta, 0.636831, 1e-6);
    EXPECT_NEAR(g.gamma, 0.018762, 1e-6);
    EXPECT_NEAR(g.vega, 37.524035, 1e-5);
    EXPECT_NEAR(g.theta, -6.414028, 1e-5);
    EXPECT_NEAR(g.rho, 53.232482, 1e-5);
}

// ---------------------------------------------------------------------------
// Parité call-put : C - P = S e^{-qT} - K e^{-rT}, y compris avec dividende
// ---------------------------------------------------------------------------
TEST(BlackScholes, PutCallParity) {
    for (const double q : {0.0, 0.03}) {
        for (const double K : {80.0, 100.0, 120.0}) {
            for (const double T : {0.25, 1.0, 3.0}) {
                const MarketParams m{100.0, 0.05, q, 0.25};
                const Option call{OptionType::Call, ExerciseStyle::European, K, T};
                const Option put{OptionType::Put, ExerciseStyle::European, K, T};

                const double lhs = black_scholes_price(call, m) - black_scholes_price(put, m);
                const double rhs = m.S * std::exp(-q * T) - K * std::exp(-m.r * T);
                EXPECT_NEAR(lhs, rhs, 1e-10) << "q=" << q << " K=" << K << " T=" << T;
            }
        }
    }
}

// Dériver la parité donne des relations exactes entre grecques du call et du put.
TEST(BlackScholes, PutGreeksConsistentWithParity) {
    const MarketParams m{100.0, 0.05, 0.03, 0.25};
    const Option call{OptionType::Call, ExerciseStyle::European, 110.0, 2.0};
    const Option put{OptionType::Put, ExerciseStyle::European, 110.0, 2.0};
    const Greeks c = black_scholes_greeks(call, m);
    const Greeks p = black_scholes_greeks(put, m);

    const double df_q = std::exp(-m.q * call.T);
    const double df_r = std::exp(-m.r * call.T);

    EXPECT_NEAR(c.delta - p.delta, df_q, 1e-12);
    EXPECT_NEAR(c.gamma, p.gamma, 1e-12);
    EXPECT_NEAR(c.vega, p.vega, 1e-12);
    EXPECT_NEAR(c.rho - p.rho, call.K * call.T * df_r, 1e-10);
    // Theta = -d/dT de la parité : -(-q S e^{-qT} + r K e^{-rT}) = q S e^{-qT} - r K e^{-rT}
    EXPECT_NEAR(c.theta - p.theta, m.q * m.S * df_q - m.r * call.K * df_r, 1e-10);
}

// ---------------------------------------------------------------------------
// Bon sens financier
// ---------------------------------------------------------------------------
TEST(BlackScholes, GreeksSigns) {
    const Greeks c = black_scholes_greeks(standard_call, standard_market);
    const Greeks p = black_scholes_greeks(standard_put, standard_market);
    EXPECT_GT(c.delta, 0.0);
    EXPECT_LT(c.delta, 1.0);
    EXPECT_LT(p.delta, 0.0);
    EXPECT_GT(p.delta, -1.0);
    EXPECT_GT(c.gamma, 0.0);
    EXPECT_GT(c.vega, 0.0);
    EXPECT_LT(c.theta, 0.0);  // un call ATM perd de la valeur avec le temps
    EXPECT_GT(c.rho, 0.0);
    EXPECT_LT(p.rho, 0.0);
}

TEST(BlackScholes, PriceWithinNoArbitrageBounds) {
    // max(S e^{-qT} - K e^{-rT}, 0) <= C <= S e^{-qT}
    const MarketParams m{100.0, 0.05, 0.02, 0.3};
    for (const double K : {50.0, 100.0, 150.0}) {
        const Option call{OptionType::Call, ExerciseStyle::European, K, 1.0};
        const double c = black_scholes_price(call, m);
        const double forward_intrinsic = m.S * std::exp(-m.q) - K * std::exp(-m.r);
        EXPECT_GE(c, std::max(forward_intrinsic, 0.0));
        EXPECT_LE(c, m.S * std::exp(-m.q));
    }
}

// ---------------------------------------------------------------------------
// Entrées invalides
// ---------------------------------------------------------------------------
TEST(BlackScholes, RejectsAmericanOptions) {
    const Option american{OptionType::Put, ExerciseStyle::American, 100.0, 1.0};
    EXPECT_THROW(black_scholes_price(american, standard_market), std::invalid_argument);
    EXPECT_THROW(black_scholes_greeks(american, standard_market), std::invalid_argument);
}

TEST(BlackScholes, RejectsInvalidInputs) {
    const MarketParams zero_vol{100.0, 0.05, 0.0, 0.0};
    const Option zero_maturity{OptionType::Call, ExerciseStyle::European, 100.0, 0.0};
    EXPECT_THROW(black_scholes_price(standard_call, zero_vol), std::invalid_argument);
    EXPECT_THROW(black_scholes_price(zero_maturity, standard_market), std::invalid_argument);
    EXPECT_THROW(black_scholes_greeks(standard_call, zero_vol), std::invalid_argument);
}
