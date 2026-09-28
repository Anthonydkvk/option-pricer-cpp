#pragma once

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

}  // namespace pricer
