#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "pricer/binomial.hpp"
#include "pricer/black_scholes.hpp"

using namespace pricer;

namespace {

const MarketParams standard_market{100.0, 0.05, 0.0, 0.2};

Option make(const OptionType type, const ExerciseStyle style, const double K = 100.0, const double T = 1.0) {
    return Option{type, style, K, T};
}

}  // namespace

// ---------------------------------------------------------------------------
// Convergence vers Black-Scholes
// ---------------------------------------------------------------------------
TEST(Binomial, ConvergesToBlackScholesAt500Steps) {
    for (const OptionType type : {OptionType::Call, OptionType::Put}) {
        const Option opt = make(type, ExerciseStyle::European);
        const double error = std::abs(binomial_price(opt, standard_market, 500)
                                      - black_scholes_price(opt, standard_market));
        EXPECT_LT(error, 1e-2);
    }
}

// L'erreur oscille entre pas pairs et impairs (position du strike par rapport aux nœuds) :
// on compare donc des nombres de pas de même parité.
TEST(Binomial, ErrorDecreasesWithSteps) {
    const Option call = make(OptionType::Call, ExerciseStyle::European);
    const double bs = black_scholes_price(call, standard_market);
    double previous_error = 1e9;
    for (const int steps : {50, 100, 200, 400, 800, 1600}) {
        const double error = std::abs(binomial_price(call, standard_market, steps) - bs);
        EXPECT_LT(error, previous_error) << "steps=" << steps;
        previous_error = error;
    }
}

// ---------------------------------------------------------------------------
// Américain
// ---------------------------------------------------------------------------
TEST(Binomial, AmericanPutReference) {
    const Option put = make(OptionType::Put, ExerciseStyle::American);
    EXPECT_NEAR(binomial_price(put, standard_market, 5000), 6.0902, 1e-3);
}

TEST(Binomial, AmericanWorthAtLeastEuropean) {
    const MarketParams m{100.0, 0.05, 0.03, 0.25};
    for (const OptionType type : {OptionType::Call, OptionType::Put}) {
        for (const double K : {80.0, 100.0, 120.0}) {
            const double american = binomial_price(make(type, ExerciseStyle::American, K), m, 300);
            const double european = binomial_price(make(type, ExerciseStyle::European, K), m, 300);
            EXPECT_GE(american, european - 1e-12) << "K=" << K;
        }
    }
}

// Sans dividende, exercer un call tôt n'est jamais optimal : américain = européen.
TEST(Binomial, AmericanCallWithoutDividendEqualsEuropean) {
    for (const double K : {80.0, 100.0, 120.0}) {
        const double american = binomial_price(make(OptionType::Call, ExerciseStyle::American, K), standard_market, 1000);
        const double european = binomial_price(make(OptionType::Call, ExerciseStyle::European, K), standard_market, 1000);
        EXPECT_NEAR(american, european, 1e-3) << "K=" << K;
    }
}

// Avec un dividende, l'exercice anticipé du call devient intéressant (pour toucher le dividende).
TEST(Binomial, AmericanCallWithDividendWorthMore) {
    const MarketParams m{100.0, 0.05, 0.08, 0.2};
    const double american = binomial_price(make(OptionType::Call, ExerciseStyle::American, 90.0, 2.0), m, 500);
    const double european = binomial_price(make(OptionType::Call, ExerciseStyle::European, 90.0, 2.0), m, 500);
    EXPECT_GT(american, european + 0.01);
}

// Le put américain vaut au moins son exercice immédiat, même très dans la monnaie.
TEST(Binomial, AmericanPutAtLeastIntrinsic) {
    const MarketParams m{60.0, 0.05, 0.0, 0.2};
    const Option put = make(OptionType::Put, ExerciseStyle::American);
    EXPECT_NEAR(binomial_price(put, m, 500), 40.0, 1e-9);   // exercer tout de suite est optimal
}

// ---------------------------------------------------------------------------
// Parité call-put (européen) : exacte dans l'arbre CRR, par construction de p
// ---------------------------------------------------------------------------
TEST(Binomial, EuropeanPutCallParity) {
    for (const double q : {0.0, 0.03}) {
        for (const double K : {80.0, 100.0, 120.0}) {
            const MarketParams m{100.0, 0.05, q, 0.25};
            const double c = binomial_price(make(OptionType::Call, ExerciseStyle::European, K, 2.0), m, 400);
            const double p = binomial_price(make(OptionType::Put, ExerciseStyle::European, K, 2.0), m, 400);
            const double rhs = m.S * std::exp(-q * 2.0) - K * std::exp(-m.r * 2.0);
            EXPECT_NEAR(c - p, rhs, 1e-9) << "q=" << q << " K=" << K;
        }
    }
}

// ---------------------------------------------------------------------------
// Entrées invalides
// ---------------------------------------------------------------------------
TEST(Binomial, RejectsNonPositiveSteps) {
    const Option call = make(OptionType::Call, ExerciseStyle::European);
    EXPECT_THROW(binomial_price(call, standard_market, 0), std::invalid_argument);
    EXPECT_THROW(binomial_price(call, standard_market, -10), std::invalid_argument);
}

TEST(Binomial, RejectsInvalidInputs) {
    const Option call = make(OptionType::Call, ExerciseStyle::European);
    EXPECT_THROW(binomial_price(call, MarketParams{100.0, 0.05, 0.0, 0.0}, 100), std::invalid_argument);
    EXPECT_THROW(binomial_price(make(OptionType::Call, ExerciseStyle::European, 100.0, 0.0), standard_market, 100),
                 std::invalid_argument);
}

// Un seul pas avec un taux élevé et une vol minuscule : e^{r dt} > u, donc p > 1.
TEST(Binomial, RejectsProbabilityOutsideUnitInterval) {
    const MarketParams m{100.0, 0.10, 0.0, 0.01};
    const Option call = make(OptionType::Call, ExerciseStyle::European);
    EXPECT_THROW(binomial_price(call, m, 1), std::invalid_argument);
    EXPECT_NO_THROW(binomial_price(call, m, 1000));  // avec des pas plus fins, p redevient valide
}
