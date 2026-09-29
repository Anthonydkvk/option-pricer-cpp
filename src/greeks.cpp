#include "pricer/greeks.hpp"

#include <stdexcept>
#include <string>

namespace pricer {

namespace {

void require_positive_bump(const double value, const char* name) {
    if (!(value > 0.0)) {
        throw std::invalid_argument(std::string("bump ") + name + " must be > 0, got "
                                    + std::to_string(value));
    }
}

}  // namespace

Greeks finite_difference_greeks(const PricingFunction& price, const Option& option,
                                const MarketParams& market, const BumpSizes& bumps) {
    validate(option);
    validate(market);
    require_positive_bump(bumps.spot_relative, "spot_relative");
    require_positive_bump(bumps.sigma, "sigma");
    require_positive_bump(bumps.r, "r");
    require_positive_bump(bumps.T, "T");

    // Copie du marché (ou de l'option) avec un seul champ modifié : on passe le membre
    // à bumper en paramètre (pointeur vers membre), pour ne pas répéter 8 fois la même copie.
    const auto price_bumped_market = [&](double MarketParams::*field, const double shift) {
        MarketParams bumped = market;
        bumped.*field += shift;
        return price(option, bumped);
    };
    const auto price_bumped_option = [&](double Option::*field, const double shift) {
        Option bumped = option;
        bumped.*field += shift;
        return price(bumped, market);
    };

    const double base = price(option, market);

    const double h_S = bumps.spot_relative * market.S;
    const double up_S = price_bumped_market(&MarketParams::S, +h_S);
    const double down_S = price_bumped_market(&MarketParams::S, -h_S);

    const double h_sigma = bumps.sigma;
    const double up_sigma = price_bumped_market(&MarketParams::sigma, +h_sigma);
    const double down_sigma = price_bumped_market(&MarketParams::sigma, -h_sigma);

    const double h_r = bumps.r;
    const double up_r = price_bumped_market(&MarketParams::r, +h_r);
    const double down_r = price_bumped_market(&MarketParams::r, -h_r);

    const double h_T = bumps.T;
    const double up_T = price_bumped_option(&Option::T, +h_T);
    const double down_T = price_bumped_option(&Option::T, -h_T);

    return Greeks{
        (up_S - down_S) / (2.0 * h_S),
        (up_S - 2.0 * base + down_S) / (h_S * h_S),
        (up_sigma - down_sigma) / (2.0 * h_sigma),
        -(up_T - down_T) / (2.0 * h_T),
        (up_r - down_r) / (2.0 * h_r),
    };
}

}  // namespace pricer
