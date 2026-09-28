#include "pricer/binomial.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace pricer {

double binomial_price(const Option& option, const MarketParams& market, const int steps) {
    validate(option);
    validate(market);
    if (steps <= 0) {
        throw std::invalid_argument("steps must be > 0, got " + std::to_string(steps));
    }

    const double dt = option.T / steps;
    const double u = std::exp(market.sigma * std::sqrt(dt));
    const double d = 1.0 / u;
    const double p = (std::exp((market.r - market.q) * dt) - d) / (u - d);
    if (!(p > 0.0 && p < 1.0)) {
        throw std::invalid_argument("CRR risk-neutral probability p = " + std::to_string(p)
                                    + " is outside ]0, 1[: increase the number of steps");
    }
    const double discount = std::exp(-market.r * dt);
    const bool is_call = option.type == OptionType::Call;
    const bool is_american = option.style == ExerciseStyle::American;

    const auto payoff = [&](const double spot) {
        return is_call ? std::max(spot - option.K, 0.0) : std::max(option.K - spot, 0.0);
    };

    // À maturité, le nœud j (j hausses sur `steps` pas) vaut S * u^j * d^(steps - j) = S * u^(2j - steps).
    const auto n = static_cast<std::size_t>(steps);
    std::vector<double> spot(n + 1);
    std::vector<double> value(n + 1);
    for (std::size_t j = 0; j <= n; ++j) {
        spot[j] = market.S * std::pow(u, 2.0 * static_cast<double>(j) - steps);
        value[j] = payoff(spot[j]);
    }

    // Backward induction : le niveau i n'a besoin que du niveau i + 1, on écrase donc le même vecteur.
    // En parcourant j croissant, value[j + 1] n'a pas encore été écrasé quand on l'utilise.
    for (std::size_t i = n; i-- > 0;) {
        for (std::size_t j = 0; j <= i; ++j) {
            const double continuation = discount * (p * value[j + 1] + (1.0 - p) * value[j]);
            if (is_american) {
                spot[j] *= u;  // nœud (i, j) = nœud (i + 1, j) / d = nœud (i + 1, j) * u
                value[j] = std::max(continuation, payoff(spot[j]));
            } else {
                value[j] = continuation;
            }
        }
    }
    return value[0];
}

}  // namespace pricer
