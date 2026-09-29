#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "pricer/black_scholes.hpp"
#include "pricer/monte_carlo.hpp"

using namespace pricer;

namespace {

const MarketParams standard_market{100.0, 0.05, 0.0, 0.2};
constexpr std::uint64_t seed = 42;

Option european(const OptionType type, const double K = 100.0) {
    return Option{type, ExerciseStyle::European, K, 1.0};
}

}  // namespace

// ---------------------------------------------------------------------------
// Précision : le prix MC doit être à moins de 3 erreurs standard du prix exact
// ---------------------------------------------------------------------------
TEST(MonteCarlo, MatchesBlackScholesWithin3StdErrors) {
    for (const OptionType type : {OptionType::Call, OptionType::Put}) {
        const Option opt = european(type);
        const MonteCarloResult mc = monte_carlo_price(opt, standard_market, 500'000, seed);
        const double bs = black_scholes_price(opt, standard_market);
        EXPECT_LT(std::abs(mc.price - bs), 3.0 * mc.std_error);
        EXPECT_GT(mc.std_error, 0.0);
    }
}

TEST(MonteCarlo, ConfidenceIntervalIsCenteredAndContainsBlackScholes) {
    const Option call = european(OptionType::Call);
    const MonteCarloResult mc = monte_carlo_price(call, standard_market, 500'000, seed);
    EXPECT_NEAR(mc.ci_high - mc.price, 1.96 * mc.std_error, 1e-3 * mc.std_error);
    EXPECT_NEAR(mc.price - mc.ci_low, 1.96 * mc.std_error, 1e-3 * mc.std_error);
    const double bs = black_scholes_price(call, standard_market);
    EXPECT_LT(mc.ci_low, bs);
    EXPECT_GT(mc.ci_high, bs);
}

// ---------------------------------------------------------------------------
// Vitesse de convergence en 1/sqrt(N) : 4x plus de trajectoires -> erreur standard divisée par 2
// ---------------------------------------------------------------------------
TEST(MonteCarlo, StdErrorScalesAsOneOverSqrtN) {
    const Option call = european(OptionType::Call);
    const double se_n = monte_carlo_price(call, standard_market, 50'000, seed).std_error;
    const double se_4n = monte_carlo_price(call, standard_market, 200'000, seed).std_error;
    EXPECT_NEAR(se_4n / se_n, 0.5, 0.05);
}

// ---------------------------------------------------------------------------
// Variables antithétiques : à nombre de trajectoires égal, erreur standard plus faible
// ---------------------------------------------------------------------------
TEST(MonteCarlo, AntitheticReducesStdError) {
    for (const OptionType type : {OptionType::Call, OptionType::Put}) {
        const Option opt = european(type);
        const MonteCarloResult plain = monte_carlo_price(opt, standard_market, 200'000, seed, false);
        const MonteCarloResult anti = monte_carlo_price(opt, standard_market, 200'000, seed, true);
        EXPECT_LT(anti.std_error, plain.std_error);
        EXPECT_LT(std::abs(anti.price - black_scholes_price(opt, standard_market)), 3.0 * anti.std_error);
    }
}

// ---------------------------------------------------------------------------
// Reproductibilité : même seed -> même résultat (indispensable pour les grecques MC)
// ---------------------------------------------------------------------------
TEST(MonteCarlo, SameSeedGivesSameResult) {
    const Option call = european(OptionType::Call);
    const MonteCarloResult a = monte_carlo_price(call, standard_market, 10'000, seed);
    const MonteCarloResult b = monte_carlo_price(call, standard_market, 10'000, seed);
    EXPECT_EQ(a.price, b.price);
    EXPECT_EQ(a.std_error, b.std_error);
    const MonteCarloResult c = monte_carlo_price(call, standard_market, 10'000, seed + 1);
    EXPECT_NE(a.price, c.price);
}

// ---------------------------------------------------------------------------
// Entrées invalides
// ---------------------------------------------------------------------------
TEST(MonteCarlo, RejectsAmericanOptions) {
    const Option american{OptionType::Put, ExerciseStyle::American, 100.0, 1.0};
    EXPECT_THROW(monte_carlo_price(american, standard_market, 1000, seed), std::invalid_argument);
}

TEST(MonteCarlo, RejectsInvalidInputs) {
    const Option call = european(OptionType::Call);
    EXPECT_THROW(monte_carlo_price(call, standard_market, 0, seed), std::invalid_argument);
    EXPECT_THROW(monte_carlo_price(call, standard_market, -10, seed), std::invalid_argument);
    EXPECT_THROW(monte_carlo_price(call, standard_market, 1001, seed, true), std::invalid_argument);
    EXPECT_THROW(monte_carlo_price(call, MarketParams{100.0, 0.05, 0.0, 0.0}, 1000, seed),
                 std::invalid_argument);
    EXPECT_THROW(monte_carlo_price(european(OptionType::Put, -1.0), standard_market, 1000, seed),
                 std::invalid_argument);
}
