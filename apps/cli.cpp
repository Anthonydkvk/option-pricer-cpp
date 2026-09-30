#include "cli.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "pricer/binomial.hpp"
#include "pricer/black_scholes.hpp"
#include "pricer/version.hpp"

namespace pricer {

namespace {

// Le texte entier doit être un nombre : std::stod("10x") renverrait 10 sans erreur,
// d'où la vérification de `pos` (nombre de caractères effectivement lus).
double parse_double(const std::string& flag, const std::string& text) {
    std::size_t pos = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos == 0 || pos != text.size()) {
        throw std::invalid_argument(flag + ": not a number: '" + text + "'");
    }
    return value;
}

int parse_int(const std::string& flag, const std::string& text) {
    std::size_t pos = 0;
    int value = 0;
    try {
        value = std::stoi(text, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos == 0 || pos != text.size()) {
        throw std::invalid_argument(flag + ": not an integer: '" + text + "'");
    }
    return value;
}

std::uint64_t parse_seed(const std::string& flag, const std::string& text) {
    // std::stoull accepte "-1" et le transforme en un énorme nombre positif : on le refuse.
    if (text.empty() || text[0] == '-') {
        throw std::invalid_argument(flag + ": not a non-negative integer: '" + text + "'");
    }
    std::size_t pos = 0;
    std::uint64_t value = 0;
    try {
        value = std::stoull(text, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos == 0 || pos != text.size()) {
        throw std::invalid_argument(flag + ": not a non-negative integer: '" + text + "'");
    }
    return value;
}

// Exécute f(), stocke sa durée en microsecondes dans `elapsed_us` et renvoie son résultat.
// steady_clock : horloge monotone, l'équivalent de time.perf_counter() en Python.
template <typename F>
auto timed(F&& f, double& elapsed_us) {
    const auto start = std::chrono::steady_clock::now();
    auto result = f();
    const auto stop = std::chrono::steady_clock::now();
    elapsed_us = std::chrono::duration<double, std::micro>(stop - start).count();
    return result;
}

bool is_european(const Option& option) {
    return option.style == ExerciseStyle::European;
}

std::string describe(const Option& option) {
    const std::string style = is_european(option) ? "European" : "American";
    const std::string type = option.type == OptionType::Call ? "call" : "put";
    return style + " " + type;
}

}  // namespace

CliConfig parse_args(const std::vector<std::string>& args) {
    CliConfig config;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& flag = args[i];
        // Renvoie l'argument suivant (la valeur de l'option) et avance l'indice.
        const auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size()) {
                throw std::invalid_argument(flag + " expects a value");
            }
            return args[++i];
        };

        if (flag == "--help" || flag == "-h") {
            config.help = true;
        } else if (flag == "--csv") {
            config.csv = true;
        } else if (flag == "--antithetic") {
            config.antithetic = true;
        } else if (flag == "--S") {
            config.market.S = parse_double(flag, value());
        } else if (flag == "--K") {
            config.option.K = parse_double(flag, value());
        } else if (flag == "--r") {
            config.market.r = parse_double(flag, value());
        } else if (flag == "--q") {
            config.market.q = parse_double(flag, value());
        } else if (flag == "--sigma") {
            config.market.sigma = parse_double(flag, value());
        } else if (flag == "--T") {
            config.option.T = parse_double(flag, value());
        } else if (flag == "--type") {
            const std::string& type = value();
            if (type == "call") {
                config.option.type = OptionType::Call;
            } else if (type == "put") {
                config.option.type = OptionType::Put;
            } else {
                throw std::invalid_argument("--type: expected 'call' or 'put', got '" + type + "'");
            }
        } else if (flag == "--style") {
            const std::string& style = value();
            if (style == "european") {
                config.option.style = ExerciseStyle::European;
            } else if (style == "american") {
                config.option.style = ExerciseStyle::American;
            } else {
                throw std::invalid_argument("--style: expected 'european' or 'american', got '" +
                                            style + "'");
            }
        } else if (flag == "--steps") {
            config.steps = parse_int(flag, value());
        } else if (flag == "--tree-bump") {
            config.tree_spot_bump = parse_double(flag, value());
        } else if (flag == "--paths") {
            config.paths = parse_int(flag, value());
        } else if (flag == "--seed") {
            config.seed = parse_seed(flag, value());
        } else {
            throw std::invalid_argument("unknown option: '" + flag + "'");
        }
    }
    return config;
}

std::string usage() {
    return "Usage: pricer_cli [options]\n"
           "\n"
           "Option and market (defaults: standard case of the test suite)\n"
           "  --S <x>            spot                              (100)\n"
           "  --K <x>            strike                            (100)\n"
           "  --r <x>            continuous risk-free rate         (0.05)\n"
           "  --q <x>            continuous dividend yield         (0)\n"
           "  --sigma <x>        annual volatility                 (0.2)\n"
           "  --T <x>            maturity in years                 (1)\n"
           "  --type <call|put>                                    (call)\n"
           "  --style <european|american>                          (european)\n"
           "\n"
           "Methods\n"
           "  --steps <n>        CRR tree steps, odd is better     (2001)\n"
           "  --tree-bump <x>    relative spot bump for tree Greeks (0.02)\n"
           "  --paths <n>        Monte Carlo paths                 (500000)\n"
           "  --seed <n>         Monte Carlo seed                  (42)\n"
           "  --antithetic       Monte Carlo antithetic variates\n"
           "\n"
           "Output\n"
           "  --csv              machine-readable CSV instead of the table\n"
           "  -h, --help         show this help\n";
}

std::vector<MethodResult> run_all(const CliConfig& config) {
    const Option& option = config.option;
    const MarketParams& market = config.market;
    validate(market);
    validate(option);

    std::vector<MethodResult> results;

    if (is_european(option)) {
        // 1. Black-Scholes, grecques analytiques (formules fermées).
        MethodResult bs{"bs_analytic", "Black-Scholes (analytic)", 0.0, {}, 0.0, 0.0, std::nullopt};
        bs.price = timed([&] { return black_scholes_price(option, market); }, bs.price_us);
        bs.greeks = timed([&] { return black_scholes_greeks(option, market); }, bs.greeks_us);
        results.push_back(bs);

        // 2. Black-Scholes, grecques par différences finies : vérifie la méthode générique
        //    sur un cas où la réponse exacte est connue (ligne précédente).
        const PricingFunction bs_price = [](const Option& o, const MarketParams& m) {
            return black_scholes_price(o, m);
        };
        MethodResult bs_fd{"bs_fd", "Black-Scholes (finite diff.)", bs.price, {}, bs.price_us,
                           0.0, std::nullopt};
        bs_fd.greeks = timed([&] { return finite_difference_greeks(bs_price, option, market); },
                             bs_fd.greeks_us);
        results.push_back(bs_fd);
    }

    // 3. Arbre CRR (européennes et américaines). Pas en S plus grand que l'écart entre nœuds,
    //    sinon le Gamma mesure un coude de l'arbre au lieu de la courbure (voir README).
    const int steps = config.steps;
    const PricingFunction crr = [steps](const Option& o, const MarketParams& m) {
        return binomial_price(o, m, steps);
    };
    BumpSizes tree_bumps;
    tree_bumps.spot_relative = config.tree_spot_bump;
    MethodResult tree{"crr", "CRR tree (" + std::to_string(steps) + " steps)", 0.0, {}, 0.0, 0.0,
                      std::nullopt};
    tree.price = timed([&] { return crr(option, market); }, tree.price_us);
    tree.greeks = timed([&] { return finite_difference_greeks(crr, option, market, tree_bumps); },
                        tree.greeks_us);
    results.push_back(tree);

    if (is_european(option)) {
        // 4. Monte Carlo. La lambda capture la seed : les 9 prix bumpés réutilisent exactement
        //    les mêmes nombres aléatoires (common random numbers).
        const int paths = config.paths;
        const std::uint64_t seed = config.seed;
        const bool antithetic = config.antithetic;
        const PricingFunction mc_price = [=](const Option& o, const MarketParams& m) {
            return monte_carlo_price(o, m, paths, seed, antithetic).price;
        };
        MethodResult mc{"mc", "Monte Carlo (" + std::to_string(paths) + " paths)", 0.0, {}, 0.0,
                        0.0, std::nullopt};
        mc.monte_carlo = timed([&] { return monte_carlo_price(option, market, paths, seed,
                                                              antithetic); },
                               mc.price_us);
        mc.price = mc.monte_carlo->price;
        mc.greeks = timed([&] { return finite_difference_greeks(mc_price, option, market); },
                          mc.greeks_us);
        results.push_back(mc);
    }

    return results;
}

void print_table(std::ostream& out, const CliConfig& config,
                 const std::vector<MethodResult>& results) {
    const MarketParams& m = config.market;
    const Option& o = config.option;
    out << "option-pricer " << version() << "\n"
        << describe(o) << "  S=" << m.S << "  K=" << o.K << "  r=" << m.r << "  q=" << m.q
        << "  sigma=" << m.sigma << "  T=" << o.T << "\n\n";

    constexpr int label_width = 30;
    constexpr int number_width = 11;
    constexpr int time_width = 13;

    out << std::left << std::setw(label_width) << "Method" << std::right;
    for (const char* name : {"Price", "Std err", "Delta", "Gamma", "Vega", "Theta", "Rho"}) {
        out << std::setw(number_width) << name;
    }
    out << std::setw(time_width) << "Time price" << std::setw(time_width) << "Time greeks"
        << "\n";
    out << std::string(label_width + 7 * number_width + 2 * time_width, '-') << "\n";

    for (const MethodResult& r : results) {
        out << std::left << std::setw(label_width) << r.label << std::right << std::fixed
            << std::setprecision(6) << std::setw(number_width) << r.price;
        if (r.monte_carlo) {
            out << std::setw(number_width) << r.monte_carlo->std_error;
        } else {
            out << std::setw(number_width) << "-";
        }
        const Greeks& g = r.greeks;
        for (const double value : {g.delta, g.gamma, g.vega, g.theta, g.rho}) {
            out << std::setw(number_width) << value;
        }
        out << std::setprecision(1) << std::setw(time_width) << r.price_us
            << std::setw(time_width) << r.greeks_us << "\n";
    }
    out << std::defaultfloat << std::setprecision(6);

    for (const MethodResult& r : results) {
        if (r.monte_carlo) {
            out << "\nMonte Carlo: seed " << config.seed
                << (config.antithetic ? ", antithetic variates" : "") << ", 95 % CI ["
                << std::fixed << std::setprecision(6) << r.monte_carlo->ci_low << ", "
                << r.monte_carlo->ci_high << "]" << std::defaultfloat << "\n";
        }
    }
    if (!is_european(o)) {
        out << "\nBlack-Scholes and Monte Carlo price European options only: CRR tree only.\n";
    }
    out << "\nVega and Rho per 1.0 change of sigma and r (divide by 100 for 1 %).\n"
           "Theta per year (divide by 365 for one day).\n"
           "Times in microseconds, single run (use a Release build).\n"
           "Tree Greeks use a spot bump of "
        << config.tree_spot_bump * 100.0 << " % of S; the other methods use 0.01 %.\n";
}

void print_csv(std::ostream& out, const std::vector<MethodResult>& results) {
    out << "method,price,std_error,delta,gamma,vega,theta,rho,price_us,greeks_us\n";
    // 12 chiffres significatifs : assez pour comparer au 1e-10 près avec la référence Python.
    std::ostringstream line;
    for (const MethodResult& r : results) {
        const Greeks& g = r.greeks;
        line.str("");
        line << std::setprecision(12) << r.id << ',' << r.price << ',';
        if (r.monte_carlo) {
            line << r.monte_carlo->std_error;  // vide sinon : pas d'erreur statistique
        }
        line << ',' << g.delta << ',' << g.gamma << ',' << g.vega << ',' << g.theta << ','
             << g.rho << ',' << r.price_us << ',' << r.greeks_us << '\n';
        out << line.str();
    }
}

}  // namespace pricer
