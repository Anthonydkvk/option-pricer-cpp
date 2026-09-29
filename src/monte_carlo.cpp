#include "pricer/monte_carlo.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>

namespace pricer {

MonteCarloResult monte_carlo_price(const Option& option, const MarketParams& market, const int paths,
                                   const std::uint64_t seed, const bool antithetic) {
    validate(option);
    validate(market);
    // Le MC « naïf » ne sait pas quand exercer : à une date t, la valeur de continuation dépend
    // du futur de la trajectoire. Extension possible : Longstaff-Schwartz (régression des
    // valeurs de continuation sur des fonctions du spot, par backward induction).
    if (option.style == ExerciseStyle::American) {
        throw std::invalid_argument(
            "Monte Carlo pricer supports European options only (see Longstaff-Schwartz for American)");
    }
    if (paths <= 0) {
        throw std::invalid_argument("paths must be > 0, got " + std::to_string(paths));
    }
    if (antithetic && paths % 2 != 0) {
        throw std::invalid_argument("paths must be even with antithetic variates, got "
                                    + std::to_string(paths));
    }

    const double drift = (market.r - market.q - 0.5 * market.sigma * market.sigma) * option.T;
    const double vol = market.sigma * std::sqrt(option.T);
    const bool is_call = option.type == OptionType::Call;

    const auto payoff_at_maturity = [&](const double z) {
        const double spot = market.S * std::exp(drift + vol * z);
        return is_call ? std::max(spot - option.K, 0.0) : std::max(option.K - spot, 0.0);
    };

    std::mt19937_64 rng(seed);
    std::normal_distribution<double> normal(0.0, 1.0);

    // En antithétique, l'échantillon i.i.d. est la moyenne de la paire (f(Z) + f(-Z)) / 2 :
    // les deux membres d'une paire ne sont pas indépendants, on ne peut donc pas les compter séparément.
    const int samples = antithetic ? paths / 2 : paths;
    double sum = 0.0;
    double sum_sq = 0.0;
    for (int i = 0; i < samples; ++i) {
        const double z = normal(rng);
        const double x = antithetic ? 0.5 * (payoff_at_maturity(z) + payoff_at_maturity(-z))
                                    : payoff_at_maturity(z);
        sum += x;
        sum_sq += x * x;
    }

    const double n = static_cast<double>(samples);
    const double mean = sum / n;
    // Variance empirique non biaisée (division par n - 1) ; 0 si un seul échantillon.
    const double variance = samples > 1 ? std::max((sum_sq - n * mean * mean) / (n - 1.0), 0.0) : 0.0;

    const double discount = std::exp(-market.r * option.T);
    const double price = discount * mean;
    const double std_error = discount * std::sqrt(variance / n);
    constexpr double z_95 = 1.959963984540054;  // quantile 97.5 % de la loi normale
    return MonteCarloResult{price, std_error, price - z_95 * std_error, price + z_95 * std_error};
}

}  // namespace pricer
