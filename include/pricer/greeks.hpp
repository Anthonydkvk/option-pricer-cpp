#pragma once

#include <functional>

#include "pricer/option.hpp"

namespace pricer {

// Sensibilités du prix. Conventions (voir CLAUDE.md) :
// vega et rho pour une variation de 1.0 de sigma / r (diviser par 100 pour « par 1 % »),
// theta par an (diviser par 365 pour « par jour »).
struct Greeks {
    double delta;
    double gamma;
    double vega;
    double theta;
    double rho;
};

// N'importe quelle méthode de pricing vue comme « (option, marché) -> prix ».
// Les paramètres propres à la méthode (nombre de pas, trajectoires, seed...) sont capturés
// par une lambda, par exemple :
//   [](const Option& o, const MarketParams& m) { return binomial_price(o, m, 500); }
using PricingFunction = std::function<double(const Option&, const MarketParams&)>;

// Tailles des pas (« bumps »). Valeurs par défaut = CLAUDE.md.
// spot_relative est relatif (h_S = spot_relative * S), les autres sont absolus.
struct BumpSizes {
    double spot_relative = 1e-4;
    double sigma = 1e-4;
    double r = 1e-4;
    double T = 1e-4;
};

// Grecques par différences finies centrées (« bump & reprice ») :
//   delta = (P(S+h) - P(S-h)) / 2h           gamma = (P(S+h) - 2 P(S) + P(S-h)) / h²
//   vega  = (P(sigma+h) - P(sigma-h)) / 2h   rho   = (P(r+h) - P(r-h)) / 2h
//   theta = -(P(T+h) - P(T-h)) / 2h   (le temps qui passe réduit la maturité restante)
// 9 appels à `price` au total. Pour une méthode aléatoire (Monte Carlo), `price` doit utiliser
// la même seed à chaque appel (common random numbers), sinon le bruit domine les différences.
// Lève std::invalid_argument si un pas est <= 0, ou si un prix bumpé est invalide
// (par exemple T - h_T <= 0 ou sigma - h_sigma <= 0).
Greeks finite_difference_greeks(const PricingFunction& price, const Option& option,
                                const MarketParams& market, const BumpSizes& bumps = {});

}  // namespace pricer
