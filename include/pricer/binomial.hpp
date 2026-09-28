#pragma once

#include "pricer/option.hpp"

namespace pricer {

// Prix par arbre binomial de Cox-Ross-Rubinstein (CRR), européennes et américaines.
//   u = e^{sigma * sqrt(dt)}, d = 1 / u, p = (e^{(r - q) dt} - d) / (u - d)
// Complexité : O(steps²) en temps, O(steps) en mémoire.
// Lève std::invalid_argument si steps <= 0, si les entrées sont invalides,
// ou si p n'est pas dans ]0, 1[ (pas de temps trop grand).
double binomial_price(const Option& option, const MarketParams& market, int steps);

}  // namespace pricer
