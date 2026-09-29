#pragma once

#include <cstdint>

#include "pricer/option.hpp"

namespace pricer {

// Résultat d'un pricing Monte Carlo : l'estimation seule ne suffit pas,
// il faut aussi savoir à quel point elle est précise.
struct MonteCarloResult {
    double price;      // moyenne des payoffs actualisés
    double std_error;  // écart-type de l'estimateur = écart-type des payoffs / sqrt(nombre d'échantillons)
    double ci_low;     // intervalle de confiance à 95 % : price ± 1.96 * std_error
    double ci_high;
};

// Prix Monte Carlo d'une option européenne sous GBM, par simulation exacte à maturité :
//   S_T = S * exp((r - q - sigma²/2) T + sigma sqrt(T) Z),  Z ~ N(0, 1)
// `paths` = nombre total de trajectoires simulées (= nombre de payoffs calculés).
// `seed` fixe le générateur (std::mt19937_64) : même seed -> mêmes Z -> même résultat.
// `antithetic` : chaque tirage Z est apparié à -Z (paths / 2 paires, paths doit être pair).
// Lève std::invalid_argument pour une américaine, des entrées invalides, paths <= 0,
// ou paths impair en mode antithétique.
MonteCarloResult monte_carlo_price(const Option& option, const MarketParams& market, int paths,
                                   std::uint64_t seed, bool antithetic = false);

}  // namespace pricer
