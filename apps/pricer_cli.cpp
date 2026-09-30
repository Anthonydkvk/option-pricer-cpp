// Démo en ligne de commande : prix, grecques et temps de calcul des 3 méthodes.
//   ./build/apps/pricer_cli                       cas standard, tableau
//   ./build/apps/pricer_cli --type put --K 110    autre option
//   ./build/apps/pricer_cli --csv                 sortie CSV (lue par python/compare.py)
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cli.hpp"

int main(int argc, char* argv[]) {
    // argv[0] est le nom du programme : on ne garde que les arguments.
    const std::vector<std::string> args(argv + 1, argv + argc);
    try {
        const pricer::CliConfig config = pricer::parse_args(args);
        if (config.help) {
            std::cout << pricer::usage();
            return 0;
        }
        const auto results = pricer::run_all(config);
        if (config.csv) {
            pricer::print_csv(std::cout, results);
        } else {
            pricer::print_table(std::cout, config, results);
        }
    } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << "\n\n" << pricer::usage();
        return 1;
    }
    return 0;
}
