#pragma once

#include "pricer/greeks.hpp"
#include "pricer/option.hpp"

namespace pricer {

// Prix Black-Scholes (formule fermée). Options européennes uniquement :
// lève std::invalid_argument pour une américaine ou des entrées invalides.
double black_scholes_price(const Option& option, const MarketParams& market);

// Grecques analytiques Black-Scholes (mêmes restrictions que le prix).
Greeks black_scholes_greeks(const Option& option, const MarketParams& market);

}  // namespace pricer
