#include "pricer/normal.hpp"

#include <cmath>

namespace pricer {

namespace {
constexpr double inv_sqrt_2pi = 0.39894228040143267794;  // 1 / sqrt(2π)
}

double normal_cdf(const double x) {
    // erfc est précis dans les queues (x très négatif), contrairement à 0.5 * (1 + erf(...)).
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double normal_pdf(const double x) {
    return inv_sqrt_2pi * std::exp(-0.5 * x * x);
}

}  // namespace pricer
