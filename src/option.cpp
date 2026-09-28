#include "pricer/option.hpp"

#include <stdexcept>
#include <string>

namespace pricer {

namespace {

// !(x > 0) plutôt que x <= 0 : rejette aussi NaN (toute comparaison avec NaN est fausse).
void require_positive(const double value, const char* name) {
    if (!(value > 0.0)) {
        throw std::invalid_argument(std::string(name) + " must be > 0, got " + std::to_string(value));
    }
}

}  // namespace

void validate(const MarketParams& market) {
    require_positive(market.S, "S");
    require_positive(market.sigma, "sigma");
}

void validate(const Option& option) {
    require_positive(option.K, "K");
    require_positive(option.T, "T");
}

}  // namespace pricer
