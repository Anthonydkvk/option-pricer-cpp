#pragma once

namespace pricer {

enum class OptionType { Call, Put };

enum class ExerciseStyle { European, American };

// Données de marché, communes à toutes les options sur le même sous-jacent.
struct MarketParams {
    double S;          // spot
    double r;          // taux sans risque continu
    double q = 0.0;    // dividende continu
    double sigma;      // volatilité annuelle
};

// Caractéristiques du contrat.
struct Option {
    OptionType type;
    ExerciseStyle style;
    double K;          // strike
    double T;          // maturité en années
};

// Lèvent std::invalid_argument si une entrée est invalide (≤ 0 ou NaN).
void validate(const MarketParams& market);
void validate(const Option& option);

}  // namespace pricer
