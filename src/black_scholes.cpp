#include "pricer/black_scholes.hpp"

#include <cmath>
#include <stdexcept>

#include "pricer/normal.hpp"

namespace pricer {

namespace {

// Quantités communes au prix et aux grecques, calculées une seule fois.
struct Terms {
    double d1;
    double d2;
    double sqrt_T;
    double df_r;   // e^{-rT} : actualisation du strike
    double df_q;   // e^{-qT} : effet du dividende sur le spot
};

Terms compute_terms(const Option& option, const MarketParams& market) {
    validate(option);
    validate(market);
    if (option.style != ExerciseStyle::European) {
        throw std::invalid_argument("Black-Scholes closed form only prices European options");
    }

    const double sqrt_T = std::sqrt(option.T);
    const double vol_sqrt_T = market.sigma * sqrt_T;
    const double d1 = (std::log(market.S / option.K)
                       + (market.r - market.q + 0.5 * market.sigma * market.sigma) * option.T)
                      / vol_sqrt_T;
    const double d2 = d1 - vol_sqrt_T;
    return {d1, d2, sqrt_T, std::exp(-market.r * option.T), std::exp(-market.q * option.T)};
}

}  // namespace

double black_scholes_price(const Option& option, const MarketParams& market) {
    const Terms t = compute_terms(option, market);
    const double S = market.S;
    const double K = option.K;

    if (option.type == OptionType::Call) {
        return S * t.df_q * normal_cdf(t.d1) - K * t.df_r * normal_cdf(t.d2);
    }
    return K * t.df_r * normal_cdf(-t.d2) - S * t.df_q * normal_cdf(-t.d1);
}

Greeks black_scholes_greeks(const Option& option, const MarketParams& market) {
    const Terms t = compute_terms(option, market);
    const double S = market.S;
    const double K = option.K;
    const double pdf_d1 = normal_pdf(t.d1);

    Greeks g{};
    // Gamma et Vega sont identiques pour le call et le put.
    g.gamma = t.df_q * pdf_d1 / (S * market.sigma * t.sqrt_T);
    g.vega = S * t.df_q * pdf_d1 * t.sqrt_T;

    // Terme de theta commun : perte de valeur temps liée à la volatilité.
    const double theta_vol = -S * t.df_q * pdf_d1 * market.sigma / (2.0 * t.sqrt_T);

    if (option.type == OptionType::Call) {
        g.delta = t.df_q * normal_cdf(t.d1);
        g.theta = theta_vol - market.r * K * t.df_r * normal_cdf(t.d2)
                  + market.q * S * t.df_q * normal_cdf(t.d1);
        g.rho = K * option.T * t.df_r * normal_cdf(t.d2);
    } else {
        g.delta = -t.df_q * normal_cdf(-t.d1);
        g.theta = theta_vol + market.r * K * t.df_r * normal_cdf(-t.d2)
                  - market.q * S * t.df_q * normal_cdf(-t.d1);
        g.rho = -K * option.T * t.df_r * normal_cdf(-t.d2);
    }
    return g;
}

}  // namespace pricer
