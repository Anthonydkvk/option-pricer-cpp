#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "pricer/greeks.hpp"
#include "pricer/monte_carlo.hpp"
#include "pricer/option.hpp"

namespace pricer {

// Tout ce que l'utilisateur peut régler en ligne de commande.
// Valeurs par défaut = cas standard de CLAUDE.md.
struct CliConfig {
    MarketParams market{100.0, 0.05, 0.0, 0.2};
    Option option{OptionType::Call, ExerciseStyle::European, 100.0, 1.0};
    int steps = 2001;             // arbre CRR : impair -> aucun nœud pile sur le strike
    double tree_spot_bump = 2e-2; // h_S relatif pour les grecques de l'arbre (voir README)
    int paths = 500'000;          // Monte Carlo
    std::uint64_t seed = 42;
    bool antithetic = false;
    bool csv = false;
    bool help = false;
};

// Lit les arguments (sans le nom du programme), au format « --nom valeur ».
// Lève std::invalid_argument pour une option inconnue, une valeur manquante ou mal écrite.
// Ne vérifie pas le sens financier (sigma <= 0...) : c'est le rôle des pricers.
CliConfig parse_args(const std::vector<std::string>& args);

// Texte d'aide affiché par --help.
std::string usage();

// Résultat d'une méthode : prix, grecques et temps de calcul (en microsecondes).
struct MethodResult {
    std::string id;     // identifiant court, stable, pour le CSV (bs_analytic, crr...)
    std::string label;  // libellé lisible pour le tableau
    double price;
    Greeks greeks;
    double price_us;    // temps d'un pricing
    double greeks_us;   // temps du calcul des 5 grecques
    std::optional<MonteCarloResult> monte_carlo;  // erreur standard et IC, Monte Carlo uniquement
};

// Lance toutes les méthodes applicables à l'option (Black-Scholes et Monte Carlo : européennes
// uniquement). Lève std::invalid_argument si les paramètres sont invalides.
std::vector<MethodResult> run_all(const CliConfig& config);

// Tableau aligné pour un humain.
void print_table(std::ostream& out, const CliConfig& config,
                 const std::vector<MethodResult>& results);

// Même contenu en CSV (une ligne d'en-tête, puis une ligne par méthode), pleine précision.
void print_csv(std::ostream& out, const std::vector<MethodResult>& results);

}  // namespace pricer
